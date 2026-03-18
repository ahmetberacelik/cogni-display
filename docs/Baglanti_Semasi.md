# CogniDisplay — Bağlantı Şeması

> ESP32-S3 DevKitC ile tüm sensör ve modüllerin breadboard üzerindeki bağlantıları.

---

## Genel Bakış

```
                        ┌─────────────────┐
                        │    ESP32-S3      │
                        │    DevKitC       │
                        │                  │
                  3.3V ─┤ 3V3         5V   ├─ 5V (USB'den)
                   GND ─┤ GND        GND   ├─ GND
          (Trig)  GPIO4─┤ 4                ├
          (Echo)  GPIO5─┤ 5                ├
    (Mikrofon)    GPIO6─┤ 6                ├
     (LED Data)   GPIO7─┤ 7                ├
                        │                  │
                        └─────────────────┘
```

---

## Güç Dağıtımı

Breadboard'un güç raylarını şu şekilde hazırla:

| Ray        | Kaynak              | Bağlanan Bileşenler                         |
|------------|----------------------|----------------------------------------------|
| **5V ray** | ESP32-S3 `5V` pini   | HC-SR04 VCC, Level Converter HV, NeoPixel 5V |
| **3.3V**   | ESP32-S3 `3V3` pini  | Level Converter LV, MAX4466 VCC              |
| **GND ray**| ESP32-S3 `GND` pini  | Tüm bileşenlerin GND'si (ortak toprak)       |

> **Kritik:** Tüm bileşenlerin GND'si aynı GND rayına bağlanmalı. Ortak toprak olmadan sinyaller düzgün çalışmaz.

---

## 1. Level Converter (3.3V ↔ 5V) Bağlantıları

Level converter'ın **ortadaki** modül olarak düşün — ESP32 tarafı (LV/düşük gerilim) ve sensör tarafı (HV/yüksek gerilim) arasında köprü görevi görüyor.

### Güç Pinleri

| Level Converter Pini | Bağlantı           |
|-----------------------|---------------------|
| **LV**                | 3.3V ray            |
| **HV**                | 5V ray              |
| **GND** (LV tarafı)  | GND ray             |
| **GND** (HV tarafı)  | GND ray             |

### Sinyal Kanalları

| Kanal | LV Pini (3.3V tarafı)   | HV Pini (5V tarafı)         | Sinyal Yönü       |
|-------|--------------------------|------------------------------|--------------------|
| CH1   | LV1 ← ESP32 **GPIO4**   | HV1 → HC-SR04 **Trig**      | ESP32 → Sensör     |
| CH2   | LV2 ← ESP32 **GPIO5**   | HV2 ← HC-SR04 **Echo**      | Sensör → ESP32     |
| CH3   | LV3 ← ESP32 **GPIO7**   | HV3 → 330Ω → NeoPixel **DIN** | ESP32 → LED Ring |
| CH4   | (boş)                   | (boş)                        | —                  |

---

## 2. HC-SR04 Ultrasonik Sensör Bağlantıları

HC-SR04 5V ile çalışır, pinleri **doğrudan ESP32'ye bağlanmaz**. Level converter üzerinden geçer.

| HC-SR04 Pini | Bağlantı                       |
|---------------|--------------------------------|
| **VCC**       | 5V ray                         |
| **Trig**      | Level Converter **HV1**        |
| **Echo**      | Level Converter **HV2**        |
| **GND**       | GND ray                        |

```
ESP32 GPIO4 ──► LV1 ═══ HV1 ──► HC-SR04 Trig
ESP32 GPIO5 ◄── LV2 ═══ HV2 ◄── HC-SR04 Echo
                    Level
                  Converter
```

---

## 3. MAX4466 Mikrofon Bağlantıları

MAX4466 3.3V ile çalışır, **level converter'a gerek yok**, doğrudan ESP32'ye bağlanır.

| MAX4466 Pini | Bağlantı              |
|--------------|------------------------|
| **VCC**      | 3.3V ray               |
| **GND**      | GND ray                |
| **OUT**      | ESP32 **GPIO6**        |

```
MAX4466 OUT ──────► ESP32 GPIO6 (ADC1_CH5)
MAX4466 VCC ──────► 3.3V ray
MAX4466 GND ──────► GND ray
```

> **Not:** GPIO6 özellikle seçildi çünkü ADC1 kanalında. ADC2, Wi-Fi açıkken çalışmaz.

---

## 4. WS2812B NeoPixel Ring Bağlantıları

NeoPixel 5V ile çalışır, data hattı level converter üzerinden geçer. Data hattına **330Ω direnç** eklenir (sinyal koruma).

| NeoPixel Pini | Bağlantı                                  |
|---------------|--------------------------------------------|
| **5V**        | 5V ray                                     |
| **GND**       | GND ray                                    |
| **DIN**       | 330Ω direnç ← Level Converter **HV3**     |
| **DOUT**      | Bağlantısız (zincirleme LED yoksa)         |

```
ESP32 GPIO7 ──► LV3 ═══ HV3 ──► [330Ω] ──► NeoPixel DIN
                    Level
                  Converter
```

---

## 5. Kondansatör (1000µF) Bağlantısı

NeoPixel'in ani akım çekişlerini dengelemek için 5V güç hattına eklenir.

| Kondansatör Bacağı         | Bağlantı  |
|----------------------------|-----------|
| **Uzun bacak (+) pozitif** | 5V ray    |
| **Kısa bacak (−) negatif** | GND ray   |

> **Dikkat:** Ters bağlama kondansatörü kalıcı olarak hasar verir. Uzun bacak = pozitif.

Kondansatörü breadboard'da NeoPixel'in güç bağlantısına yakın bir yere koy.

---

## Tüm Bağlantıların Özet Tablosu

| ESP32-S3 Pini | Gidiş Yeri                        | Ara Bileşen              |
|----------------|-----------------------------------|--------------------------|
| **3V3**        | Level Converter LV, MAX4466 VCC   | —                        |
| **5V**         | Level Converter HV, HC-SR04 VCC, NeoPixel 5V, Kondansatör (+) | — |
| **GND**        | Tüm bileşenlerin GND'si, Kondansatör (−) | —               |
| **GPIO4**      | HC-SR04 Trig                      | Level Converter CH1      |
| **GPIO5**      | HC-SR04 Echo                      | Level Converter CH2      |
| **GPIO6**      | MAX4466 OUT                       | Doğrudan (converter yok) |
| **GPIO7**      | NeoPixel DIN                      | Level Converter CH3 + 330Ω direnç |

---

## Breadboard Yerleşim Önerisi

```
┌──────────────────────────────────────────────────────────┐
│  [5V Ray]  ════════════════════════════════════════════   │
│  [GND Ray] ════════════════════════════════════════════   │
│                                                          │
│  ┌───────────┐   ┌──────────┐   ┌────────┐   ┌───────┐  │
│  │ Level     │   │ HC-SR04  │   │MAX4466 │   │NeoPixel│ │
│  │ Converter │   │          │   │        │   │ Ring   │  │
│  │           │   │ VCC GND  │   │VCC GND │   │5V  GND│  │
│  │LV1 ── HV1├───┤ Trig     │   │OUT     │   │DIN    │  │
│  │LV2 ── HV2├───┤ Echo     │   │  │     │   │  ▲    │  │
│  │LV3 ── HV3├───┤          │   │  │     │   │  │    │  │
│  │LV  ── HV │   └──────────┘   │  │     │   │[330Ω]│  │
│  │GND ── GND│                   └──┼─────┘   │  │    │  │
│  └─┬──┬──┬──┘                      │         └──┼────┘  │
│    │  │  │                         │            │        │
│    │  │  └─── GPIO7 ───────────────┼────────────┘        │
│    │  └────── GPIO5                │                     │
│    └───────── GPIO4                │                     │
│                          GPIO6 ────┘                     │
│                                                          │
│          ┌────────────────┐                              │
│          │   ESP32-S3     │   [1000µF]                   │
│          │   DevKitC      │   (+)──5V ray                │
│          │ (breadboard    │   (−)──GND ray               │
│          │  dışında,      │                              │
│          │  M-F kablolarla│                              │
│          │  bağlanır)     │                              │
│          └────────────────┘                              │
│                                                          │
│  [GND Ray] ════════════════════════════════════════════   │
│  [5V Ray]  ════════════════════════════════════════════   │
└──────────────────────────────────────────────────────────┘
```

> **İpucu:** ESP32-S3'ün pinleri ucuz breadboard'larda düzgün temas etmeyebilir. ESP32'yi breadboard dışında tut ve M-F (dişi-erkek) jumper kablolarla breadboard'a bağla.

---

## Kontrol Listesi

Bağlantıları yaptıktan sonra bu listeyi kontrol et:

- [ ] Tüm GND'ler aynı GND rayına bağlı mı?
- [ ] Level converter'ın LV pini 3.3V'a, HV pini 5V'a bağlı mı?
- [ ] HC-SR04'ün Trig ve Echo pinleri level converter üzerinden mi geçiyor?
- [ ] MAX4466 doğrudan GPIO6'ya bağlı mı (level converter yok)?
- [ ] NeoPixel DIN'e giden hatta 330Ω direnç var mı?
- [ ] 1000µF kondansatör doğru kutupla (uzun bacak = +) 5V-GND arasına bağlı mı?
- [ ] ESP32 USB'den güç alıyor mu (5V pini aktif)?

---

*Oluşturulma tarihi: Mart 2025 | Proje: CogniDisplay*
