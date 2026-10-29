# VanillaBTN

> **The Definitive, Authentic Native PC Port of Harvest Moon: Back to Nature**  
> *Preserving the true essence and childhood nostalgia of Mineral Town on modern hardware.*

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](#building)
[![Platform](https://img.shields.io/badge/platform-Windows%20x86--64-blue)](#quick-start)
[![Tests](https://img.shields.io/badge/tests-121%2F121%20passing%20(100%25)-success)](#building)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## 🌾 The VanillaBTN Manifesto

Banyak proyek dekompilasi dan modifikasi *Harvest Moon: Back to Nature* akhir-akhir ini disusupi beragam mod aneh, cheat agresif, maupun penambahan konten luar yang merusak keseimbangan dan esensi nostalgia game aslinya.

**VanillaBTN hadir untuk mengembalikan kemurnian tersebut.**

Prinsip kami sederhana dan tegas:
1. **100% Pure Vanilla Parity**: Logika simulasi, jadwal NPC, festival, bertani, beternak, dan event cerita berjalan persis seperti versi aslinya di PlayStation 1 (`SLUS_011.15`).
2. **Zero Bloat & No Game-Breaking Mods**: Tidak ada penambahan karakter asing, mod aneh, atau mekanik curang yang merusak atmosfer Mineral Town.
3. **Native 60 FPS Modern Engineering**: Bukan emulator dan bukan static recompiler lambat. Game ini dikompilasi secara native dari kode sumber C decompilation dengan implementasi clean-room runtime PS1 (GPU, SPU, GTE, Pad, Card, CD-ROM) yang berjalan sangat ringan dan mulus di PC modern.
4. **Tasteful & Optional Quality of Life (QoL)**: Setiap peningkatan teknis (widescreen, audio studio, stamina HUD, tool charge bar) dirancang elegan dengan gaya visual otentik dan **dapat diatur / dimatikan kapan saja** melalui menu jeda in-game (ESC).

---

## ✨ Fitur Utama

- **Single Window Native Launch**: Game langsung terbuka dalam satu jendela game mandiri tanpa jendela terminal hitam yang mengganggu.
- **Tampilan Grafis Modern**:
  - Dukungan resolusi native viewport, **16:9 Widescreen Mode**, dan Fullscreen adaptif.
  - **True 32-bit Micro-Dither Debanding**: Menghilangkan color banding pada langit malam, kabut pagi, dan gradasi senja.
  - Bilinear texture filtering & FXAA antialiasing opsional.
  - VSync & frame pacing presisi tanpa percepatan simulasi waktu game.
- **Studio-Quality Audio**:
  - Resampling audio beresolusi tinggi menggunakan algoritma *Catmull-Rom Cubic Spline* untuk nada treble yang renyah dan instrumen yang jernih.
  - Slider volume independen untuk Musik Latar (BGM) dan Efek Suara (SFX).
- **Kontrol Modern yang Fleksibel**:
  - Dukungan penuh Gamepad / Controller (Xbox, DualShock/DualSense, dsb).
  - Keyboard: **Preset A** (Panah + ZX) dan **Preset B** (WASD + JK).
  - Mode Mouse opsional untuk interaksi cepat.
- **In-Game Pause Interface (Tombol ESC)**:
  - Menu jeda berdesain otentik menggunakan font asli game untuk mengubah pengaturan Grafis, Kontrol, Audio, dan Gameplay secara langsung di tengah permainan.
- **Peningkatan QoL Elegan (Opsional - Default OFF)**:
  - **Stamina HUD**: Tampilan visual 10-hati di pojok kiri atas (otomatis sembunyi saat menu layar penuh).
  - **Tool Charge Bar**: Indikator pengisian tenaga cangkul, kapak, palu, sabit, dan siraman di atas kepala Pete dengan kode warna tingkatan ore.
  - **Tile Targeting Grid**: Garis proyeksi 3D petak tanah yang ditarget di kebun & greenhouse.
  - **Fast Text**: Teks dialog warga yang instan dan responsif.
  - Opsi berjalan menembus tanaman, pagar kayu tahan lama, dan bell cerdas hewan ternak.
- **Penyimpanan Save & Data Mandiri**:
  - Save game dan konfigurasi tersimpan rapi di folder `Documents\VanillaBTN\`.
  - Dilengkapi migrasi otomatis jika sebelumnya memiliki data di versi lama.
- **Crash Handler Andal**:
  - Dilengkapi dialog error native Windows yang memuat kode dan alamat memori fault jika terjadi kendala teknis.

---

## 🚀 Quick Start (Cara Menjalankan)

1. Unduh arsip rilis terbaru: **`VanillaBTN-windows-x64-v0.9.0.zip`** dari menu [Releases](https://github.com/kodeaqua/VanillaBTN/releases).
2. Ekstrak file ZIP ke folder pilihanmu di komputer.
3. Siapkan file disc image original *Harvest Moon: Back to Nature (USA)* milikmu sendiri (`.bin`/`.cue` atau `.iso`).
4. Klik ganda **`VanillaBTN.exe`**:
   - Pada peluncuran pertama, jika game meminta lokasi disc, pilih file `.bin` / `.cue` milikmu.
   - Lokasi disc akan diingat secara otomatis untuk sesi bermain selanjutnya.
5. Selamat menikmati Mineral Town dengan kenyamanan modern!

---

## 🎮 Kontrol Permainan

| Aksi | Gamepad | Keyboard (Preset A) | Keyboard (Preset B - WASD) |
|---|---|---|---|
| **Bergerak** | D-Pad / Left Stick | Tombol Panah | W / A / S / D |
| **Aksi / Bicara / Ambil** | Tombol A / Cross | Tombol X | Tombol K |
| **Gunakan Tool** | Tombol X / Square | Tombol Z | Tombol J |
| **Buka Tas / Rucksack** | Tombol Y / Triangle | Tombol W | Tombol I |
| **Makan / Simpan** | Tombol B / Circle | Tombol D | Tombol L |
| **Ganti Item / Tool** | Tombol L1 / R1 | Tombol S | Tombol U |
| **Panggil Kuda / Anjing** | Tombol L2 / R2 | Tombol A | Tombol O |
| **Lari Cepat** | Hold RB / R1 | Hold Shift / Otomatis | Hold Shift / Otomatis |
| **Pause Menu & Settings** | Tombol START | Tombol ESC | Tombol ESC |

---

## 🛠️ Building from Source (Windows)

VanillaBTN menggunakan toolchain native otomatis tanpa membutuhkan WSL, Docker, atau instalasi PSY-Q SDK.

### Prasyarat:
- Windows 10 atau 11 (64-bit).
- PowerShell 5.1 atau lebih baru.
- Git.

### Perintah Build:
Cukup jalankan satu perintah berikut di terminal:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release -Game
```

Skrip ini akan:
1. Menyediakan toolchain Clang/LLVM-MinGW, Ninja, dan CMake secara otomatis.
2. Mengompilasi resource icon sapi, Windows GUI subsystem, dan seluruh modul runtime game.
3. Menjalankan 121 unit tests otomatis untuk memastikan stabilitas 100%.
4. Mengeluarkan executable `VanillaBTN.exe` di `build/port/windows-release/bin/`.
5. Mengemas file mandiri dan README ke `dist/VanillaBTN-windows-x64-v0.9.0.zip`.

---

## ⚖️ Legal & Licensing

- Kode sumber native port VanillaBTN dilisensikan di bawah **[MIT License](LICENSE)**.
- Didasarkan pada hasil proyek dekompilasi *Harvest Moon: Back to Nature* yang berlisensi [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/) serta alat `maspsx` berlisensi [MIT](tools/maspsx/LICENSE).
- **Pemberitahuan Hak Cipta**: Proyek ini **tidak menyertakan** aset berhak cipta (audio CD, gambar tekstur, video STR, ROM/ISO) milik Victor Interactive Software / Marvelous Inc. Pemain wajib menggunakan salinan legal game original mereka sendiri untuk bermain.
