# CogniDisplay — Geliştirme ve Mimari Raporu

> Bu doküman, CogniDisplay projesinin tüm teknik mimarisini, tasarım kararlarını, modül yapılarını ve uygulama planını kapsamaktadır. Kod geliştirme sürecinde referans kılavuz olarak kullanılacaktır.

---

## İçindekiler

1. [Proje Özeti](#1-proje-özeti)
2. [Sistem Mimarisi](#2-sistem-mimarisi)
3. [Donanım Bağlantıları](#3-donanım-bağlantıları)
4. [ESP32 Firmware Mimarisi](#4-esp32-firmware-mimarisi)
5. [Python Server Mimarisi](#5-python-server-mimarisi)
6. [Veri Akışı ve JSON Şemaları](#6-veri-akışı-ve-json-şemaları)
7. [LED Animasyon Türleri](#7-led-animasyon-türleri)
8. [Hata Yönetimi Stratejisi](#8-hata-yönetimi-stratejisi)
9. [Uygulama Planı](#9-uygulama-planı)
10. [Test ve Doğrulama](#10-test-ve-doğrulama)

---

## 1. Proje Özeti

**CogniDisplay**, yazılım dünyasındaki A/B testi mantığını fiziksel dünyaya taşıyan otonom bir "Fiziksel Etkileşim Ajanı"dır.

Sistem bir vitrin (veya stant) önünden geçen insanların **etkileşim süresini (dwell time)** ölçer, ortam gürültü seviyesini algılar ve bu verileri bir Büyük Dil Modeli'ne (LLM — Google Gemini) ileterek **en dikkat çekici LED ışık rengi ve animasyonunu** otonom olarak belirler.

**Temel Döngü:**
1. Kişi vitrinin önüne gelir → sensörler ölçüm yapar
2. Kişi ayrılır → veriler sunucuya gönderilir
3. Gemini AI yeni strateji belirler
4. LED halkası yeni animasyonu uygular
5. Bir sonraki kişide yeni stratejinin performansı ölçülür

**Teknoloji Yığını:**
- **Mikrodenetleyici:** ESP32-S3 (ESP-IDF 5.5.3, C dili)
- **Sensörler:** HC-SR04 (mesafe), MAX4466 (ses)
- **Aktüatör:** WS2812B 8-Bit NeoPixel Ring (RGB LED)
- **İletişim:** Wi-Fi, HTTP, JSON
- **Sunucu:** Python Flask (yerel bilgisayar)
- **Yapay Zeka:** Google Gemini API (gemini-3.1-flash-lite-preview)

---

## 2. Sistem Mimarisi

Proje, modern bir 3 katmanlı IoT mimarisi üzerine inşa edilmiştir:

### 2.1 Cihaz Katmanı (Device Layer)

Fiziksel dünyayı gözlemleyen ve eyleme geçen katmandır.

| Bileşen | Rol | Voltaj |
|---|---|---|
| ESP32-S3 | Ana kontrolcü, Wi-Fi, veri işleme | 3.3V mantık |
| HC-SR04 | Kişi tespiti ve dwell time ölçümü | 5V (level converter gerekli) |
| MAX4466 | Ortam gürültü seviyesi ölçümü | 3.3V (doğrudan bağlanır) |
| WS2812B NeoPixel Ring | AI kararını fiziksel olarak uygulama | 5V (level converter önerilir) |
| Lojik Seviye Dönüştürücü | 3.3V ↔ 5V sinyal köprüsü | Çift yönlü, 4 kanal |
| 330Ω Direnç | WS2812B data hattı parazit koruması | — |
| 1000µF Kapasitör | 5V güç hattı filtreleme | 16V max |

### 2.2 Ağ Katmanı (Network Layer)

ESP32-S3 üzerindeki Wi-Fi modülü ile sağlanır. Cihaz, sensör verilerini **HTTP POST** isteği ile JSON formatında üst katmana iletir. Sunucudan gelen yeni strateji yine JSON olarak alınır.

| Parametre | Değer |
|---|---|
| Protokol | HTTP (plain, TLS yok — yerel ağ) |
| Veri Formatı | JSON |
| Yön | Çift yönlü (POST gönder, response al) |
| Wi-Fi Modu | STA (Station) |
| Wi-Fi Kimlik Bilgileri | Hardcoded (header dosyasında) |

### 2.3 Uygulama Katmanı (Application Layer)

Bilgisayar üzerinde çalışan yerel bir Python Flask sunucusu ve Google Gemini API'sinden oluşur.

| Bileşen | Görev |
|---|---|
| Flask Sunucu | REST API, trial geçmişi yönetimi |
| Gemini API | A/B test verilerini analiz edip yeni strateji belirleyen AI |
| trials.json | Tüm denemelerin saklandığı JSON dosyası |

---

## 3. Donanım Bağlantıları

### 3.1 GPIO Pin Atamaları

| Fonksiyon | GPIO | Yön | ADC Kanalı | Açıklama |
|---|---|---|---|---|
| HC-SR04 Trig | GPIO4 | Digital Output | — | Level converter CH1 üzerinden 5V Trig pinine |
| HC-SR04 Echo | GPIO5 | Digital Input | — | Level converter CH2 üzerinden 5V Echo pininden |
| MAX4466 OUT | GPIO6 | Analog Input | ADC1_CH5 | Doğrudan bağlantı (3.3V uyumlu) |
| WS2812B DIN | GPIO7 | Digital Output (RMT) | — | Level converter CH3 + 330Ω direnç üzerinden |

**Pin Seçim Gerekçeleri:**
- **ADC1 zorunluluğu:** ESP32-S3'te ADC2, Wi-Fi aktifken kullanılamaz. Mikrofon için ADC1 kanallarından biri (GPIO1-10) seçilmelidir. GPIO6 = ADC1_CH5.
- **Kaçınılan pinler:** GPIO0, 3, 45, 46 (strapping), GPIO19-20 (USB), GPIO26-32 (SPI flash/PSRAM), GPIO39-42 (JTAG).
- **Bitişik pin grubu:** GPIO4-7 kullanımı breadboard üzerinde düzenli kablolama sağlar.

### 3.2 Lojik Seviye Dönüştürücü Bağlantıları

| LLC Kanalı | LV Tarafı (3.3V) | HV Tarafı (5V) | Sinyal Yönü | Amaç |
|---|---|---|---|---|
| Referans | ESP32 3.3V → LV pin | 5V supply → HV pin | — | Referans gerilimleri |
| GND | Ortak GND | Ortak GND | — | Toprak hattı |
| CH1 (LV1↔HV1) | GPIO4 | HC-SR04 Trig | 3.3V → 5V (Step-up) | Tetik sinyali gönderme |
| CH2 (LV2↔HV2) | GPIO5 | HC-SR04 Echo | 5V → 3.3V (Step-down) | Yankı sinyali alma |
| CH3 (LV3↔HV3) | GPIO7 | 330Ω → WS2812B DIN | 3.3V → 5V (Step-up) | LED veri sinyali |
| CH4 | Boş | Boş | — | Yedek kanal |

### 3.3 Güç Bağlantıları

| Bileşen | Güç Kaynağı | Açıklama |
|---|---|---|
| ESP32-S3 | USB (5V) | USB üzerinden beslenir, dahili regülatör 3.3V üretir |
| HC-SR04 | ESP32 5V pin (VBUS) | ~15mA çeker |
| MAX4466 | ESP32 3.3V pin | 3.3V ile doğrudan beslenir |
| WS2812B Ring | ESP32 5V pin (VBUS) | Max ~160mA (tam beyaz), tipik 40-80mA |
| Level Converter | LV=3.3V, HV=5V | Her iki referans bağlanır |
| 1000µF Kapasitör | 5V ile GND arası | NeoPixel besleme hattına yakın, güç dalgalanması filtresi |

**Toplam Güç Tüketimi:** ~250-400mA. USB güç kaynağı (500mA+) yeterlidir.

---

## 4. ESP32 Firmware Mimarisi

### 4.1 Dosya Yapısı

```
main/
├── CMakeLists.txt          # Kaynak dosya listesi (güncellenir)
├── idf_component.yml       # led_strip bağımlılığı
├── config.h                # Tüm pin, Wi-Fi, sabit tanımları
├── main.c                  # app_main(), state machine, ana döngü
├── wifi_manager.h          # Wi-Fi modülü arayüzü
├── wifi_manager.c          # Wi-Fi STA bağlantısı, otomatik reconnect
├── ultrasonic.h            # Ultrasonik sensör arayüzü
├── ultrasonic.c            # HC-SR04 sürücü (trig pulse + echo ölçüm)
├── mic_adc.h               # Mikrofon ADC arayüzü
├── mic_adc.c               # MAX4466 okuma, RMS gürültü hesabı
├── led_animation.h         # LED animasyon arayüzü
├── led_animation.c         # 6 animasyon tipi + FreeRTOS task
├── http_client.h           # HTTP istemci arayüzü
└── http_client.c           # JSON POST gönder, cevap parse et
```

**idf_component.yml** içeriği:
```yaml
dependencies:
  espressif/led_strip:
    version: "^3.0.0"
```

**CMakeLists.txt** (main/) güncellenmiş hali:
```cmake
idf_component_register(
    SRCS "main.c"
         "wifi_manager.c"
         "ultrasonic.c"
         "mic_adc.c"
         "led_animation.c"
         "http_client.c"
    INCLUDE_DIRS "."
)
```

### 4.2 FreeRTOS Task Yapısı

| Task | Öncelik | Stack | Çekirdek | Görev |
|---|---|---|---|---|
| Main (app_main) | 5 | 3584 (default) | Herhangi | State machine, sensör okuma, HTTP istekleri |
| LED Animation | 4 | 4096 | Herhangi | Sürekli LED animasyon render (~30 FPS) |
| Wi-Fi (ESP-IDF sistem) | 23 | Sistem default | Core 0 | Wi-Fi stack (sdkconfig ile Core 0'a sabitlenmiş) |

**Senkronizasyon Mekanizmaları:**
- `led_strategy_t` yapısı: **Mutex** (SemaphoreHandle_t) ile korunur. Main task yazar (nadiren), LED task okur (saniyede 30 kez).
- Wi-Fi bağlantı durumu: **EventGroup** ile CONNECTED_BIT flag'i.

### 4.3 State Machine (Durum Makinesi)

Sistemin ana mantığı 3 durumlu bir state machine ile yönetilir:

```
IDLE ──→ PERSON_DETECTED ──→ PERSON_LEFT ──→ (HTTP gönder) ──→ IDLE
  ↑                                                              │
  └──────────────────────────────────────────────────────────────┘
```

**Durum Geçişleri:**

| Mevcut Durum | Koşul | Sonraki Durum | Eylem |
|---|---|---|---|
| IDLE | mesafe < 120cm | PERSON_DETECTED | Kronometre başlat, gürültü toplayıcı sıfırla |
| PERSON_DETECTED | mesafe > 150cm (5 ardışık okuma) | PERSON_LEFT | Dwell time ve ortalama gürültü hesapla |
| PERSON_DETECTED | mesafe < 150cm | PERSON_DETECTED | Gürültü ölçümü biriktir, taramaya devam |
| PERSON_LEFT | dwell_time < 1000ms | IDLE | Çok kısa, yoksay |
| PERSON_LEFT | dwell_time >= 1000ms | IDLE | HTTP POST gönder, yeni strateji uygula |

**Tasarım Kararları:**
- **Hysteresis (Gecikme Farkı):** Tespit eşiği 120cm, ayrılma eşiği 150cm. Bu 30cm'lik fark, kişi sınırda durduğunda sürekli tespit/kayıp döngüsünü önler.
- **Onay Sayacı:** Kişinin ayrıldığı 5 ardışık okuma (100ms × 5 = 500ms) ile onaylanır. Anlık sensör gürültüsünden kaynaklanan yanlış "ayrıldı" tespitlerini önler.
- **Minimum Dwell Time:** 1000ms altındaki tespitler yoksayılır. Hızlıca geçen kişiler veya sensör glitch'leri filtrelenir.
- **Zarif Bozulma (Graceful Degradation):** Wi-Fi kopuk veya sunucu erişilemezse, mevcut animasyon çalışmaya devam eder.

### 4.4 Modül Detayları

#### 4.4.1 config.h — Merkezi Konfigürasyon

Tüm sabitler bu dosyada tanımlanır:

| Kategori | Sabit | Değer | Açıklama |
|---|---|---|---|
| Wi-Fi | `WIFI_SSID` | "ssid_buraya" | Ağ adı |
| Wi-Fi | `WIFI_PASS` | "sifre_buraya" | Ağ şifresi |
| Wi-Fi | `WIFI_MAX_RETRIES` | 10 | Maksimum bağlantı deneme sayısı |
| Sunucu | `SERVER_URL` | "http://192.168.x.x:5000/api/trial" | Flask sunucu adresi |
| Sunucu | `HTTP_TIMEOUT_MS` | 10000 | HTTP istek zaman aşımı |
| Pin | `PIN_ULTRASONIC_TRIG` | GPIO_NUM_4 | HC-SR04 tetik pini |
| Pin | `PIN_ULTRASONIC_ECHO` | GPIO_NUM_5 | HC-SR04 yankı pini |
| Pin | `PIN_MIC_ADC` | GPIO_NUM_6 | Mikrofon analog giriş |
| Pin | `PIN_LED_DATA` | GPIO_NUM_7 | NeoPixel veri pini |
| ADC | `MIC_ADC_UNIT` | ADC_UNIT_1 | ADC birimi |
| ADC | `MIC_ADC_CHANNEL` | ADC_CHANNEL_5 | GPIO6 için ADC1 kanalı |
| ADC | `MIC_ADC_ATTEN` | ADC_ATTEN_DB_12 | Tam aralık ~0-3.1V |
| ADC | `MIC_SAMPLE_COUNT` | 256 | Ölçüm başına örnek sayısı |
| Ultrasonik | `ULTRASONIC_SCAN_INTERVAL_MS` | 100 | Tarama aralığı |
| Ultrasonik | `PERSON_DETECT_THRESHOLD_CM` | 120 | Tespit eşiği |
| Ultrasonik | `PERSON_LEFT_THRESHOLD_CM` | 150 | Ayrılma eşiği |
| Ultrasonik | `MIN_DWELL_TIME_MS` | 1000 | Minimum geçerli süre |
| Ultrasonik | `PERSON_LEFT_CONFIRM_COUNT` | 5 | Ayrılma onay sayısı |
| LED | `NUM_LEDS` | 8 | NeoPixel LED sayısı |
| LED | `LED_ANIMATION_FPS` | 30 | Animasyon kare hızı |
| LED | `LED_TASK_STACK_SIZE` | 4096 | LED task stack boyutu |

#### 4.4.2 wifi_manager — Wi-Fi Bağlantı Yöneticisi

**Arayüz:**

| Fonksiyon | Dönüş | Açıklama |
|---|---|---|
| `wifi_manager_init()` | `esp_err_t` | NVS, netif, event loop başlat, Wi-Fi STA konfigüre et ve bağlantı başlat |
| `wifi_manager_wait_connected(timeout)` | `esp_err_t` | Bağlanana kadar veya timeout'a kadar bloke ol |
| `wifi_manager_is_connected()` | `bool` | Anlık bağlantı durumu |

**Çalışma Prensibi:**
- ESP-IDF Wi-Fi STA (Station) modeli kullanılır.
- `WIFI_EVENT_STA_DISCONNECTED` event'inde otomatik `esp_wifi_connect()` çağrısı (retry sayacı ile).
- `IP_EVENT_STA_GOT_IP` event'inde EventGroup'ta `CONNECTED_BIT` set edilir ve IP adresi loglanır.
- Maksimum retry aşılırsa `esp_restart()` ile cihaz yeniden başlatılır.

#### 4.4.3 ultrasonic — HC-SR04 Sürücü

**Arayüz:**

| Fonksiyon | Dönüş | Açıklama |
|---|---|---|
| `ultrasonic_init()` | `esp_err_t` | Trig (output) ve Echo (input) GPIO konfigürasyonu |
| `ultrasonic_measure_cm(float *distance)` | `esp_err_t` | Tek ölçüm yap, sonucu cm olarak yaz |

**Çalışma Prensibi:**
1. Trig pinine 10µs HIGH darbesi gönderilir (`esp_rom_delay_us(10)`)
2. Echo pininin HIGH olması beklenir (timeout: 25ms)
3. HIGH başlangıç zamanı kaydedilir (`esp_timer_get_time()`)
4. Echo pininin LOW olması beklenir (timeout: 25ms)
5. HIGH bitiş zamanı kaydedilir
6. `mesafe_cm = süre_µs / 58.0`
7. Timeout durumunda hata kodu döner veya 999.0cm sentinel değeri kullanılır

**Ölçüm hassasiyeti:** ±3mm (HC-SR04 spesifikasyonu). Yazılım zamanlama çözünürlüğü: 1µs (`esp_timer_get_time`).

#### 4.4.4 mic_adc — MAX4466 Mikrofon ADC

**Arayüz:**

| Fonksiyon | Dönüş | Açıklama |
|---|---|---|
| `mic_adc_init()` | `esp_err_t` | ADC1 Oneshot driver konfigürasyonu, kalibrasyon |
| `mic_adc_read_noise_level(float *level)` | `esp_err_t` | 0-100 skalasında gürültü seviyesi oku |

**Çalışma Prensibi:**
1. ADC1, CH5, 12-bit çözünürlük, 12dB zayıflatma ile konfigüre edilir
2. ADC kalibrasyon şeması oluşturulur (ESP-IDF `adc_cali_create_scheme_curve_fitting`)
3. 256 adet hızlı ADC okuması yapılır
4. DC offset (ortalama) hesaplanır — MAX4466 çıkışı VCC/2 (~1.65V) civarında merkezlenir
5. Her örnekten DC offset çıkarılarak AC bileşen elde edilir
6. RMS (Root Mean Square) hesaplanır: `sqrt(sum((sample - mean)²) / N)`
7. RMS değeri 0-100 skalasına haritalanır (0 = sessiz, 100 = çok gürültülü)

**Kalibrasyon notu:** Skalalandırma faktörü (`max_rms` değeri) pratikte ortama göre ayarlanmalıdır. Ayrıca MAX4466 üzerindeki trim pot ile kazanç (gain) fiziksel olarak ayarlanabilir.

#### 4.4.5 led_animation — LED Animasyon Motoru

**Veri Yapıları:**

```c
typedef enum {
    ANIM_SOLID,          // Sabit renk
    ANIM_BREATHING,      // Nefes alma (fade in/out)
    ANIM_RAINBOW_CYCLE,  // Gökkuşağı döngüsü
    ANIM_BLINK,          // Yanıp sönme
    ANIM_WAVE,           // Dalga efekti
    ANIM_COLOR_WIPE,     // Sıralı dolum
    ANIM_COUNT           // Toplam animasyon sayısı
} animation_type_t;

typedef struct {
    animation_type_t animation;
    uint8_t r, g, b;     // RGB renk değerleri
    uint8_t speed;       // 1-100 (animasyon hızı)
} led_strategy_t;
```

**Arayüz:**

| Fonksiyon | Açıklama |
|---|---|
| `led_animation_init()` | led_strip init + FreeRTOS animasyon task'ını başlat |
| `led_animation_set_strategy(strategy)` | Yeni strateji ata (mutex ile thread-safe) |
| `led_animation_get_strategy()` | Mevcut stratejiyi oku (mutex ile thread-safe) |
| `led_animation_type_to_str(type)` | Enum → string dönüşümü ("breathing" vb.) |
| `led_animation_str_to_type(str)` | String → enum dönüşümü |

**LED Strip Konfigürasyonu:**
- Component: `espressif/led_strip` v3.0.0+ (IDF Component Registry)
- Backend: RMT (Remote Control Transceiver) peripheral
- LED modeli: WS2812 (GRB renk sırası)
- RMT çözünürlüğü: 10 MHz
- DMA: Kapalı (8 LED için gereksiz)

**Animasyon Task'ı:**
- Sonsuz döngüde ~30 FPS (33ms/kare) ile çalışır
- Her karede mevcut `led_strategy_t`'yi okur (mutex ile)
- İlgili animasyon fonksiyonunu çağırır
- Strateji değiştiğinde dahili sayaçlar sıfırlanır (kesintisiz geçiş)

**Yardımcı Fonksiyon:** `hsv_to_rgb()` — HSV renk uzayından RGB'ye dönüşüm (gökkuşağı animasyonu için gerekli).

#### 4.4.6 http_client — HTTP İstemci

**Veri Yapısı:**

```c
typedef struct {
    uint32_t dwell_time_ms;      // Kişinin durma süresi (ms)
    float noise_level;           // Ortalama gürültü seviyesi (0-100)
    led_strategy_t current_strategy;  // Ölçüm sırasındaki aktif strateji
} trial_data_t;
```

**Arayüz:**

| Fonksiyon | Açıklama |
|---|---|
| `http_client_send_trial(trial, new_strategy)` | Trial verisini POST et, yeni strateji al |

**Çalışma Prensibi:**
- ESP-IDF `esp_http_client` API kullanılır
- JSON oluşturma/parse: ESP-IDF'de dahili olan `cJSON` kütüphanesi (ek bağımlılık gerektirmez)
- POST body: trial verisi JSON formatında
- Response: yeni strateji JSON formatında
- Hata durumunda (timeout, HTTP 4xx/5xx, geçersiz JSON) hata kodu döner, çağıran taraf mevcut stratejiyi korur

### 4.5 Başlatma Sırası (app_main)

`app_main()` fonksiyonundaki başlatma adımları:

| Sıra | İşlem | Açıklama |
|---|---|---|
| 1 | `nvs_flash_init()` | Non-Volatile Storage başlat (Wi-Fi için gerekli) |
| 2 | `wifi_manager_init()` | Wi-Fi STA başlat |
| 3 | `wifi_manager_wait_connected(30s)` | Bağlantı bekle |
| 4 | `ultrasonic_init()` | HC-SR04 GPIO konfigüre |
| 5 | `mic_adc_init()` | ADC1 konfigüre ve kalibre et |
| 6 | `led_animation_init()` | LED strip + animasyon task'ını başlat |
| 7 | Varsayılan strateji ata | `ANIM_RAINBOW_CYCLE, r=0, g=0, b=255, speed=50` |
| 8 | Ana döngüye gir | State machine, 100ms aralıklarla |

---

## 5. Python Server Mimarisi

### 5.1 Dosya Yapısı

```
server/
├── app.py              # Flask uygulaması, REST endpoint'ler
├── gemini_client.py    # Gemini API entegrasyonu
├── requirements.txt    # Python bağımlılıkları
├── .env                # API anahtarı (git'e eklenmez)
└── trials.json         # Çalışma zamanında oluşturulur
```

**requirements.txt:**
```
flask>=3.0
google-generativeai>=0.8
python-dotenv>=1.0
```

### 5.2 Flask Endpoint'leri

| Method | URL | Açıklama |
|---|---|---|
| POST | `/api/trial` | Yeni trial verisi al, Gemini'ye sor, yeni strateji dön |
| GET | `/api/status` | Son 10 trial + güncel strateji (debug için) |

**POST /api/trial akışı:**
1. JSON body'den `dwell_time_ms`, `noise_level`, `current_strategy` al
2. trials.json'a yeni trial olarak ekle (trial_number otomatik artan)
3. Gemini API'ye trial geçmişini gönder
4. Gemini'den gelen yeni strateji JSON'unu doğrula (geçerli animasyon adı, RGB 0-255, speed 1-100)
5. Doğrulanmış stratejiyi ESP32'ye dön

**Hata durumu:** Gemini API hatası → rastgele strateji üretilerek döner (fallback).

### 5.3 Gemini Entegrasyonu

- **Model:** `gemini-3.1-flash-lite-preview`
- **Rol:** "Pazarlama ve Görsel Dikkat Uzmanı" — A/B test sonuçlarını analiz edip dwell time'ı maksimize edecek yeni LED stratejisi belirler
- **Bağlam:** Son 20 trial geçmişi (token tasarrufu için), en iyi ve en kötü trial vurgulanır
- **Strateji mantığı:**
  - < 5 trial: Keşif modu (çeşitli animasyonları dene)
  - >= 5 trial: Sömürü modu (iyi performans gösteren stratejileri küçük varyasyonlarla tekrarla)
- **Çıktı:** Sadece JSON formatında yanıt zorunluluğu, sunucu tarafında validasyon ve clamp uygulanır

### 5.4 Trial Geçmişi Yönetimi

- **Saklama:** `trials.json` dosyası (basit, okunabilir)
- **Format:** JSON dizisi, her eleman bir trial kaydı
- **Yükleme:** Sunucu her istekte dosyadan okur
- **Kaydetme:** Her yeni trial sonrası dosyaya yazar (indent=2, okunabilir format)

---

## 6. Veri Akışı ve JSON Şemaları

### 6.1 ESP32 → Sunucu (POST /api/trial body)

```json
{
  "dwell_time_ms": 4500,
  "noise_level": 42.3,
  "current_strategy": {
    "animation": "breathing",
    "r": 0,
    "g": 0,
    "b": 255,
    "speed": 50
  }
}
```

| Alan | Tip | Açıklama |
|---|---|---|
| `dwell_time_ms` | integer | Kişinin vitrin önünde durma süresi (milisaniye) |
| `noise_level` | float | Ortalama ortam gürültü seviyesi (0.0 - 100.0) |
| `current_strategy.animation` | string | Ölçüm sırasında aktif animasyon türü |
| `current_strategy.r/g/b` | integer | Aktif RGB renk değerleri (0-255) |
| `current_strategy.speed` | integer | Aktif animasyon hızı (1-100) |

### 6.2 Sunucu → ESP32 (Response body)

```json
{
  "animation": "rainbow_cycle",
  "r": 255,
  "g": 100,
  "b": 0,
  "speed": 75
}
```

| Alan | Tip | Açıklama |
|---|---|---|
| `animation` | string | Uygulanacak yeni animasyon türü |
| `r/g/b` | integer | Yeni RGB renk değerleri (0-255) |
| `speed` | integer | Yeni animasyon hızı (1-100) |

### 6.3 trials.json (Sunucu Disk Yapısı)

```json
[
  {
    "trial_number": 1,
    "dwell_time_ms": 4500,
    "noise_level": 42.3,
    "strategy": {
      "animation": "breathing",
      "r": 0,
      "g": 0,
      "b": 255,
      "speed": 50
    }
  }
]
```

---

## 7. LED Animasyon Türleri

| # | Animasyon | Tanım | RGB Kullanımı | Speed Etkisi |
|---|---|---|---|---|
| 1 | `solid` | Tüm 8 LED aynı renkte sabit yanar | Doğrudan renk | Etkisiz (statik) |
| 2 | `breathing` | Tüm LED'ler sinüzoidal parlaklık değişimi (fade in/out) | Temel renk | Nefes döngüsü süresi (hızlı=kısa döngü) |
| 3 | `rainbow_cycle` | Her LED HSV renk uzayında 45° ofsetli, hue döner | Yoksayılır | Döndürme hızı |
| 4 | `blink` | Tüm LED'ler açık/kapalı geçiş yapar | Açık durumdaki renk | Yanıp sönme frekansı |
| 5 | `wave` | Renk halkada "yürüyen dalga" efekti ile ilerler | Dalga rengi | Dalga ilerleme hızı |
| 6 | `color_wipe` | LED'ler sırayla birer birer yanar, sonra sırayla söner | Dolum rengi | Dolum hızı |

**Speed parametresi haritalama:**
- speed=1 → En yavaş (örn: breathing döngüsü ~5 saniye)
- speed=100 → En hızlı (örn: breathing döngüsü ~0.5 saniye)
- Ara değerler lineer interpolasyon ile hesaplanır

---

## 8. Hata Yönetimi Stratejisi

### 8.1 ESP32 Firmware Hataları

| Hata Durumu | Tespit | Çözüm |
|---|---|---|
| Wi-Fi bağlantı hatası | Event handler retry sayacı | 10 deneme, sonra `esp_restart()` |
| Wi-Fi çalışma sırasında kopma | `WIFI_EVENT_STA_DISCONNECTED` | Arka planda otomatik reconnect |
| Sunucu erişilemez / timeout | `esp_http_client` hata kodu | Log yaz, mevcut animasyonu koru |
| Sunucu geçersiz JSON dönerse | cJSON parse hatası | Log yaz, mevcut animasyonu koru |
| Sunucu HTTP 4xx/5xx | Status code kontrolü | Log yaz, mevcut animasyonu koru |
| Ultrasonik timeout (echo yok) | Ölçüm fonksiyonu hata/sentinel döner | 999cm kabul et (kişi yok) |
| Ultrasonik geçersiz mesafe | < 2cm veya > 400cm | Okuyu at, önceki geçerli okumayı kullan |
| ADC okuma hatası | `adc_oneshot_read` hata kodu | Örneklemi atla, sayacı artırma |
| LED strip init hatası | `led_strip_new_rmt_device` hata | Fatal — `ESP_ERROR_CHECK` ile dur (donanım hatası) |

### 8.2 Python Sunucu Hataları

| Hata Durumu | Çözüm |
|---|---|
| Gemini API hatası / timeout | Rastgele strateji üret (fallback) |
| Gemini geçersiz JSON dönerse | 1 kez tekrar dene, başarısızsa fallback |
| trials.json okunamaz | Boş liste ile başla |
| POST body geçersiz | HTTP 400 + hata mesajı dön |

### 8.3 Genel İlke

**"Asla durma, daima çalış."** Sunucu erişilemez olsa bile ESP32 mevcut animasyonunu göstermeye devam eder. Hiçbir hata durumunda sistem tamamen durmaz (LED strip init hariç — bu donanım bağlantı hatasıdır).

---

## 9. Uygulama Planı

Geliştirme, her fazda test edilebilir çıktılar üreten artımlı (incremental) bir yaklaşımla yapılacaktır.

### Faz 1: Temel Donanım (Serial Monitor ile Test)

| Adım | Modül | Test Yöntemi |
|---|---|---|
| 1 | `config.h` oluştur | Derleme başarısı |
| 2 | `led_animation` modülü + `idf_component.yml` | Flash → NeoPixel ring animasyon gösterimi |
| 3 | `ultrasonic` modülü | Flash → Serial monitörde mesafe okumaları |
| 4 | `mic_adc` modülü | Flash → Serial monitörde gürültü seviyeleri |

### Faz 2: State Machine (ESP32 Bağımsız)

| Adım | Modül | Test Yöntemi |
|---|---|---|
| 5 | `main.c` state machine (HTTP yerine serial log) | El hareketi ile yaklaş-uzaklaş, serial çıktıda doğru state geçişleri ve dwell_time kontrolü |

### Faz 3: Bağlantı

| Adım | Modül | Test Yöntemi |
|---|---|---|
| 6 | `wifi_manager` | Flash → Serial log'da IP adresi |
| 7 | `http_client` | Trial tetikle → sunucuya POST ulaşması |

### Faz 4: Python Sunucu

| Adım | Modül | Test Yöntemi |
|---|---|---|
| 8 | Flask sunucu (Gemini olmadan, rastgele fallback) | `curl` ile POST test, trials.json oluşumu |
| 9 | Gemini entegrasyonu | `curl` ile POST → Gemini stratejisi dönmesi |

### Faz 5: Entegrasyon ve Kalibrasyon

| Adım | İşlem | Test Yöntemi |
|---|---|---|
| 10 | Uçtan uca test | 10-20 trial, LED değişimi + trials.json kontrolü |
| 11 | Kalibrasyon | Eşik değerleri, mikrofon gain, Gemini prompt ayarı |

---

## 10. Test ve Doğrulama

### 10.1 Modül Testleri

| Modül | Doğrulama Kriteri |
|---|---|
| LED Animation | 6 animasyonun tamamı gözle doğrulanır; strateji değişimi anlık yansır |
| Ultrasonik | Bilinen mesafelerde (30cm, 100cm) ±5cm doğruluk |
| Mikrofon | Sessiz ortam < 15, konuşma ~30-50, alkış/bağırma > 70 |
| Wi-Fi | IP adresi alınır, router kapatıp açınca yeniden bağlanır |
| HTTP Client | Sunucu loglarında POST verisi görünür, response parse edilir |

### 10.2 Entegrasyon Testi

| Senaryo | Beklenen Davranış |
|---|---|
| Kişi yaklaşır, 5sn durur, uzaklaşır | Dwell time ~5000ms loglanır, sunucuya gönderilir, yeni animasyon uygulanır |
| Kişi hızlıca geçer (< 1sn) | Yoksayılır, sunucuya gönderilmez |
| Sunucu kapatılır | ESP32 mevcut animasyonla çalışmaya devam eder, hata loglanır |
| Wi-Fi kesilir | Otomatik reconnect denenır, bağlantı yokken trial gönderilmez |
| 10 ardışık trial | trials.json'da 10 kayıt, Gemini stratejileri giderek daha bilinçli |

### 10.3 `/api/status` ile İzleme

Geliştirme sırasında tarayıcıdan `http://localhost:5000/api/status` açılarak:
- Toplam trial sayısı
- Son 10 trial detayı
- Güncel aktif strateji

görüntülenebilir.

---

*Doküman tarihi: Mart 2025 | Proje: CogniDisplay | Platform: ESP32-S3, ESP-IDF 5.5.3*