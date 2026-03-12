# ESP32-S3 Projesi — Donanım Teknik Özellikler Raporu

> Bu rapor, ESP32-S3 tabanlı proje için satın alınan donanım bileşenlerinin tüm teknik özelliklerini kapsamaktadır.

---

## İçindekiler

1. [HC-SR04 Ultrasonik Mesafe Sensörü](#1-hc-sr04-ultrasonik-mesafe-sensörü)
2. [Lojik Gerilim Seviye Dönüştürücü (3.3V – 5V)](#2-lojik-gerilim-seviye-dönüştürücü-33v--5v)
3. [MAX4466 Elektret Mikrofon Modülü](#3-max4466-elektret-mikrofon-modülü)
4. [8-Bit NeoPixel Ring WS2812B 5050 RGB LED Modülü](#4-8-bit-neopixel-ring-ws2812b-5050-rgb-led-modülü)
5. [Breadboard (830 Delik)](#5-breadboard-830-delik)
6. [20 cm 40 Pin M-M Jumper Kablo](#6-20-cm-40-pin-m-m-jumper-kablo)
7. [30 cm 40 Pin M-F Jumper Kablo](#7-30-cm-40-pin-m-f-jumper-kablo)
8. [1/4W 330Ω Direnç Paketi (10 Adet)](#8-14w-330ω-direnç-paketi-10-adet)
9. [16V 1000µF Elektrolitik Kondansatör Paketi (5 Adet)](#9-16v-1000µf-elektrolitik-kondansatör-paketi-5-adet)

---

## 1. HC-SR04 Ultrasonik Mesafe Sensörü

**Kaynak:** [robotistan.com](https://www.robotistan.com/hc-sr04-ultrasonik-mesafe-sensoru)

### Genel Tanım
HC-SR04, 40 kHz ultrasonik dalgalar kullanarak 2 cm ile 400 cm arasında mesafe ölçümü yapabilen, düşük maliyetli ve yaygın kullanımlı bir sensördür. Uzaklık okuma, radar ve robot uygulamalarında tercih edilir.

> ⚠️ **ESP32-S3 Uyumluluğu:** HC-SR04 5V mantık seviyesi ile çalışır. ESP32-S3'ün I/O pinleri 3.3V toleranslıdır. **Echo pini için mutlaka Logic Level Converter kullanılmalıdır.**

### Teknik Özellikler

| Parametre | Değer |
|---|---|
| Çalışma Voltajı | DC 5V |
| Çektiği Akım | 15 mA |
| Çalışma Frekansı | 40 kHz |
| Ölçüm Parametresi | Mesafe |
| Minimum Görme Menzili | 2 cm |
| Maksimum Görme Menzili | 4 m (400 cm) |
| Ölçüm Hassasiyeti | 3 mm |
| Görme Açısı | 15° |
| Tetik Giriş Sinyali | 10 µs TTL Darbesi |
| Echo Çıkış Sinyali | Giriş TTL sinyali ve mesafe oranı |
| Boyutlar | 45 × 20 × 15 mm |

### Pin Yapısı

| Pin | Açıklama |
|---|---|
| VCC | 5V Güç Girişi |
| Trig | Tetik Sinyali (Dijital Giriş) |
| Echo | Yankı Sinyali (Dijital Çıkış) |
| GND | Toprak |

### Çalışma Prensibi
1. `Trig` pinine en az 10 µs'lik TTL yüksek sinyali verilir.
2. Sensör 8 adet 40 kHz ultrasonik darbe yayar.
3. Ses dalgası engele çarpıp geri döndüğünde `Echo` pini HIGH olur.
4. HIGH süresinin ölçülmesiyle mesafe hesaplanır: `Mesafe (cm) = Süre (µs) / 58`

---

## 2. Lojik Gerilim Seviye Dönüştürücü (3.3V – 5V)

**Kaynak:** [hepsiburada.com](https://www.hepsiburada.com/robo-dunya-lojik-gerilim-seviye-donusturucu-3-3-v-5-v-logic-level-converter-pm-HBC00003Z3XND)  
**Marka:** Robo Dünya

### Genel Tanım
Çift yönlü (bi-directional) lojik seviye dönüştürücü modülüdür. 5V mantık sinyallerini 3.3V'a, 3.3V sinyallerini 5V'a dönüştürür. ESP32-S3 (3.3V) ile 5V çalışan çevre birimler (HC-SR04 gibi) arasındaki sinyal uyumsuzluğunu giderir.

### Teknik Özellikler

| Parametre | Değer |
|---|---|
| Yüksek Taraf Gerilimi (HV) | 5V |
| Düşük Taraf Gerilimi (LV) | 3.3V |
| Dönüşüm Yönü | Çift Yönlü (Bi-Directional) |
| Kanal Sayısı | 4 Kanal (genellikle BSS138 MOSFET tabanlı) |
| Çalışma Prensibi | MOSFET tabanlı gerilim bölücü |
| Garanti Süresi | 1 Ay |

### Pin Yapısı

| Pin | Açıklama |
|---|---|
| HV | Yüksek gerilim girişi (5V) |
| LV | Düşük gerilim girişi (3.3V) |
| GND | Toprak (her iki taraf ortak) |
| HV1–HV4 | 5V taraf sinyal pinleri |
| LV1–LV4 | 3.3V taraf sinyal pinleri |

### Kullanım Notu
- `HV` pinine 5V bağlanır (HC-SR04'ün besleme tarafı)
- `LV` pinine 3.3V bağlanır (ESP32-S3 tarafı)
- `GND` her iki devreyle ortak toprak yapılır
- Sinyal kablosu ilgili HVx/LVx pin çiftine bağlanır

---

## 3. MAX4466 Elektret Mikrofon Modülü

**Kaynak:** [hepsiburada.com](https://www.hepsiburada.com/741-elektronik-max4466-elektret-mikrofon-modulu-pm-HBC00002OH10K)  
**Marka:** 741 Elektronik

### Genel Tanım
Maxim Integrated'ın MAX4466 op-amp entegresi ile donatılmış elektret mikrofon modülüdür. Ayarlanabilir kazanç (gain) özelliği sayesinde zayıf ses sinyallerini güçlendirerek analog çıkış verir. Ses algılama, ses seviyesi ölçümü ve ses kayıt uygulamalarında kullanılır.

### Teknik Özellikler (MAX4466 IC Tabanlı)

| Parametre | Değer |
|---|---|
| Çalışma Voltajı | 2.4V – 5.5V |
| Çıkış Tipi | Analog (ses sinyali) |
| Kazanç Aralığı | 25× – 125× (ayarlanabilir trim pot ile) |
| Frekans Bandı | 20 Hz – 20 kHz |
| Orta Nokta Gerilimi | VCC/2 (DC bias) |
| Yayın Modeli | Tek Beslemeli (Single Supply) |
| Gürültü Karakteristiği | Düşük gürültülü mikrofon amplifikatörü |
| Mikrofon Tipi | Elektret kondenser |
| Çıkış Empedansı | Düşük (~600Ω) |

### Pin Yapısı

| Pin | Açıklama |
|---|---|
| VCC | Güç Girişi (2.4V – 5.5V; 3.3V ile ESP32-S3'e doğrudan bağlanabilir) |
| GND | Toprak |
| OUT | Analog ses çıkışı (ESP32-S3 ADC pinine bağlanır) |

### Kullanım Notu
- ESP32-S3'ün 3.3V pini ile doğrudan beslenebilir
- OUT pini ESP32-S3'ün ADC pinine bağlanır
- Board üzerindeki trim pot ile kazanç ayarlanabilir
- Yüksek kazanç → hassas ama gürültüye duyarlı; düşük kazanç → daha temiz sinyal

---

## 4. 8-Bit NeoPixel Ring WS2812B 5050 RGB LED Modülü

**Kaynak:** [hepsiburada.com](https://www.hepsiburada.com/roboyol-store-8-bit-neopixel-halka-ring-ws2812b-5050-rgb-led-modul-5050-adreslenebilir-lamba-kart-full-color-ekran-pm-HBC00006WAURV)  
**Marka:** Roboyol Store

### Genel Tanım
WS2812B sürücü entegresi gömülü 8 adet adreslenebilir 5050 RGB LED'den oluşan halka (ring) modülüdür. Tek bir data hattı üzerinden tüm LED'ler bireysel olarak kontrol edilebilir. Birden fazla modül zincirleme bağlanabilir.

> ⚠️ **ESP32-S3 Uyumluluğu:** WS2812B data hattı 5V mantık seviyesi bekler. ESP32-S3 (3.3V) bazı koşullarda çalışabilse de güvenilir iletişim için Logic Level Converter kullanımı önerilir. Güç hattına (5V ile GND arasına) 100-470µF kondansatör eklenmesi tavsiye edilir.

### Teknik Özellikler

| Parametre | Değer |
|---|---|
| LED Sayısı | 8 adet WS2812B 5050 RGB LED |
| Çalışma Voltajı | DC 5V (4V – 7V aralığı) |
| LED Başına Akım | 20 mA (tam beyaz, maksimum parlaklık) |
| Toplam Maksimum Akım | ~160 mA (8 LED × 20 mA) |
| Renk Derinliği | 24 bit (R:8 + G:8 + B:8) |
| Renk Sayısı | 16.7 Milyon (256³) |
| Parlaklık Seviyesi | 256 kademe (her kanal) |
| Yenileme Hızı | 30 FPS (1024 LED maksimum zincirde) |
| Maksimum Zincirleme | 1024 LED (modüller arası 5 metreye kadar) |
| Dış Çap | 32 mm |
| İç Çap | 18 mm |
| Haberleşme Protokolü | Tek-kablo seri (NeoPixel/WS2812 protokolü) |
| Garanti Süresi | 6 Ay |

### Pin Yapısı

| Pin | Açıklama |
|---|---|
| 5V | Güç Girişi (5V) |
| GND | Toprak |
| DIN | Data Giriş Pini (ESP32-S3'ten gelen sinyal) |
| DOUT | Data Çıkış Pini (bir sonraki modüle bağlanır) |

### Uyumlu Kütüphaneler
- **Arduino/ESP-IDF:** `FastLED`, `Adafruit NeoPixel`
- **MicroPython:** `neopixel` modülü

---

## 5. Breadboard (830 Delik)

**Kaynak:** [robotistan.com](https://www.robotistan.com/breadboard)  
**Marka:** Robotistan

### Genel Tanım
Lehimsiz prototipleme için kullanılan standart büyüklükte breadboard. ESP32-S3 ve diğer modüllerin bağlantılarını hızlıca test etmek için idealdir.

### Teknik Özellikler

| Parametre | Değer |
|---|---|
| Toplam Delik Sayısı | 830 |
| Güç Hattı | 2 adet güç bus şeridi |
| Satır Sayısı | 60 satır |
| Sütun Sayısı | 10 sütun |
| Pin Aralığı (Pitch) | 2.54 mm (standart) |
| Uyumlu Kablo Çapı | 29 – 20 AWG |
| Renk | Beyaz |
| Arka Yüzey | Yapışkanlı kâğıt (yüzeye sabitlenebilir) |
| Boyutlar | 165.5 × 54.5 × 8.5 mm |
| DIP IC Desteği | Evet (5 sıralı ikiye ayrılmış yapı) |

---

## 6. 20 cm 40 Pin M-M Jumper Kablo

**Kaynak:** [robotistan.com](https://www.robotistan.com/20cm-40-pin-m-m-jumper-wires)  
**Marka:** Robotistan

### Genel Tanım
Her iki ucu erkek (male) olan, breadboard ve geliştirme kartı bağlantıları için tasarlanmış hazır jumper kablo seti.

### Teknik Özellikler

| Parametre | Değer |
|---|---|
| Uzunluk | 20 cm |
| Toplam Kablo Adedi | 40 |
| Renk Çeşidi | 10 farklı renk |
| Her Renkten Adet | 4 |
| Uç Tipi | Erkek – Erkek (M-M) |
| Pin Aralığı Uyumu | 2.54 mm (standart) |
| İletken Kesiti | 26 AWG |
| Kullanım Alanı | Breadboard, Arduino, Raspberry Pi, ESP32 vb. |

---

## 7. 30 cm 40 Pin M-F Jumper Kablo

**Kaynak:** [robotistan.com](https://www.robotistan.com/30cm-40-pin-m-f-jumper-wires)  
**Marka:** Robotistan

### Genel Tanım
Bir ucu erkek (male), diğer ucu dişi (female) olan jumper kablo seti. GPIO header pinlerinden breadboard'a veya modüllere bağlantı için uygundur.

### Teknik Özellikler

| Parametre | Değer |
|---|---|
| Uzunluk | 30 cm |
| Toplam Kablo Adedi | 40 |
| Renk Çeşidi | 10 farklı renk |
| Her Renkten Adet | 4 |
| Uç Tipi | Erkek – Dişi (M-F) |
| Pin Aralığı Uyumu | 2.54 mm (standart) |
| İletken Kesiti | 26 AWG |
| Kullanım Alanı | Breadboard, Arduino, Raspberry Pi, ESP32 header pinleri vb. |

---

## 8. 1/4W 330Ω Direnç Paketi (10 Adet)

**Kaynak:** [robotistan.com](https://www.robotistan.com/14w-330r-direnc-paketi-10-adet)  
**Marka:** Robotistan

### Genel Tanım
Karbon film (carbon film) yapılı, renk bantlarıyla değeri kodlanmış standart delik montaj (through-hole) dirençtir. LED akım sınırlama, pull-up/pull-down ve sinyal bölme devrelerinde yaygın biçimde kullanılır.

### Teknik Özellikler

| Parametre | Değer |
|---|---|
| Direnç Değeri | 330 Ω (Ohm) |
| Güç Değeri | 1/4 W (0.25 W) |
| Tolerans | ±5% (genellikle altın bant) |
| Paket İçeriği | 10 adet |
| Montaj Tipi | Through-Hole (delik montaj) |
| Renk Kodu | Turuncu – Turuncu – Kahverengi – Altın |
| Tipik Kullanım | LED seri direnci (5V → LED), sinyal bölücü |

### LED Hesabı (ESP32-S3 ile)
- ESP32-S3 GPIO çıkışı: ~3.3V
- LED ileri gerilimi (kırmızı): ~2.0V
- Akım = (3.3V – 2.0V) / 330Ω ≈ **3.9 mA** ✅ (güvenli)
- WS2812B data sinyali için de 300–500Ω seri direnç önerilir

---

## 9. 16V 1000µF Elektrolitik Kondansatör Paketi (5 Adet)

**Kaynak:** [robotistan.com](https://www.robotistan.com/16v-1000uf-kondansator-paketi-5-adet)  
**Marka:** Robotistan

### Genel Tanım
Elektronik devrelerde güç kaynağı filtrelemesi, bypass ve enerji depolama amacıyla kullanılan polar (kutuplu) elektrolitik kondansatördür. Özellikle WS2812B LED ring ve motor gibi ani akım çeken yükler için güç hattına eklenmesi şiddetle tavsiye edilir.

### Teknik Özellikler

| Parametre | Değer |
|---|---|
| Kapasitans | 1000 µF |
| Maksimum Gerilim | 16V |
| Tip | Elektrolitik (Polar) |
| Paket İçeriği | 5 adet |
| Montaj Tipi | Through-Hole (radyal) |
| Kutupluluk | Pozitif (+) ve Negatif (−) |
| Tipik Kullanım | Güç hattı filtreleme, decoupling, ani akım tamponlama |

### Kullanım Notu
- Kondansatörü devre üzerinde 5V güç hattı ile GND arasına bağlayın (NeoPixel besleme hattına yakın olmalı)
- Uzun bacak **pozitif (+)**, kısa bacak veya beyaz şerit işaretli bacak **negatif (−)**
- Ters bağlantı kondansatörü kalıcı olarak hasar verebilir veya patlatabilir

---

## Bileşenler Arası Bağlantı Özeti (ESP32-S3)

```
ESP32-S3 (3.3V sistemi)
    │
    ├─► Logic Level Converter (LV=3.3V ↔ HV=5V)
    │       ├─ HC-SR04 (5V, Trig & Echo pinleri)
    │       └─ WS2812B Ring DIN (5V data)
    │
    ├─► MAX4466 Mikrofon (3.3V ile doğrudan besleme, OUT → ADC)
    │
    ├─► 330Ω Direnç → LED / WS2812B data koruma
    │
    └─► 1000µF Kondansatör (5V güç hattı filtresi, WS2812B yakınına)
```

---

## Önemli Notlar

| # | Konu | Açıklama |
|---|---|---|
| 1 | **Gerilim Uyumsuzluğu** | ESP32-S3 GPIO'ları 3.3V; HC-SR04 Echo ve WS2812B data hattı 5V. Logic Level Converter zorunludur. |
| 2 | **NeoPixel Akım** | 8 LED tam beyazda ~160 mA çeker. 5V güç kaynağının bu akımı karşılayabilecek kapasitede olmasına dikkat edin. |
| 3 | **Kondansatör Yönü** | Elektrolitik kondansatör polar'dır; ters bağlama arızaya yol açar. |
| 4 | **MAX4466 Gürültü** | Yüksek kazanç gürültüyü artırır. Proje gereksinimlerine göre trim pot ayarlayın. |
| 5 | **Breadboard Akım Limiti** | Breadboard'un güç rayları ~1A ile sınırlıdır; yüksek akım çeken devreler için ayrı bağlantı kullanın. |

---

*Rapor tarihi: Ocak 2025 | Hedef platform: ESP32-S3*
