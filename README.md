# 🐟 Smart Fish Feeding System (Blynk IoT)

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![IoT Platform](https://img.shields.io/badge/IoT-Blynk-success.svg)](https://blynk.io/)

**Smart Fish Feeding System** adalah proyek IoT berbasis ESP32-S3 yang mengotomatisasi proses pemberian pakan ikan dan secara bersamaan memonitor kualitas air serta level pakan. Proyek ini terintegrasi penuh dengan platform **Blynk IoT** untuk kontrol manual, penjadwalan, dan monitoring data sensor secara *real-time* dari mana saja.

***

## ✨ Fitur Utama

| Kategori | Fitur | Detail Teknis |
| :--- | :--- | :--- |
| **Kontrol Pakan** | **Penjadwalan Otomatis** | Memberikan pakan secara otomatis pada jam yang telah ditentukan (default: 09:00, 15:00, 21:00) menggunakan `WidgetRTC` Blynk. |
| | **Pakan Manual** | Pakan dapat diberikan secara instan melalui tombol di aplikasi Blynk. |
| | **Mekanisme Servo** | Menggunakan Servo untuk mekanisme *dispensing*, membuka 120 derajat selama 1 detik. |
| **Monitoring Air** | **Sensor pH** | Mengukur tingkat keasaman air, menggunakan kalibrasi 3-titik (4.0, 7.0, 10.0). |
| | **Sensor TDS/PPM** | Mengukur Total Dissolved Solids (kandungan nutrisi/mineral) dengan kompensasi suhu dan filter median 30 sampel. |
| | **Sensor Suhu** | Mengukur suhu air (`DS18B20`) untuk kompensasi TDS. |
| **Monitoring Pakan** | **Ultrasonic Level** | Menggunakan sensor JSN-SR04T untuk mengukur jarak pakan, memberikan indikasi level pakan yang tersisa. |
| **Komunikasi** | **Data Real-time** | Semua data sensor dikirim ke Blynk setiap 10 detik. |

***

## 🛠️ Persyaratan Hardware

| Komponen | Pin (GPIO) | Detail |
| :--- | :--- | :--- |
| **Mikrokontroler** | - | ESP32 Dev Module (atau sejenisnya) |
| **Servo Motor** | **1** | Servo SG90 atau sejenisnya untuk mekanisme pakan. |
| **Sensor pH** | **4** | Modul Sensor pH Analog. |
| **Sensor TDS** | **5** | Modul Sensor TDS Analog. |
| **Sensor Suhu** | **6** | Sensor DS18B20 (OneWire). |
| **Ultrasonic** | **TRIG: 17** | Sensor JSN-SR04T / HC-SR04 (Untuk Level Pakan). |
| **Ultrasonic** | **ECHO: 16** | Sensor JSN-SR04T / HC-SR04. |

***

## ⚙️ Instalasi dan Setup

### 1. Kebutuhan Library

Pastikan semua library berikut sudah terinstal di Arduino IDE / VS Code Anda:

* `WiFi` (Built-in ESP32)
* `BlynkSimpleEsp32`
* `OneWire`, `DallasTemperature`
* `ESP32Servo`
* `WidgetRTC`
* `NewPing` (untuk Ultrasonic)

### 2. Konfigurasi Awal (`fish_feeding.ino`)

Sebelum mengunggah kode, modifikasi bagian-bagian ini:

| Bagian | Konstanta | Keterangan |
| :--- | :--- | :--- |
| **Blynk Config** | `BLYNK_TEMPLATE_ID` | `TMPL67fGjS_6t` (Ganti dengan Template ID Blynk Anda)|
| | `BLYNK_AUTH_TOKEN` | `z3uKQfMErQW92uMgMzF2umXf_lLa8LF7` (Ganti dengan Auth Token Perangkat Anda)|
| **WiFi Config** | `ssid` | Nama WiFi Anda (`YTTA` default)|
| | `pass` | Password WiFi Anda|
| **Feeding Schedule** | `FEEDING_HOURS[]` | Atur jam pemberian pakan otomatis (Default: `{9, 15, 21}`)|

### 3. Virtual Pin Blynk

Pastikan widget-widget di Blynk Anda terhubung ke Virtual Pin (VPIN) yang sesuai:

| VPIN | Fungsi | Jenis Widget (Contoh) |
| :--- | :--- | :--- |
| `V1` | pH Value | Gauge / Nilai |
| `V2` | TDS (PPM) | Gauge / Nilai |
| `V3` | Temperature (°C) | Gauge / Nilai |
| `V4` | Distance (cm) | Gauge / Nilai |
| `V5` | Manual Feed | Tombol (Mode PUSH) |
| `V6` | Status System | Label |

***

## 📝 Catatan Tambahan

### Kalibrasi Sensor

* **pH Sensor:** Kode ini menggunakan nilai kalibrasi yang sudah *hardcoded* (`PH_ACID_VOLTAGE`, `PH_NEUTRAL_VOLTAGE`, `PH_ALKALINE_VOLTAGE`). Untuk akurasi maksimum, ganti nilai-nilai ini setelah mengukur voltase sensor Anda pada larutan pH 4, 7, dan 10.
* **TDS Sensor:** `TDS_CORRECTION_FACTOR` diatur ke `1.0` secara default. Koreksi ini dapat disesuaikan jika Anda memiliki standar TDS yang diketahui.

### RTC (Real-Time Clock)

* Sistem menggunakan `WidgetRTC` Blynk, yang berarti jam akan disinkronkan dari server Blynk saat perangkat terhubung.
* Status pemberian pakan otomatis direset setiap hari pada jam 00:01.
