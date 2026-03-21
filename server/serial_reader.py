"""
ESP32 serial port reader — background thread that reads ESP-IDF logs,
parses them, and maintains live ESP32 state for the dashboard.
"""

import os
import re
import threading
import time
from datetime import datetime

import serial


class SerialReader:
    def __init__(self, port="COM6", baudrate=115200, on_event=None):
        self.port = port
        self.baudrate = baudrate
        self.on_event = on_event  # callback(event_type, message) for server log integration

        self._logs = []          # Filtered: only meaningful events
        self._max_logs = 200
        self._raw_logs = []      # All lines including distance readings
        self._max_raw_logs = 500
        self._lock = threading.Lock()

        # Raw log file — new session overwrites
        self._log_file_path = os.path.join(os.path.dirname(__file__), "esp_serial.log")
        self._log_file = None
        self._running = False
        self._thread = None
        self._serial = None

        # Live ESP32 state
        self.esp_state = {
            "connected": False,
            "state": "UNKNOWN",
            "distance": None,
            "baseline": None,
            "wifi_connected": False,
            "calibrated": False,
            "idle_mode": False,
            "last_dwell": None,
            "last_noise": None,
        }

        # Compiled regex patterns for ESP-IDF log format:
        # I (12345) TAG: message
        self._re_log = re.compile(r'^([IWE]) \((\d+)\) (\w+): (.*)$')

        # Main loop: [STATE] Distance: X cm (baseline: Y)
        # Firmware prints IDLE, DETECTED, LEFT (not PERSON_DETECTED etc.)
        self._re_state = re.compile(
            r'\[(IDLE|DETECTED|LEFT)\] Distance: ([\d.]+) cm \(baseline: ([\d.]+)\)'
        )
        # Person detected
        self._re_detect = re.compile(r'Person detected! Distance: ([\d.]+) cm')
        # Dwell time + noise
        self._re_dwell = re.compile(r'Dwell time: (\d+) ms, Average noise: ([\d.]+)')
        # Density
        self._re_density = re.compile(
            r'Density: ([\d.]+) \((\w+)\), Last 2min people: (\d+)'
        )
        # Calibration done
        self._re_calib = re.compile(r'Calibration done! Baseline: ([\d.]+) cm')
        # Baseline update
        self._re_baseline = re.compile(r'Baseline updated: ([\d.]+) cm')

    def start(self):
        if self._running:
            return
        self._running = True
        # Open raw log file (new session = overwrite)
        self._log_file = open(self._log_file_path, "w", encoding="utf-8", buffering=1)
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join(timeout=3)
        if self._serial and self._serial.is_open:
            self._serial.close()
        if self._log_file:
            self._log_file.close()

    def get_logs(self, n=50):
        """Return last n filtered (meaningful) log entries."""
        with self._lock:
            return list(self._logs[-n:])

    def get_raw_logs(self, n=200):
        """Return last n raw log entries (all lines including distance)."""
        with self._lock:
            return list(self._raw_logs[-n:])

    def get_esp_state(self):
        with self._lock:
            return dict(self.esp_state)

    def _read_loop(self):
        """Main loop: connect to serial, read lines, parse."""
        while self._running:
            try:
                self._serial = serial.Serial(
                    self.port, self.baudrate, timeout=1
                )
                with self._lock:
                    self.esp_state["connected"] = True
                self._emit("esp_connected", f"Serial connected to {self.port}")

                while self._running and self._serial.is_open:
                    try:
                        raw = self._serial.readline()
                        if not raw:
                            continue
                        line = raw.decode("utf-8", errors="replace").strip()
                        if line:
                            self._process_line(line)
                    except serial.SerialException:
                        break

            except serial.SerialException as e:
                with self._lock:
                    self.esp_state["connected"] = False
                self._emit("esp_disconnected", f"Serial error: {e}")

            # Wait before retry
            if self._running:
                time.sleep(3)

    def _process_line(self, line):
        """Parse one line of ESP-IDF log output."""
        m = self._re_log.match(line)
        if m:
            level, boot_ms, tag, message = m.groups()
        else:
            # Non-standard line (boot messages, panics, etc.)
            level, boot_ms, tag, message = "I", "0", "SYS", line

        now = datetime.now().isoformat(timespec="seconds")
        entry = {
            "timestamp": now,
            "level": level,
            "tag": tag,
            "message": message,
            "raw": line,
        }

        # Write to raw log file
        if self._log_file:
            self._log_file.write(f"{now} | {level} | {tag} | {message}\n")

        # Check if this is the high-frequency distance line (every 100ms)
        is_distance_spam = bool(self._re_state.search(message))

        with self._lock:
            # Raw buffer: ALL lines
            self._raw_logs.append(entry)
            if len(self._raw_logs) > self._max_raw_logs:
                self._raw_logs.pop(0)

            # Filtered buffer: skip repetitive distance readings
            if not is_distance_spam:
                self._logs.append(entry)
                if len(self._logs) > self._max_logs:
                    self._logs.pop(0)

        # Update esp_state based on message content
        self._update_state(tag, message)

    def _update_state(self, tag, msg):
        """Extract live state from parsed log messages."""

        # Main loop state + distance + baseline (every 100ms)
        m = self._re_state.search(msg)
        if m:
            raw_state = m.group(1)  # "IDLE", "DETECTED", or "LEFT"
            # Map firmware short names to readable names
            state_map = {"IDLE": "IDLE", "DETECTED": "PERSON_DETECTED", "LEFT": "PERSON_LEFT"}
            mapped = state_map.get(raw_state, raw_state)
            with self._lock:
                self.esp_state["distance"] = float(m.group(2))
                self.esp_state["baseline"] = float(m.group(3))
                # Don't overwrite IDLE_MODE with regular IDLE updates.
                # idle_mode is only cleared by "Idle mode off" message.
                if not (self.esp_state["idle_mode"] and raw_state == "IDLE"):
                    self.esp_state["state"] = mapped
            return

        # Idle mode ON
        if "Idle mode: solid white" in msg:
            with self._lock:
                self.esp_state["idle_mode"] = True
                self.esp_state["state"] = "IDLE_MODE"
            self._emit("esp_idle_on", "ESP32 entered idle mode (solid white LED)")
            return

        # Idle mode OFF
        if "Idle mode off" in msg:
            with self._lock:
                self.esp_state["idle_mode"] = False
                self.esp_state["state"] = "IDLE"
            self._emit("esp_idle_off", "ESP32 exited idle mode")
            return

        # Person detected
        m = self._re_detect.search(msg)
        if m:
            with self._lock:
                self.esp_state["state"] = "PERSON_DETECTED"
                self.esp_state["distance"] = float(m.group(1))
                self.esp_state["idle_mode"] = False
            self._emit("esp_person_detected", f"Person detected at {m.group(1)} cm")
            return

        # Dwell + noise
        m = self._re_dwell.search(msg)
        if m:
            with self._lock:
                self.esp_state["last_dwell"] = int(m.group(1))
                self.esp_state["last_noise"] = float(m.group(2))
            return

        # Density
        m = self._re_density.search(msg)
        if m:
            # density data is also sent via HTTP, so no special state update needed
            return

        # Calibration done
        m = self._re_calib.search(msg)
        if m:
            with self._lock:
                self.esp_state["calibrated"] = True
                self.esp_state["baseline"] = float(m.group(1))
            self._emit("esp_calibrated", f"Calibration done, baseline: {m.group(1)} cm")
            return

        # Baseline update
        m = self._re_baseline.search(msg)
        if m:
            with self._lock:
                self.esp_state["baseline"] = float(m.group(1))
            return

        # Wi-Fi connected
        if "Wi-Fi connected." in msg or "Connection successful" in msg:
            with self._lock:
                self.esp_state["wifi_connected"] = True
            return

        # Wi-Fi disconnected
        if "Connection lost" in msg or "Wi-Fi connection failed" in msg:
            with self._lock:
                self.esp_state["wifi_connected"] = False
            return

    def _emit(self, event_type, message):
        """Forward important events to the server's log system."""
        if self.on_event:
            self.on_event(event_type, message)
