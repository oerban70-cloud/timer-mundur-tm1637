# Timer Mundur 7-Segmen TM1637 — ESP32 + 3 Tombol + Buzzer

Telur rebusmu sering kematangan dan jadi keras? Timer kecil ini solusinya.
Tiga tombol mengatur menit dan detik, ESP32 menghitung mundur tiap detik,
modul TM1637 menampilkan sisa waktu dalam format MM:SS, dan begitu angka
menyentuh 00:00 buzzer aktif berbunyi nyaring tiga kali sambil layar
berkedip — jadi kamu tidak perlu lagi menebak waktu dari jam dinding dan
terus melirik kompor. Tombol START/STOP menjeda dan melanjutkan hitungan;
tahan tombol itu 1,5 detik untuk kembali ke mode atur dengan waktu yang
sama seperti sebelum jalan.

Video Short (60 detik): https://www.youtube.com/shorts/mg_tIbh7ATg

## Cara kerja

1. Dua tombol SET (menit dan detik) menambah waktu yang diatur — menit
   0-99, detik 0-59, dan kembali ke 0 kalau sudah melewati batas atas.
   Semua tombol aktif LOW (INPUT_PULLUP), jadi tidak butuh resistor
   pull-up eksternal.
2. Tekan START/STOP untuk mulai. ESP32 menyimpan sisa waktu dalam detik
   dan menguranginya satu per satu tiap 1.000 ms memakai millis() —
   tanpa delay yang memblokir, jadi tombol tetap responsif selama
   hitungan berjalan.
3. Sisa waktu ditampilkan di modul TM1637 sebagai MM:SS (angka =
   menit x 100 + detik, titik dua di tengah menyala selama hitungan
   berjalan). Modul TM1637 memakai protokol 2 kabel mirip I2C (CLK +
   DIO) dan didorong library TM1637 1.2.0 (Avishorp).
4. Saat sisa mencapai nol, status berubah menjadi selesai: buzzer aktif
   berbunyi tiga kali (300 ms bunyi, 300 ms diam) dan layar berkedip
   menampilkan 00:00.
5. Tekan START/STOP saat jeda untuk melanjutkan, atau tahan 1,5 detik
   untuk reset kembali ke mode atur dengan waktu yang sama. Tekan tombol
   SET saat selesai untuk langsung mengatur waktu baru.
6. Status MM:SS dikirim ke Serial Monitor tiap 1 detik (baud 115200).

Kenapa hitung mundur pakai millis(), bukan delay? Kalau pakai delay 1
detik di dalam loop, tombol tidak bisa dibaca selama delay itu berjalan
— jeda dan reset akan terasa lambat atau terlewat. Dengan millis(),
loop tetap memeriksa tombol puluhan kali tiap detik, jadi jeda langsung
direspons dan debounce 50 ms tetap bekerja.

## Wiring

| Komponen | Pin modul | ESP32 |
| --- | --- | --- |
| Modul TM1637 CLK (jam) | CLK | GPIO18 |
| Modul TM1637 DIO (data) | DIO | GPIO19 |
| Modul TM1637 VCC | VCC | 3V3 |
| Modul TM1637 GND | GND | GND |
| Tombol SET menit kaki 1 (aktif LOW) | 1 | GPIO32 (INPUT_PULLUP) |
| Tombol SET menit kaki 2 | 2 | GND |
| Tombol SET detik kaki 1 (aktif LOW) | 1 | GPIO33 (INPUT_PULLUP) |
| Tombol SET detik kaki 2 | 2 | GND |
| Tombol START/STOP kaki 1 (aktif LOW) | 1 | GPIO27 (INPUT_PULLUP) |
| Tombol START/STOP kaki 2 | 2 | GND |
| Buzzer aktif (+) lewat resistor 220 ohm | + | GPIO25 |
| Buzzer aktif (-) | - | GND |
| ESP32 | USB | Adaptor USB 5V |

Skema lengkap (gaya Fritzing): lihat `schematic.png`.

PERHATIAN: modul TM1637 dicatu 3V3 dari ESP32 — jangan mencatu 5V ke
pin GPIO ESP32. Buzzer aktif dipasang seri resistor 220 ohm untuk
membatasi arus dari GPIO25. Waktu awal saat dinyalakan adalah 05:00;
atur dulu dengan tombol SET sebelum menekan START.

## Konfigurasi

Di `TimerMundurTm1637/TimerMundurTm1637.ino`:

- `PIN_TM1637_CLK` (18) / `PIN_TM1637_DIO` (19) — CLK dan DIO modul TM1637.
- `PIN_TOMBOL_MENIT` (32) / `PIN_TOMBOL_DETIK` (33) — tombol SET, aktif LOW.
- `PIN_TOMBOL_START` (27) — tombol START/STOP, aktif LOW.
- `PIN_BUZZER` (25) — buzzer aktif, HIGH = bunyi.
- `MENIT_MAKSIMAL` (99) / `DETIK_MAKSIMAL` (59) — batas atas pengaturan.
- `KECERAHAN_LAYAR` (7) — kecerahan TM1637, 0-7.
- `DEBOUNCE_TOMBOL_MS` (50) — abaikan pantulan tombol.
- `TAHAN_RESET_MS` (1500) — tahan START selama ini = reset ke mode atur.
- `JUMLAH_BUNYI_SELESAI` (3) / `JEDA_ANTAR_BUNYI_MS` (300) — pola bunyi selesai.
- `INTERVAL_TIK_MS` (1000) — satu detik per hitungan mundur.
- `BAUD_SERIAL` (115200) — status MM:SS di Serial Monitor tiap 1 detik.

## Komponen

- ESP32 DevKit
- Modul TM1637 4 digit 7-segmen
- 3x tombol tekan (push button)
- Buzzer aktif 5V + resistor 220 ohm
- Adaptor USB 5V (catu ESP32)

## Verifikasi compile

Terverifikasi compile dengan Arduino CLI 1.6.0, ESP32 core 4.0.0,
board `esp32:esp32:esp32`:

- Sketch memakai 294.373 byte (22%) dari program storage (maksimal 1.310.720 byte).
- Variabel global memakai 23.272 byte (7%) dari dynamic memory (maksimal 327.680 byte).
- Library eksternal: TM1637 1.2.0 (Avishorp) — lihat `libraries/TM1637.zip`.

## Lisensi

MIT — lihat `LICENSE`.
