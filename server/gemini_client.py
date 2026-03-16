import json
import os

import google.generativeai as genai
from dotenv import load_dotenv

# .env dosyasından API anahtarını yükle
load_dotenv(os.path.join(os.path.dirname(__file__), ".env"))
genai.configure(api_key=os.getenv("GEMINI_API_KEY"))

# -----------------------------------------------------------------
# Gemini'ye verdiğimiz rol tanımı (System Prompt)
#
# Gemini'ye "Sen bir pazarlama uzmanısın" diyoruz ve kuralları
# belirliyoruz: hangi animasyonlar var, ne döndürmeli, nasıl
# düşünmeli. Bu prompt'u ileride birlikte iyileştireceğiz.
# -----------------------------------------------------------------
SYSTEM_PROMPT = """You are a Marketing Display Expert AI that optimizes physical storefront LED displays through A/B testing.

You control an 8-LED NeoPixel ring. Your goal is to MAXIMIZE dwell time (how long people stop and look).

Available animations: "solid", "breathing", "rainbow_cycle", "blink", "wave", "color_wipe"

Parameters you control:
- "animation": one of the above
- "r", "g", "b": RGB color (0-255). Ignored for rainbow_cycle.
- "speed": 1-100 (higher = faster)

Strategy:
- If fewer than 5 trials, explore diverse options.
- After 5+ trials, favor strategies with high dwell times, with small variations.
- Consider noise_level: noisy environments may need bolder animations.

Respond with ONLY a valid JSON object:
{"animation": "type", "r": 0, "g": 0, "b": 0, "speed": 50}"""


def get_next_strategy(trials):
    """
    Trial geçmişini Gemini'ye gönder, yeni strateji al.

    Args:
        trials: trial kayıtlarının listesi (trials.json'daki format)

    Returns:
        dict: {"animation": "...", "r": N, "g": N, "b": N, "speed": N}
    """
    model = genai.GenerativeModel(
        model_name="gemini-3.1-flash-lite-preview",
        system_instruction=SYSTEM_PROMPT,
    )

    # Kullanıcı mesajını hazırla
    if len(trials) == 0:
        user_msg = "No trials yet. Suggest an initial strategy."
    else:
        # Tüm trial geçmişini gönder (Gemini'nin geniş context penceresi bunu destekler)
        lines = ["Trial history:"]
        for t in trials:
            lines.append(
                f"  #{t['trial_number']}: "
                f"anim={t['strategy']['animation']}, "
                f"color=({t['strategy']['r']},{t['strategy']['g']},{t['strategy']['b']}), "
                f"speed={t['strategy']['speed']} "
                f"-> dwell={t['dwell_time_ms']}ms, noise={t['noise_level']:.1f}"
            )

        # En iyi ve en kötü trial'ı vurgula
        best = max(trials, key=lambda t: t["dwell_time_ms"])
        worst = min(trials, key=lambda t: t["dwell_time_ms"])
        lines.append(f"\nBest: #{best['trial_number']} ({best['dwell_time_ms']}ms)")
        lines.append(f"Worst: #{worst['trial_number']} ({worst['dwell_time_ms']}ms)")
        lines.append(f"Total trials: {len(trials)}")
        lines.append("\nWhat strategy next? JSON only.")

        user_msg = "\n".join(lines)

    # Gemini'ye sor
    response = model.generate_content(user_msg)
    text = response.text.strip()

    # Gemini bazen ```json ... ``` ile sarabilir, temizle
    if text.startswith("```"):
        text = text.split("\n", 1)[1]
        text = text.rsplit("```", 1)[0].strip()

    strategy = json.loads(text)

    # Değerleri doğrula ve sınırla
    valid_animations = ["solid", "breathing", "rainbow_cycle", "blink", "wave", "color_wipe"]
    if strategy.get("animation") not in valid_animations:
        strategy["animation"] = "breathing"

    for c in ["r", "g", "b"]:
        strategy[c] = max(0, min(255, int(strategy.get(c, 128))))

    strategy["speed"] = max(1, min(100, int(strategy.get("speed", 50))))

    return strategy
