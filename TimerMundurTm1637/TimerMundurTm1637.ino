// TimerMundurTm1637.ino — Timer mundur MM:SS dengan modul TM1637
//
// ESP32 DevKit menghitung mundur dari waktu yang diatur lewat tiga
// tombol, menampilkan MM:SS di modul 7-segmen TM1637, dan membunyikan
// buzzer aktif saat hitungan mencapai nol. Tombol START/STOP menjeda
// dan melanjutkan hitungan; tahan tombol itu 1,5 detik untuk kembali
// ke mode atur (reset) dengan waktu yang sama seperti sebelum jalan.
//
// Library yang dipakai:
//   - TM1637 1.2.0 (Avishorp) — driver modul 4 digit 7-segmen TM1637.
//     Pasang lewat Library Manager Arduino IDE / arduino-cli:
//     arduino-cli lib install "TM1637"
//     (header yang dipakai: TM1637Display.h, kelas TM1637Display)
//
// Perilaku tombol (semua aktif LOW, INPUT_PULLUP):
//   - SET menit  : tiap tekan +1 menit (0..99, lalu kembali ke 0).
//   - SET detik  : tiap tekan +1 detik (0..59, lalu kembali ke 0).
//   - START/STOP : tekan singkat = mulai / jeda / lanjut.
//                  tahan 1,5 detik saat jeda atau selesai = reset ke
//                  waktu yang diatur (mode atur).
//   - Saat timer selesai: buzzer bunyi 3 kali, layar berkedip 00:00.
//     Tekan SET menit atau SET detik untuk kembali mengatur waktu.

#include <TM1637Display.h>

// ---------------- Pin ----------------
const uint8_t PIN_TM1637_CLK = 18;    // CLK modul TM1637
const uint8_t PIN_TM1637_DIO = 19;    // DIO modul TM1637
const uint8_t PIN_TOMBOL_MENIT = 32;  // SET menit, aktif LOW
const uint8_t PIN_TOMBOL_DETIK = 33;  // SET detik, aktif LOW
const uint8_t PIN_TOMBOL_START = 27;  // START/STOP, aktif LOW
const uint8_t PIN_BUZZER = 25;        // buzzer aktif, HIGH = bunyi

// ---------------- Konstanta waktu ----------------
const uint8_t MENIT_MAKSIMAL = 99;      // menit tertinggi yang bisa diatur
const uint8_t DETIK_MAKSIMAL = 59;      // detik tertinggi yang bisa diatur
const uint8_t DETIK_PER_MENIT = 60;     // 60 detik = 1 menit
const uint8_t KECERAHAN_LAYAR = 7;      // 0..7, 7 = paling terang
const uint8_t BIT_TITIK_DUA = 0x40;     // bit colon (tengah) TM1637

// ---------------- Konstanta tombol ----------------
const unsigned long DEBOUNCE_TOMBOL_MS = 50;    // abaikan pantulan tombol
const unsigned long TAHAN_RESET_MS = 1500;      // tahan START = reset
const unsigned long JEDA_ANTAR_BUNYI_MS = 300;  // jeda antar bunyi selesai
const uint8_t JUMLAH_BUNYI_SELESAI = 3;         // buzzer bunyi 3 kali

// ---------------- Konstanta hitung mundur ----------------
const unsigned long INTERVAL_TIK_MS = 1000;  // 1 detik per hitungan
const unsigned long INTERVAL_STATUS_MS = 1000;  // status Serial tiap 1 detik
const unsigned long BAUD_SERIAL = 115200;

// ---------------- Status timer ----------------
enum StatusTimer : uint8_t {
  STATUS_ATUR,     // mengatur menit/detik, timer belum jalan
  STATUS_JALAN,    // hitung mundur berjalan
  STATUS_JEDA,     // hitung mundur dijeda
  STATUS_SELESAI,  // hitungan nol, buzzer sudah bunyi
};

TM1637Display layar(PIN_TM1637_CLK, PIN_TM1637_DIO);

StatusTimer status = STATUS_ATUR;
uint8_t menitDiatur = 5;    // waktu yang diatur (menit), awal 05:00
uint8_t detikDiatur = 0;    // waktu yang diatur (detik)
uint16_t sisaDetik = 0;     // sisa hitungan dalam detik
unsigned long waktuTikTerakhir = 0;      // millis() saat detik terakhir berkurang
unsigned long waktuStatusTerakhir = 0;   // millis() status terakhir dikirim

// Status tombol (debounce + deteksi tekan singkat / tahan).
struct StatusTombol {
  uint8_t pin;
  bool ditekanSebelumnya;      // level LOW yang stabil sebelumnya
  unsigned long waktuPerubahan;  // millis() saat level stabil terakhir berubah
  unsigned long waktuTekanMulai;  // millis() saat tombol mulai ditekan
  bool tahanDiproses;          // tahan sudah diproses (jangan ulang)
};

StatusTombol tombolMenit = {PIN_TOMBOL_MENIT, false, 0, 0, false};
StatusTombol tombolDetik = {PIN_TOMBOL_DETIK, false, 0, 0, false};
StatusTombol tombolStart = {PIN_TOMBOL_START, false, 0, 0, false};

// ---------------- Tampilan ----------------
// Tampilkan menit:detik di TM1637. Titik dua menyala saat jalan,
// berkedip saat selesai, mati saat mengatur.
void tampilkanWaktu(uint8_t menit, uint8_t detik, bool titikDuaNyala) {
  const uint16_t angka = menit * DETIK_PER_MENIT + detik;
  const uint8_t titik = titikDuaNyala ? BIT_TITIK_DUA : 0;
  layar.showNumberDecEx(angka, titik, true, 4, 0);
}

void tampilkanSisa() {
  const uint8_t menit = sisaDetik / DETIK_PER_MENIT;
  const uint8_t detik = sisaDetik % DETIK_PER_MENIT;
  tampilkanWaktu(menit, detik, status == STATUS_JALAN);
}

void tampilkanWaktuDiatur() {
  tampilkanWaktu(menitDiatur, detikDiatur, false);
}

// ---------------- Buzzer ----------------
void bunyiBuzzer(unsigned long durasiMs) {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(durasiMs);
  digitalWrite(PIN_BUZZER, LOW);
}

// Bunyi 3 kali saat timer selesai, layar berkedip 00:00.
void bunyiSelesai() {
  for (uint8_t i = 0; i < JUMLAH_BUNYI_SELESAI; i++) {
    bunyiBuzzer(JEDA_ANTAR_BUNYI_MS);
    tampilkanWaktu(0, 0, false);
    delay(JEDA_ANTAR_BUNYI_MS);
    tampilkanWaktu(0, 0, true);
    delay(JEDA_ANTAR_BUNYI_MS);
  }
}

// ---------------- Tombol ----------------
// Perbarui satu tombol; kembalikan true bila baru saja ditekan singkat
// (dilepas sebelum batas tahan). Tahan panjang diproses di sini juga.
bool perbaruiTombol(StatusTombol &tombol, bool saatTahan) {
  const bool ditekan = digitalRead(tombol.pin) == LOW;
  const unsigned long sekarang = millis();
  bool tekanSingkat = false;

  if (ditekan != tombol.ditekanSebelumnya) {
    // Level berubah — tunggu sampai stabil (debounce).
    if (sekarang - tombol.waktuPerubahan >= DEBOUNCE_TOMBOL_MS) {
      tombol.ditekanSebelumnya = ditekan;
      tombol.waktuPerubahan = sekarang;
      if (ditekan) {
        tombol.waktuTekanMulai = sekarang;
        tombol.tahanDiproses = false;
      } else if (!tombol.tahanDiproses) {
        // Dilepas sebelum batas tahan = tekan singkat.
        tekanSingkat = true;
      }
    }
  } else if (ditekan && !tombol.tahanDiproses &&
             sekarang - tombol.waktuTekanMulai >= TAHAN_RESET_MS) {
    tombol.tahanDiproses = true;
    if (saatTahan) {
      resetKeModeAtur();
    }
  }
  return tekanSingkat;
}

// Kembali ke mode atur dengan waktu yang sama seperti sebelum jalan.
void resetKeModeAtur() {
  status = STATUS_ATUR;
  sisaDetik = menitDiatur * DETIK_PER_MENIT + detikDiatur;
  tampilkanWaktuDiatur();
  Serial.println("Timer direset ke waktu yang diatur.");
}

// ---------------- Logika tombol ----------------
void tanganiTombolMenit() {
  if (perbaruiTombol(tombolMenit, false)) {
    if (status == STATUS_SELESAI) {
      status = STATUS_ATUR;
    }
    if (status == STATUS_ATUR) {
      menitDiatur = (menitDiatur >= MENIT_MAKSIMAL) ? 0 : menitDiatur + 1;
      tampilkanWaktuDiatur();
      Serial.print("Menit diatur: ");
      Serial.println(menitDiatur);
    }
  }
}

void tanganiTombolDetik() {
  if (perbaruiTombol(tombolDetik, false)) {
    if (status == STATUS_SELESAI) {
      status = STATUS_ATUR;
    }
    if (status == STATUS_ATUR) {
      detikDiatur = (detikDiatur >= DETIK_MAKSIMAL) ? 0 : detikDiatur + 1;
      tampilkanWaktuDiatur();
      Serial.print("Detik diatur: ");
      Serial.println(detikDiatur);
    }
  }
}

void tanganiTombolStart() {
  if (perbaruiTombol(tombolStart, true)) {
    switch (status) {
      case STATUS_ATUR:
        if (menitDiatur == 0 && detikDiatur == 0) {
          Serial.println("Waktu masih 00:00 — atur dulu sebelum mulai.");
          return;
        }
        sisaDetik = menitDiatur * DETIK_PER_MENIT + detikDiatur;
        status = STATUS_JALAN;
        waktuTikTerakhir = millis();
        tampilkanSisa();
        Serial.println("Timer mulai.");
        break;
      case STATUS_JALAN:
        status = STATUS_JEDA;
        tampilkanSisa();
        Serial.println("Timer dijeda.");
        break;
      case STATUS_JEDA:
        status = STATUS_JALAN;
        waktuTikTerakhir = millis();
        tampilkanSisa();
        Serial.println("Timer lanjut.");
        break;
      case STATUS_SELESAI:
        resetKeModeAtur();
        break;
    }
  }
}

// ---------------- Hitung mundur ----------------
void hitungMundur() {
  if (status != STATUS_JALAN) {
    return;
  }
  const unsigned long sekarang = millis();
  if (sekarang - waktuTikTerakhir < INTERVAL_TIK_MS) {
    return;
  }
  waktuTikTerakhir = sekarang;
  if (sisaDetik > 0) {
    sisaDetik--;
  }
  tampilkanSisa();
  if (sisaDetik == 0) {
    status = STATUS_SELESAI;
    Serial.println("Waktu habis!");
    bunyiSelesai();
    tampilkanWaktu(0, 0, true);
  }
}

// Tampilkan status MM:SS di Serial Monitor tiap 1 detik.
void kirimStatusSerial() {
  const unsigned long sekarang = millis();
  if (sekarang - waktuStatusTerakhir < INTERVAL_STATUS_MS) {
    return;
  }
  waktuStatusTerakhir = sekarang;
  const uint8_t menit = sisaDetik / DETIK_PER_MENIT;
  const uint8_t detik = sisaDetik % DETIK_PER_MENIT;
  Serial.print("Status: ");
  Serial.print(status);
  Serial.print("  Sisa: ");
  if (menit < 10) {
    Serial.print('0');
  }
  Serial.print(menit);
  Serial.print(':');
  if (detik < 10) {
    Serial.print('0');
  }
  Serial.println(detik);
}

void setup() {
  Serial.begin(BAUD_SERIAL);
  pinMode(PIN_TOMBOL_MENIT, INPUT_PULLUP);
  pinMode(PIN_TOMBOL_DETIK, INPUT_PULLUP);
  pinMode(PIN_TOMBOL_START, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  layar.setBrightness(KECERAHAN_LAYAR);
  sisaDetik = menitDiatur * DETIK_PER_MENIT + detikDiatur;
  tampilkanWaktuDiatur();

  Serial.println("Timer Mundur TM1637 siap.");
  Serial.println("Atur menit/detik, tekan START untuk mulai.");
}

void loop() {
  tanganiTombolMenit();
  tanganiTombolDetik();
  tanganiTombolStart();
  hitungMundur();
  kirimStatusSerial();
}
