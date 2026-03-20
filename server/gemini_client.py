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
# düşünmeli. Yoğunluk verisini ve karar açıklamasını da istiyoruz.
# -----------------------------------------------------------------
SYSTEM_PROMPT = """You are a Marketing Display Expert AI that optimizes physical storefront LED displays through A/B testing.

You control an 8-LED NeoPixel ring. Your goal is to MAXIMIZE dwell time (how long people stop and look).

Available animations: "solid", "breathing", "rainbow_cycle", "blink", "wave", "color_wipe"

Parameters you control:
- "animation": one of the above
- "r", "g", "b": RGB color (0-255). Ignored for rainbow_cycle.
- "speed": 1-100 (higher = faster)

Context you receive:
- dwell_time_ms: how long each person stopped
- noise_level: ambient noise 0-100 (higher = noisier)
- density_score: 0.0-1.0 foot traffic density in last 2 minutes
- density_category: "low", "medium", or "high"
- person_count_2min: number of people in last 2 minutes

Strategy guidelines:
- If fewer than 5 trials, explore widely different colors and animations each time.
- After 5+ trials, use top-performing strategies as a base but ALWAYS change the color noticeably.
  A "small variation" means keeping the same animation type but shifting to a clearly different color
  (e.g. blue→orange, green→purple, red→cyan). Never repeat the exact same RGB values back to back.
- Color is your most visible variable — the audience can instantly see color changes, so make them count.
  Use the full RGB spectrum: warm tones (red, orange, yellow), cool tones (blue, cyan, purple),
  and vivid combinations. Avoid staying in the same color family for more than 2 consecutive trials.
- Consider noise_level: noisy environments may need bolder, more vivid animations.
- Consider density: high density = many people passing, use attention-grabbing animations.
  Low density = few people, use calmer/inviting animations to draw people in.
- "reason" field (2-3 sentences, English): Explain your reasoning step by step:
  1. What did you observe in the data? (e.g. "Dwell time dropped 30% since last trial")
  2. What factor influenced your decision most? (noise, density, past performance)
  3. What do you expect this new strategy to achieve?
  Example: "Dwell time has been declining with static animations. The high noise level (72) suggests a busy environment. Switching to rainbow_cycle at high speed to compete for attention — expect a 20%+ dwell increase."

Respond with ONLY a valid JSON object:
{"animation": "type", "r": 0, "g": 0, "b": 0, "speed": 50, "reason": "Your step-by-step reasoning here"}"""


def get_next_strategy(trials):
    """
    Trial geçmişini Gemini'ye gönder, yeni strateji al.

    Args:
        trials: trial kayıtlarının listesi (trials.json'daki format)

    Returns:
        dict: {"strategy": {...}, "reason": "..."}
    """
    model = genai.GenerativeModel(
        model_name="gemini-3.1-flash-lite-preview",
        system_instruction=SYSTEM_PROMPT,
    )

    # Kullanıcı mesajını hazırla
    if len(trials) == 0:
        user_msg = "No trials yet. Suggest an initial strategy."
    else:
        # Tüm trial geçmişini gönder
        lines = ["Trial history:"]
        for t in trials:
            density_info = ""
            if "density_score" in t:
                density_info = (f", density={t['density_score']:.2f}"
                                f"({t.get('density_category', '?')})")
            lines.append(
                f"  #{t['trial_number']}: "
                f"anim={t['strategy']['animation']}, "
                f"color=({t['strategy']['r']},{t['strategy']['g']},{t['strategy']['b']}), "
                f"speed={t['strategy']['speed']} "
                f"-> dwell={t['dwell_time_ms']}ms, noise={t['noise_level']:.1f}"
                f"{density_info}"
            )

        # En iyi ve en kötü trial'ı vurgula
        best = max(trials, key=lambda t: t["dwell_time_ms"])
        worst = min(trials, key=lambda t: t["dwell_time_ms"])
        lines.append(f"\nBest: #{best['trial_number']} ({best['dwell_time_ms']}ms)")
        lines.append(f"Worst: #{worst['trial_number']} ({worst['dwell_time_ms']}ms)")
        lines.append(f"Total trials: {len(trials)}")

        # Son trial'ın yoğunluk bilgisi
        latest = trials[-1]
        lines.append(f"\nCurrent density: {latest.get('density_score', 0):.2f} ({latest.get('density_category', 'unknown')})")
        lines.append(f"People in last 2min: {latest.get('person_count_2min', 0)}")

        lines.append("\nWhat strategy next? JSON only.")
        user_msg = "\n".join(lines)

    # Gemini'ye sor
    response = model.generate_content(user_msg)
    text = response.text.strip()

    # Gemini bazen ```json ... ``` ile sarabilir, temizle
    if text.startswith("```"):
        text = text.split("\n", 1)[1]
        text = text.rsplit("```", 1)[0].strip()

    raw = json.loads(text)

    # Değerleri doğrula ve sınırla
    valid_animations = ["solid", "breathing", "rainbow_cycle", "blink", "wave", "color_wipe"]
    if raw.get("animation") not in valid_animations:
        raw["animation"] = "breathing"

    for c in ["r", "g", "b"]:
        raw[c] = max(0, min(255, int(raw.get(c, 128))))

    raw["speed"] = max(1, min(100, int(raw.get("speed", 50))))

    reason = raw.pop("reason", "No explanation provided")

    return {
        "strategy": raw,
        "reason": reason,
    }
