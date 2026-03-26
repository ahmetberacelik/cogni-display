# CogniDisplay

AI destekli fiziksel ekran A/B test sistemi. ESP32-S3 ile kisi bekleme suresi ve ortam gurultusu olculur, veriler yerel Flask sunucusuna gonderilir, Google Gemini AI bir sonraki LED animasyon stratejisini belirler.

**Dongu:** Gozlem → Analiz → Aksiyon

## Sistem Mimarisi

```
┌─────────────┐         HTTP POST         ┌──────────────┐       API        ┌────────────┐
│   ESP32-S3  │ ──────────────────────►   │ Flask Server │ ──────────────► │  Gemini AI │
│             │ ◄──────────────────────   │  (Python)    │ ◄────────────── │            │
│  Sensorler  │      Yeni Strateji        │              │    Strateji     │  Karar     │
│  + LED Ring │                           │  Dashboard   │                 │  Motoru    │
└─────────────┘                           └──────────────┘                 └────────────┘
```

## Donanim Gereksinimleri

| Bilesen | Aciklama | Baglanti |
|---------|----------|----------|
| **ESP32-S3 DevKit** | Ana mikrodenetleyici | USB-C ile bilgisayara |
| **HC-SR04** | Ultrasonik mesafe sensoru (5V) | GPIO4 (Trig), GPIO5 (Echo) |
| **MAX4466** | Elektret mikrofon modulu | GPIO6 (ADC1_CH5) |
| **WS2812B** | NeoPixel LED Ring (16 LED) | GPIO7 |
| **Seviye Donusturucu** | 4 kanalli, cift yonlu (3.3V ↔ 5V) | HC-SR04 Echo + NeoPixel DIN |

### Pin Atama Tablosu

```
GPIO4  →  Seviye Donusturucu CH1  →  HC-SR04 Trig
GPIO5  →  Seviye Donusturucu CH2  →  HC-SR04 Echo
GPIO6  →  MAX4466 OUT (dogrudan, 3.3V uyumlu)
GPIO7  →  Seviye Donusturucu CH3  →  330Ω  →  WS2812B DIN
```

> **Not:** GPIO4-7 secimi, strapping/USB/JTAG/flash pinlerinden kacinmak icindir. GPIO6 ozellikle ADC1 icin secilmistir (ADC2, Wi-Fi ile cakisir).

## Yazilim Gereksinimleri

### ESP32 Firmware
- **ESP-IDF v5.5.3**
- `espressif/led_strip` komponenti (v3.0+, ilk derlemede otomatik indirilir)

### Python Sunucu
- Python 3.10+
- Bagimliliklar: Flask, google-generativeai, python-dotenv, pyserial

## Kurulum

### 1. Repoyu Klonla

```bash
git clone https://github.com/<kullanici>/cogni-display.git
cd cogni-display
```

### 2. ESP32 Firmware Yapilandirmasi

`main/header/config.h` dosyasinda Wi-Fi ve sunucu ayarlarini guncelle:

```c
#define WIFI_SSID    "WiFi_Adi"
#define WIFI_PASS    "WiFi_Sifresi"
#define SERVER_URL   "http://<BILGISAYAR_IP>:5000/api/trial"
```

### 3. Firmware Derleme ve Yukleme

```bash
idf.py build
idf.py flash -p COM6
```

### 4. Python Sunucu Kurulumu

```bash
cd server
pip install -r requirements.txt
```

`server/.env` dosyasi olustur:

```
GEMINI_API_KEY=buraya_api_anahtarin
```

Sunucuyu baslat:

```bash
python app.py
```

Dashboard: `http://localhost:5000/`

> **Onemli:** Flask sunucusu COM6 seri portunu kullanir. Sunucu calisirken `idf.py monitor` calistirma — ayni anda iki program ayni portu kullanamaz. Seri ciktiyi dashboard uzerinden gorebilirsin.

## Nasil Calisir?

### Durum Makinesi (ESP32)

```
         Mesafe < Esik               Bekleme suresi bitti
  IDLE ──────────────► PERSON_DETECTED ──────────────────► PERSON_LEFT
   ▲                                                           │
   │                        Veri sunucuya gonderildi           │
   └───────────────────────────────────────────────────────────┘
```

1. **IDLE:** 100ms aralikla mesafe olcer. 2 dakika kimse gelmezse kirmizi LED yanar.
2. **PERSON_DETECTED:** Kisi algilandi, bekleme suresi olculuyor, gurultu seviyesi kaydediliyor.
3. **PERSON_LEFT:** Kisi ayrildi. Sensor verileri (bekleme suresi, gurultu, yogunluk) sunucuya POST edilir.
4. Sunucu Gemini AI'dan yeni strateji alir ve ESP32'ye doner.
5. ESP32 yeni LED animasyonunu uygular.

### Animasyon Turleri

| Animasyon | Aciklama |
|-----------|----------|
| `rainbow` | Goz alici gokkusagi donguleri |
| `breathing` | Nefes alan yumusak parlama |
| `color_wipe` | Renk silme efekti |
| `theater_chase` | Tiyatro isigi kovalaması |
| `wave` | Dalga seklinde renk gecisi |
| `sparkle` | Rastgele parildama efekti |

### Yogunluk Takibi

2 dakikalik kayar pencere uzerinden hesaplanan bileşik skor:

- Kisi sayisi: %40
- Ortalama bekleme suresi: %30
- Ortalama gurultu: %30

Kategoriler: `low` / `medium` / `high`

## API Referansi

| Endpoint | Metod | Aciklama |
|----------|-------|----------|
| `/api/trial` | POST | ESP32 sensor verisi gonderir, yeni strateji alir |
| `/api/status` | GET | Deneme gecmisi, istatistikler, guncel strateji |
| `/api/serial` | GET | Filtrelenmis ESP32 seri loglari + canli durum |
| `/api/serial/raw` | GET | Tum seri satirlari (mesafe olcumleri dahil) |
| `/api/logs` | GET | Yapilandirilmis sunucu olay logu |

### Ornek Veri Akisi

**ESP32 → Sunucu:**
```json
{
  "dwell_time_ms": 4500,
  "noise_level": 42.3,
  "current_strategy": {"animation": "breathing", "r": 0, "g": 0, "b": 255, "speed": 50},
  "density_score": 0.45,
  "density_category": "medium",
  "person_count_2min": 3
}
```

**Sunucu → ESP32:**
```json
{
  "animation": "wave",
  "r": 255,
  "g": 100,
  "b": 0,
  "speed": 75,
  "reason": "Yuksek yogunluk ve orta gurultu seviyesi..."
}
```

## Proje Dosya Yapisi

```
cogni-display/
├── main/
│   ├── CMakeLists.txt              # Derleme dosyasi
│   ├── idf_component.yml           # led_strip bagimliligi
│   ├── header/
│   │   ├── config.h                # Merkezi konfigürasyon
│   │   ├── wifi_manager.h
│   │   ├── ultrasonic.h
│   │   ├── mic_adc.h
│   │   ├── led_animation.h
│   │   └── http_client.h
│   └── src/
│       ├── main.c                  # Durum makinesi + kalibrasyon
│       ├── wifi_manager.c          # Wi-Fi baglanti yonetimi
│       ├── ultrasonic.c            # HC-SR04 surucu
│       ├── mic_adc.c               # MAX4466 ADC okuma
│       ├── led_animation.c         # WS2812B animasyonlari
│       └── http_client.c           # HTTP istemci
├── server/
│   ├── app.py                      # Flask sunucu
│   ├── gemini_client.py            # Gemini AI entegrasyonu
│   ├── serial_reader.py            # Seri port okuyucu
│   ├── requirements.txt            # Python bagimliliklari
│   ├── templates/
│   │   └── dashboard.html          # Web arayuzu
│   └── .env                        # API anahtari
├── docs/                           # Proje dokumanlari
├── CLAUDE.md
└── README.md
```

## Donanim Notlari

- ESP32 pinleri ucuz breadboard'larda guvenilir temas saglamayabilir. Pinler yanlis okuyor ise ESP32'yi breadboard disinda tutup F-M jumper kablo kullan.
- MAX4466 uzerindeki trim pot kazanci ayarlar (25x-125x). Normal konusma icin hedef gurultu seviyesi: 20-40.
- HC-SR04'un GND kablosu GND'ye baglanmali, 5V'ye degil.
- Seviye donusturucu hem HC-SR04 (5V echo → 3.3V) hem NeoPixel (3.3V data → 5V) icin gereklidir.

## Tasarim Kararlari

- **Uyarlanabilir baseline:** Baslangicta 4 saniye kalibrasyon, dinamik esikler (baseline x 0.70 algilama, baseline x 0.85 ayrilma), 60 saniye stabil okumadan sonra otomatik guncelleme.
- **Bosta modu:** 2 dakika kimse algilanmazsa sabit kirmizi LED yanar (sunucuya istek gitmez).
- **Graceful degradation:** Wi-Fi veya sunucu erisim hatasi durumunda mevcut animasyona devam edilir, hata loglanir, sistem cokmez.
- **Seri log filtreleme:** Dashboard'da sadece anlamli olaylar gosterilir (kisi algilama, bekleme, yogunluk, hata). Ham loglar ayri endpoint'ten erisile bilir.

## Teknolojiler

- **Firmware:** C, ESP-IDF 5.5.3, FreeRTOS
- **Sunucu:** Python, Flask, Google Gemini AI
- **Arayuz:** HTML/CSS/JS, Chart.js
- **Iletisim:** HTTP/JSON, UART (seri port)

## Lisans

Bu proje egitim amacli gelistirilmistir.
