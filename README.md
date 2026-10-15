<img width="1531" height="1224" alt="image" src="https://github.com/user-attachments/assets/9b6a5b23-8a90-4dea-987e-f43cab8ed2da" />

MQAIRPURIFIER

# 🍃 Mq Air Purifier — Smart IoT Air Purifier & Telemetry Dashboard

[![ESP32-C3](https://img.shields.io/badge/Hardware-ESP32--C3%20Super%20Mini-blue?logo=espressif&style=flat-square)](https://espressif.com/)
[![MQTT Protocol](https://img.shields.io/badge/Protocol-MQTT%20HiveMQ-orange?logo=mqtt&style=flat-square)](https://www.hivemq.com/)
[![GitHub Pages](https://img.shields.io/badge/Deployment-GitHub%20Pages-brightgreen?logo=github&style=flat-square)](https://pages.github.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](https://opensource.org/licenses/MIT)

**Mq Air Purifier** adalah sistem pembersih dan pemantau kualitas udara pintar berbasis **ESP32-C3 Super Mini**, sensor gas **MQ-135**, sensor suhu/kelembapan **DHT22**, serta kipas exhaust 12V. 

Sistem ini terhubung langsung ke broker publik **HiveMQ MQTT** dan dilengkapi dengan web dashboard modern yang dapat diakses secara daring melalui **GitHub Pages** tanpa server backend tambahan.

---

## ✨ Fitur Utama

- **Offline-First & Auto-Fallback**: Sensor, indikator 3 LED (Hijau, Kuning, Merah), dan kipas tetap beroperasi otomatis mengendalikan kualitas udara ruangan meskipun jaringan internet/WiFi terputus.
- **Real-Time Telemetry**: Pengiriman data suhu, kelembapan, status gas MQ-135, serta status kipas relay secara periodik ke broker HiveMQ via protokol MQTT.
- **Modern Web Dashboard**: Tampilan visual clean, responsif, dan elegan (Tailwind CSS, Feather Icons) yang menyajikan metrik sensor, log telemetri, serta kontrol sakelar kipas instan.
- **Multi-Node Architecture**: Format topik MQTT dinamis memungkinkan monitoring banyak perangkat purifikasi sekaligus (*mesh-ready*) dari satu halaman dashboard.
- **Remote Calibration**: Pengaturan ambang batas (*threshold*) dan offset sensor dapat dilakukan secara nirkabel dari tab dashboard serta disimpan permanen ke memori flash internal ESP32 (NVS Preferences).

---

## 🛠️ Spesifikasi Perangkat Keras (Hardware)

| Komponen | Tipe / Spesifikasi | Pin ESP32-C3 | Fungsi |
| :--- | :--- | :--- | :--- |
| **Microcontroller** | ESP32-C3 Super Mini | — | Pemroses utama sistem |
| **Air Sensor** | MQ-135 (Gas & Partikel) | `GPIO 0` (ADC1) | Deteksi gas polutan / aroma |
| **Temp/Hum Sensor** | DHT22 / AM2302 | `GPIO 2` | Pembaca suhu dan kelembapan |
| **Relay Actuator** | Modul Relay 1-Channel (Active-HIGH) | `GPIO 3` | Sakelar kipas exhaust 12V |
| **LED Hijau** | LED 5mm + Resistor 220Ω | `GPIO 4` | Indikator Udara Bersih |
| **LED Kuning** | LED 5mm + Resistor 220Ω | `GPIO 5` | Indikator Udara Sedang |
| **LED Merah** | LED 5mm + Resistor 220Ω | `GPIO 6` | Indikator Polusi Buruk / Bahaya |
| **Exhaust Fan** | Kipas DC 12V + Power Supply 12V | Eksternal | Sirkulasi & pembuangan udara kotor |

---

## 📡 Struktur Topik MQTT

Sistem menggunakan hirarki topik standar untuk mendukung multi-alat:

```text
smart-purifier/
 └── {device_id}/
      ├── status    --> [ESP32 -> Broker] Telemetri JSON berkala
      ├── control   --> [Web -> ESP32] Kontrol relay kipas & mode (auto/manual)
      └── config    --> [Web -> ESP32] Kalibrasi offset & ambang batas sensor

