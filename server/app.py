import json
import os
import random
from datetime import datetime

from flask import Flask, request, jsonify, render_template
from gemini_client import get_next_strategy
from serial_reader import SerialReader

app = Flask(__name__)

# --------------- ESP32 Serial Reader ---------------
serial_reader = SerialReader(port="COM6", baudrate=115200)

# --------------- Structured Logging (in-memory ring buffer) ---------------
event_log = []
MAX_LOG_ENTRIES = 100


def log_event(event_type, message, data=None):
    """Add a structured log entry to the in-memory buffer."""
    entry = {
        "timestamp": datetime.now().isoformat(timespec="seconds"),
        "type": event_type,
        "message": message,
        "data": data or {},
    }
    event_log.append(entry)
    if len(event_log) > MAX_LOG_ENTRIES:
        event_log.pop(0)
    app.logger.info("[%s] %s", event_type, message)

# File where trial history is stored
TRIALS_FILE = os.path.join(os.path.dirname(__file__), "trials.json")


def load_trials():
    """Load trial history from trials.json."""
    if os.path.exists(TRIALS_FILE):
        with open(TRIALS_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    return []


def save_trials(trials):
    """Save trial history to trials.json."""
    with open(TRIALS_FILE, "w", encoding="utf-8") as f:
        json.dump(trials, f, indent=2, ensure_ascii=False)


def generate_fallback_strategy():
    """Generate a random fallback strategy if Gemini fails."""
    animations = ["solid", "breathing", "rainbow_cycle", "blink", "wave", "color_wipe"]
    return {
        "animation": random.choice(animations),
        "r": random.randint(0, 255),
        "g": random.randint(0, 255),
        "b": random.randint(0, 255),
        "speed": random.randint(20, 80),
    }


# -----------------------------------------------------------------
# POST /api/trial
#
# ESP32 sends sensor data here. The server:
#   1. Saves the data to trials.json (including density info)
#   2. Queries Gemini
#   3. Returns the new strategy + reasoning to ESP32
# -----------------------------------------------------------------
@app.route("/api/trial", methods=["POST"])
def receive_trial():
    data = request.get_json()
    if not data:
        return jsonify({"error": "JSON body is empty"}), 400

    # Check required fields
    required = ["dwell_time_ms", "noise_level", "current_strategy"]
    for field in required:
        if field not in data:
            return jsonify({"error": f"Missing field: {field}"}), 400

    # Load history and add new trial
    trials = load_trials()
    trial_entry = {
        "trial_number": len(trials) + 1,
        "timestamp": datetime.now().isoformat(timespec="seconds"),
        "dwell_time_ms": data["dwell_time_ms"],
        "noise_level": data["noise_level"],
        "strategy": data["current_strategy"],
        "density_score": data.get("density_score", 0),
        "density_category": data.get("density_category", "low"),
        "person_count_2min": data.get("person_count_2min", 0),
    }
    trials.append(trial_entry)

    log_event("trial_received",
              f"Trial #{trial_entry['trial_number']}: "
              f"dwell={trial_entry['dwell_time_ms']}ms, "
              f"noise={trial_entry['noise_level']:.1f}, "
              f"density={trial_entry['density_category']}",
              {"trial_number": trial_entry["trial_number"],
               "dwell_time_ms": trial_entry["dwell_time_ms"],
               "noise_level": trial_entry["noise_level"]})

    # Query Gemini, use fallback if it fails
    log_event("gemini_request", f"Asking Gemini for strategy (trial history: {len(trials)})")
    try:
        result = get_next_strategy(trials)
        new_strategy = result["strategy"]
        reason = result.get("reason", "No explanation provided")
        log_event("gemini_response",
                  f"Strategy: {new_strategy.get('animation', '?')} — {reason}",
                  {"strategy": new_strategy})
    except Exception as e:
        app.logger.error("Gemini error: %s — using fallback", e)
        log_event("gemini_error", f"Gemini failed: {e} — using fallback")
        new_strategy = generate_fallback_strategy()
        reason = "Fallback: Gemini unavailable"

    # Add new strategy and reasoning to trial record and save
    trials[-1]["ai_reason"] = reason
    trials[-1]["new_strategy"] = new_strategy
    save_trials(trials)

    # Return strategy + reason to ESP32
    response = dict(new_strategy)
    response["reason"] = reason
    return jsonify(response)


# -----------------------------------------------------------------
# GET /api/status
#
# Open in a browser to monitor the system:
#   - Total number of trials
#   - Details of the last 10 trials (density + AI reasoning included)
#   - Current active strategy
#   - Last AI reasoning
#   - Last density info
# -----------------------------------------------------------------
@app.route("/api/status", methods=["GET"])
def status():
    trials = load_trials()
    last_trial = trials[-1] if trials else None

    # Compute stats for the dashboard
    stats = None
    if trials:
        dwell_times = [t["dwell_time_ms"] for t in trials]
        best_trial = max(trials, key=lambda t: t["dwell_time_ms"])
        stats = {
            "avg_dwell": round(sum(dwell_times) / len(dwell_times)),
            "max_dwell": max(dwell_times),
            "min_dwell": min(dwell_times),
            "best_trial": best_trial["trial_number"],
            "best_animation": best_trial["strategy"].get("animation", "?"),
        }

    return jsonify({
        "total_trials": len(trials),
        "trials": trials[-50:],
        "current_strategy": last_trial.get("new_strategy", last_trial["strategy"]) if last_trial else None,
        "last_reason": last_trial.get("ai_reason", "N/A") if last_trial else None,
        "last_density": {
            "score": last_trial.get("density_score", 0),
            "category": last_trial.get("density_category", "unknown"),
            "person_count_2min": last_trial.get("person_count_2min", 0),
        } if last_trial else None,
        "stats": stats,
    })


# -----------------------------------------------------------------
# GET /  — Web dashboard
# GET /api/logs — Structured log entries
# -----------------------------------------------------------------
@app.route("/")
def dashboard():
    return render_template("dashboard.html")


@app.route("/api/logs")
def get_logs():
    return jsonify({"logs": event_log[-50:]})


@app.route("/api/serial")
def get_serial():
    return jsonify({
        "logs": serial_reader.get_logs(50),
        "esp_state": serial_reader.get_esp_state(),
    })


@app.route("/api/serial/raw")
def get_serial_raw():
    return jsonify({
        "logs": serial_reader.get_raw_logs(200),
    })


if __name__ == "__main__":
    # Flask debug mode starts the app twice (parent + reloader child).
    # Only open the serial port in the reloader child to avoid PermissionError.
    if os.environ.get("WERKZEUG_RUN_MAIN") == "true":
        serial_reader.on_event = log_event
        serial_reader.start()
        log_event("server_start", "CogniDisplay server started on port 5000")
    # host="0.0.0.0" -> accessible from all devices on the same network (including ESP32)
    # port=5000 -> server listens on this port
    app.run(host="0.0.0.0", port=5000, debug=True)
