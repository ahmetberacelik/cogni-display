import json
import os
import random

from flask import Flask, request, jsonify
from gemini_client import get_next_strategy

app = Flask(__name__)

# Trial geçmişinin saklandığı dosya
TRIALS_FILE = os.path.join(os.path.dirname(__file__), "trials.json")


def load_trials():
    """trials.json dosyasından geçmişi oku."""
    if os.path.exists(TRIALS_FILE):
        with open(TRIALS_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    return []


def save_trials(trials):
    """trials.json dosyasına geçmişi yaz."""
    with open(TRIALS_FILE, "w", encoding="utf-8") as f:
        json.dump(trials, f, indent=2, ensure_ascii=False)


def generate_fallback_strategy():
    """Gemini çalışmazsa rastgele bir strateji üret (yedek plan)."""
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
# ESP32 buraya sensör verisini gönderir. Sunucu:
#   1. Veriyi trials.json'a kaydeder
#   2. Gemini'ye sorar
#   3. Yeni stratejiyi ESP32'ye döner
# -----------------------------------------------------------------
@app.route("/api/trial", methods=["POST"])
def receive_trial():
    data = request.get_json()
    if not data:
        return jsonify({"error": "JSON body bos"}), 400

    # Gerekli alanları kontrol et
    required = ["dwell_time_ms", "noise_level", "current_strategy"]
    for field in required:
        if field not in data:
            return jsonify({"error": f"Eksik alan: {field}"}), 400

    # Geçmişi yükle ve yeni trial'ı ekle
    trials = load_trials()
    trial_entry = {
        "trial_number": len(trials) + 1,
        "dwell_time_ms": data["dwell_time_ms"],
        "noise_level": data["noise_level"],
        "strategy": data["current_strategy"],
    }
    trials.append(trial_entry)
    save_trials(trials)

    app.logger.info(
        "Trial #%d kaydedildi: dwell=%dms, noise=%.1f, anim=%s",
        trial_entry["trial_number"],
        trial_entry["dwell_time_ms"],
        trial_entry["noise_level"],
        trial_entry["strategy"].get("animation", "?"),
    )

    # Gemini'ye sor, hata olursa fallback kullan
    try:
        new_strategy = get_next_strategy(trials)
        app.logger.info("Gemini stratejisi: %s", new_strategy)
    except Exception as e:
        app.logger.error("Gemini hatasi: %s — fallback kullaniliyor", e)
        new_strategy = generate_fallback_strategy()

    return jsonify(new_strategy)


# -----------------------------------------------------------------
# GET /api/status
#
# Tarayıcıdan açarak sistemi izleyebilirsin:
#   - Toplam kaç trial yapılmış
#   - Son 10 trial'ın detayları
#   - Şu anki aktif strateji
# -----------------------------------------------------------------
@app.route("/api/status", methods=["GET"])
def status():
    trials = load_trials()
    return jsonify({
        "total_trials": len(trials),
        "trials": trials[-10:],
        "current_strategy": trials[-1]["strategy"] if trials else None,
    })


if __name__ == "__main__":
    # host="0.0.0.0" -> aynı ağdaki tüm cihazlar erişebilir (ESP32 dahil)
    # port=5000 -> sunucu bu portta dinler
    app.run(host="0.0.0.0", port=5000, debug=True)
