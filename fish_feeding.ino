/*
 * ============================================
 * SMART FISH FEEDING SYSTEM dengan ESP32
 * ============================================
 * Versi MODIFIKASI: Logika sensor pH dan TDS (PPM) diambil dari hidro_1.ino.
 * Ditambahkan Kalibrasi & Penyimpanan NVS.
 * * MODIFIKASI TERBARU:
 * - Ditambahkan Blower pada BLOWER_PIN (Pin 19)
 * - Blower akan menyala selama BLOWER_DURATION (5 detik) setiap kali 
 * fungsi dispenseFeed() dipanggil (baik manual atau otomatis).
 */

// ============================================
// KONFIGURASI BLYNK IoT (HARUS PALING ATAS!)
// ============================================
#define BLYNK_TEMPLATE_ID "TMPL67fGjS_6t"
#define BLYNK_TEMPLATE_NAME "Smart Feeding IoT"
#define BLYNK_DEVICE_NAME "Smart_Feeding_001"
#define BLYNK_AUTH_TOKEN "z3uKQfMErQW92uMgMzF2umXf_lLa8LF7"

// #define BLYNK_PRINT Serial  // Aktifkan jika ingin melihat log Blynk

// ============================================
// LIBRARY
// ============================================
#include <WiFi.h> 
#include <BlynkSimpleEsp32.h> 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <WidgetRTC.h>
#include <NewPing.h>
#include <Preferences.h> // Library BARU untuk menyimpan Kalibrasi/Pengaturan

// ============================================
// KONFIGURASI NVS (NON-VOLATILE STORAGE)
// ============================================
#define NVS_NAMESPACE "smart_feed"
#define KEY_ACID_VOLT "acidVolt"
#define KEY_NEUTRAL_VOLT "neutralVolt"
#define KEY_ALKALINE_VOLT "alkalineVolt"
#define KEY_TDS_KOREKSI "koreksiTDS"

// ============================================
// KONFIGURASI WIFI
// ============================================
const char* ssid = "Lab IoT Studio";
const char* pass = "Tult0612_labiottel-U"; 

// ============================================
// PIN CONFIGURATION (GPIO)
// ============================================
#define PH_SENSOR_PIN     4
#define TDS_SENSOR_PIN    5
#define TEMP_SENSOR_PIN   6
#define SERVO_PIN         1
#define ULTRASONIC_TRIG   17   // JSN-SR04T Trigger
#define ULTRASONIC_ECHO   16   // JSN-SR04T Echo
#define BLOWER_PIN        19  // <-- [BARU] Pin untuk Blower

// ============================================
// KONFIGURASI SENSOR & KALIBRASI (Nilai Default)
// ============================================
// Nilai Default Kalibrasi pH (Diambil dari default hidro_1.ino)
float PH_ACID_VOLTAGE = 1900.94;    // pH 4.0
float PH_NEUTRAL_VOLTAGE = 1408.82;  // pH 7.0
float PH_ALKALINE_VOLTAGE = 1000.0; // pH 10.0

// Nilai Default Kalibrasi TDS (Diambil dari default hidro_1.ino)
#define TDS_VREF 3.30
#define TDS_SAMPLE_COUNT 30
float TDS_CORRECTION_FACTOR = 1.0; 

int tdsAnalogBuffer[TDS_SAMPLE_COUNT];
int tdsBufferTemp[TDS_SAMPLE_COUNT];
int tdsAnalogBufferIndex = 0;
unsigned long lastTdsSampleTime = 0;

// ============================================
// KONFIGURASI ULTRASONIC (NewPing)
// ============================================
#define MAX_DISTANCE 600
NewPing sonar(ULTRASONIC_TRIG, ULTRASONIC_ECHO, MAX_DISTANCE);

// ============================================
// KONFIGURASI SERVO & BLOWER
// ============================================
const int SERVO_OPEN_ANGLE = 120;
const int SERVO_CLOSE_ANGLE = 0;
const int SERVO_OPEN_DURATION = 1000; // 1 detik
const int BLOWER_DURATION = 5000;     // <-- [BARU] 5 detik

// ============================================
// KONFIGURASI JADWAL PEMBERIAN PAKAN
// ============================================
const int FEEDING_HOURS[] = {9, 15, 21};
const int NUM_FEEDING_TIMES = 3;
bool feedingCompleted[NUM_FEEDING_TIMES] = {false, false, false};

// ============================================
// VIRTUAL PINS BLYNK
// ============================================
#define VPIN_PH           V1
#define VPIN_TDS          V2
#define VPIN_TEMPERATURE  V3
#define VPIN_DISTANCE     V4
#define VPIN_MANUAL_FEED  V5
#define VPIN_STATUS       V6

// ============================================
// OBJEK GLOBAL
// ============================================
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);
Servo feedingServo; 
BlynkTimer timer;
WidgetRTC rtc;
Preferences preferences; // Objek Preferences BARU

// Variabel untuk menyimpan nilai sensor (diperbarui secara kontinu)
float currentPhValue = 7.0;
float currentTdsValue = 0.0;
float currentTemperature = 25.0;

// ============================================
// DEKLARASI FUNGSI
// ============================================
void connectWiFi();
void initializeSensors();
void dispenseFeed();
void checkFeedingSchedule();
void sendSensorData();
void measureContinuousPH_TDS(); // BARU: Pembacaan sensor non-blok
float measureTemperature();
float measureDistance();
float readPHVoltage(); // Mengambil 100 sampel (untuk kalibrasi)
float readRawTDS();    // Mengambil 30 sampel (untuk kalibrasi)
int getMedianValue(int arr[], int size);
void handleCalibrationCommand(); // BARU: Handler perintah serial
void loadCalibration(); // BARU: Memuat kalibrasi dari NVS
void savePhCalibration(); // BARU: Menyimpan kalibrasi pH ke NVS
void saveTdsCalibration(); // BARU: Menyimpan kalibrasi TDS ke NVS

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("Smart Fish Feeding System (Optimized + Blower)");
  Serial.println("=================================");

  // Muat Nilai Kalibrasi
  loadCalibration(); 

  // Inisialisasi Servo
  feedingServo.attach(SERVO_PIN);
  feedingServo.write(SERVO_CLOSE_ANGLE);
  Serial.println("[OK] Servo initialized");

  // <-- [BARU] Inisialisasi Blower
  pinMode(BLOWER_PIN, OUTPUT);
  digitalWrite(BLOWER_PIN, LOW); // Pastikan mati saat awal
  Serial.println("[OK] Blower initialized");

  // Inisialisasi Sensor
  initializeSensors();

  // Koneksi WiFi & Blynk
  connectWiFi();
  Blynk.config(BLYNK_AUTH_TOKEN);
  while (Blynk.connect() == false) {}
  Serial.println("[OK] Blynk connected");
  
  // Sinkronisasi RTC
  rtc.begin();
  Serial.println("[OK] RTC synchronized");
  
  // Setup Timer
  timer.setInterval(40L, measureContinuousPH_TDS); // Pembacaan sensor kontinu 
  timer.setInterval(3000L, sendSensorData); // Kirim data sensor ke Blynk/Serial
  timer.setInterval(60000L, checkFeedingSchedule);
  
  Blynk.virtualWrite(VPIN_STATUS, "Sistem Aktif");
  Serial.println("=================================\n");
}

// ============================================
// LOOP
// ============================================
void loop() {
  Blynk.run(); 
  timer.run();
  handleCalibrationCommand(); // <-- Cek perintah serial untuk kalibrasi
}

// ============================================
// FUNGSI KONEKSI WIFI
// ============================================
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, pass);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" [OK]");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" [FAILED]");
  }
}

// ============================================
// FUNGSI INISIALISASI SENSOR
// ============================================
void initializeSensors() {
  tempSensor.begin();
  Serial.println("[OK] Sensors initialized");
}

// ============================================
// FUNGSI PEMBERIAN PAKAN (DENGAN BLOWER)
// ============================================
void dispenseFeed() {
  Serial.println("\n>>> FEEDING STARTED <<<");
  if (Blynk.connected()) {
    Blynk.logEvent("feed_given", "Pakan telah diberikan!");
    Blynk.virtualWrite(VPIN_STATUS, "Memberi Pakan...");
  }

  // <-- [BARU] 1. Aktifkan Blower & Servo
  Serial.println("Blower ON, Servo Open");
  digitalWrite(BLOWER_PIN, HIGH);
  feedingServo.write(SERVO_OPEN_ANGLE);
  
  // 2. Tunggu durasi servo terbuka (mis: 1 detik)
  delay(SERVO_OPEN_DURATION);
  
  // 3. Tutup Servo
  Serial.println("Servo Close");
  feedingServo.write(SERVO_CLOSE_ANGLE);

  // 4. Hitung sisa waktu blower
  // (Total durasi blower 5 detik) - (Waktu servo sudah berjalan 1 detik) = 4 detik
  long remainingBlowerTime = BLOWER_DURATION - SERVO_OPEN_DURATION;
  
  if (remainingBlowerTime > 0) {
     delay(remainingBlowerTime); // Tunggu sisa waktu blower
  }

  // 5. Matikan Blower
  Serial.println("Blower OFF");
  digitalWrite(BLOWER_PIN, LOW);
  
  if (Blynk.connected()) {
    Blynk.virtualWrite(VPIN_STATUS, "Pemberian Pakan Selesai");
  }
  
  Serial.println(">>> FEEDING COMPLETED <<<\n");
}

// ============================================
// BLYNK: Tombol Pakan Manual
// ============================================
BLYNK_WRITE(VPIN_MANUAL_FEED) {
  int buttonState = param.asInt();
  if (buttonState == 1) {
    Serial.println("[MANUAL] Feed button pressed");
    dispenseFeed();
  }
}

// ============================================
// FUNGSI CEK JADWAL PAKAN OTOMATIS
// ============================================
void checkFeedingSchedule() {
  if (!Blynk.connected()) return;

  int currentHour = hour();
  int currentMinute = minute();

  if (currentHour == 0 && currentMinute == 1) {
    for (int i = 0; i < NUM_FEEDING_TIMES; i++) {
      feedingCompleted[i] = false;
    }
    Blynk.virtualWrite(VPIN_STATUS, "Status pakan direset");
    Serial.println("[SCHEDULE] Daily reset");
  }

  for (int i = 0; i < NUM_FEEDING_TIMES; i++) {
    if (currentHour == FEEDING_HOURS[i] && 
        currentMinute <= 1 && 
        !feedingCompleted[i]) {
      
      Serial.print("[AUTO] Feeding at ");
      Serial.print(FEEDING_HOURS[i]);
      Serial.println(":00");
      
      dispenseFeed();
      feedingCompleted[i] = true;
      
      Blynk.logEvent("feed_auto", 
        "Pakan terjadwal " + String(FEEDING_HOURS[i]) + ":00");
    }
  }
}

// ============================================
// FUNGSI CONTINUOUS SENSOR READING (Non-Blok)
// ============================================
void measureContinuousPH_TDS() {
  // 1. Baca Suhu
  float tempRead = measureTemperature();
  if (tempRead != DEVICE_DISCONNECTED_C) {
    currentTemperature = tempRead;
  }
  
  // 2. Baca pH (Setiap 1 detik)
  static unsigned long phTimepoint = millis();
  if(millis() - phTimepoint > 1000U){
    phTimepoint = millis();
    float voltage = analogRead(PH_SENSOR_PIN)/4095.0*3300.0; // 12-bit ADC ke mV
    
    if (voltage >= PH_NEUTRAL_VOLTAGE) { 
      // pH 7.0 ke 4.0 (Tegangan Turun, pH Turun)
      float slope = (7.0 - 4.0) / (PH_NEUTRAL_VOLTAGE - PH_ACID_VOLTAGE);
      currentPhValue = 7.0 + slope * (voltage - PH_NEUTRAL_VOLTAGE);
    } else { 
      // pH 7.0 ke 10.0 (Tegangan Turun, pH Naik)
      float slope = (10.0 - 7.0) / (PH_ALKALINE_VOLTAGE - PH_NEUTRAL_VOLTAGE);
      currentPhValue = 7.0 + slope * (voltage - PH_NEUTRAL_VOLTAGE);
    }
    currentPhValue = constrain(currentPhValue, 0.0, 14.0);
  }

  // 3. Sampling TDS (Setiap 40ms)
  if(millis() - lastTdsSampleTime > 40U) { 
    lastTdsSampleTime = millis();
    tdsAnalogBuffer[tdsAnalogBufferIndex] = analogRead(TDS_SENSOR_PIN); 
    tdsAnalogBufferIndex = (tdsAnalogBufferIndex + 1) % TDS_SAMPLE_COUNT;
  }
  
  // Hitung TDS (Setiap 800ms)
  static unsigned long tdsCalcTimepoint = millis();
  if(millis() - tdsCalcTimepoint > 800U) {
    tdsCalcTimepoint = millis();
    
    for(int copyIndex = 0; copyIndex < TDS_SAMPLE_COUNT; copyIndex++) {
      tdsBufferTemp[copyIndex] = tdsAnalogBuffer[copyIndex];
    }

    int rawMedian = getMedianValue(tdsBufferTemp, TDS_SAMPLE_COUNT);
    float averageVoltage = rawMedian * (float)TDS_VREF / 4096.0; // Konversi ADC ke Volt
    
    // Kompensasi Suhu
    float compensationCoefficient = 1.0 + 0.02 * (currentTemperature - 25.0);
    float compensationVolatge = averageVoltage / compensationCoefficient; 
    
    // Rumus Polinomial TDS
    currentTdsValue = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 
                       255.86 * compensationVolatge * compensationVolatge + 
                       857.39 * compensationVolatge) * 0.5;
    
    // Koreksi Kalibrasi
    currentTdsValue = currentTdsValue * TDS_CORRECTION_FACTOR;
    currentTdsValue = constrain(currentTdsValue, 0.0, 5000.0);
  }
}

// ============================================
// FUNGSI KIRIM DATA SENSOR
// ============================================
void sendSensorData() {
  if (!Blynk.connected()) return;

  float distance = measureDistance();

  // Kirim nilai global
  Blynk.virtualWrite(VPIN_PH, currentPhValue);
  Blynk.virtualWrite(VPIN_TDS, round(currentTdsValue));
  Blynk.virtualWrite(VPIN_TEMPERATURE, currentTemperature);
  Blynk.virtualWrite(VPIN_DISTANCE, distance);

  Serial.println("--- SENSOR READINGS ---");
  Serial.print("pH: "); Serial.print(currentPhValue, 2);
  Serial.print(" | TDS: "); Serial.print(currentTdsValue, 0); Serial.print(" ppm");
  Serial.print(" | Temp: ");
  Serial.print(currentTemperature, 1); Serial.print("°C");
  Serial.print(" | Distance: ");
  if (distance < 0) {
    Serial.println("Out of range");
  } else {
    Serial.print(distance, 0); Serial.println(" cm");
  }
  Serial.println("-----------------------\n");
}

// ============================================
// FUNGSI PENGUKURAN SUHU
// ============================================
float measureTemperature() {
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  if (temp == DEVICE_DISCONNECTED_C) {
    return DEVICE_DISCONNECTED_C; 
  }
  return temp;
}

// ============================================
// FUNGSI PENGUKURAN JARAK (NewPing)
// ============================================
float measureDistance() {
  unsigned int distance = sonar.ping_cm();
  if (distance == 0) {
    return -1.0; 
  }
  return (float)distance;
}

// ============================================
// FUNGSI MEDIAN FILTER (Diambil dari hidro_1.ino)
// ============================================
int getMedianValue(int bArray[], int iFilterLen) {
    int bTab[iFilterLen];
    for (byte i = 0; i < iFilterLen; i++) {
        bTab[i] = bArray[i];
    }
    int i, j, bTemp;
    for (j = 0; j < iFilterLen - 1; j++) {
        for (i = 0; i < iFilterLen - j - 1; i++) {
            if (bTab[i] > bTab[i + 1]) {
                bTemp = bTab[i];
                bTab[i] = bTab[i + 1];
                bTab[i + 1] = bTemp;
            }
        }
    }
    if ((iFilterLen & 1) > 0) {
        bTemp = bTab[(iFilterLen - 1) / 2];
    } else {
        bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
    }
    return bTemp;
}


// ============================================
// FUNGSI BACAAN KHUSUS UNTUK KALIBRASI (Bloking)
// ============================================

// Membaca tegangan PH dengan 100 sampel rata-rata (Blocking)
float readPHVoltage() {
  long totalValue = 0;
  const int samples = 100;
  Serial.print("Membaca voltase pH (100 sampel)... ");
  for(int i = 0; i < samples; i++) {
    totalValue += analogRead(PH_SENSOR_PIN);
    delay(2);
  }
  Serial.println("Selesai.");
  float avgValue = (float)totalValue / samples;
  // Konversi ke mV (4095 = 12-bit, 3300 = 3.3V VREF)
  float voltage_mV = (avgValue / 4095.0) * 3300.0;
  return voltage_mV;
}

// Membaca nilai TDS mentah (Bloking)
float readRawTDS() {
  Serial.print("Membaca voltase TDS (30 sampel)... ");
  for(int i=0; i < TDS_SAMPLE_COUNT; i++) {
    tdsAnalogBuffer[i] = analogRead(TDS_SENSOR_PIN);
    delay(5);
  }
  Serial.println("Selesai mengisi buffer.");
  
  for(int idx=0; idx<TDS_SAMPLE_COUNT; idx++) {
    tdsBufferTemp[idx]= tdsAnalogBuffer[idx];
  }
  
  float temp = measureTemperature();
  if (temp == DEVICE_DISCONNECTED_C) {
    Serial.println("Peringatan: Suhu tidak terdeteksi, pakai 25.0 C");
    temp = 25.0;
  }
  
  int rawMedian = getMedianValue(tdsBufferTemp, TDS_SAMPLE_COUNT);
  float avgVolt = rawMedian * (float)TDS_VREF / 4096.0;
  
  float compCoeff = 1.0 + 0.02 * (temp - 25.0);
  float compVolt = avgVolt / compCoeff;
  
  // Rumus Polinomial TDS (Tanpa Faktor Koreksi)
  float calculatedTds = (133.42*compVolt*compVolt*compVolt - 255.86*compVolt*compVolt + 857.39*compVolt)*0.5;
  return calculatedTds;
}

// ============================================
// FUNGSI PENYIMPANAN & PEMUATAN KALIBRASI (NVS)
// ============================================
void loadCalibration() {
  preferences.begin(NVS_NAMESPACE, true); // Read-only
  PH_ACID_VOLTAGE = preferences.getFloat(KEY_ACID_VOLT, PH_ACID_VOLTAGE);
  PH_NEUTRAL_VOLTAGE = preferences.getFloat(KEY_NEUTRAL_VOLT, PH_NEUTRAL_VOLTAGE);
  PH_ALKALINE_VOLTAGE = preferences.getFloat(KEY_ALKALINE_VOLT, PH_ALKALINE_VOLTAGE);
  TDS_CORRECTION_FACTOR = preferences.getFloat(KEY_TDS_KOREKSI, TDS_CORRECTION_FACTOR);
  preferences.end();
  Serial.println("Pengaturan Kalibrasi dimuat dari NVS.");
}

void savePhCalibration() {
  preferences.begin(NVS_NAMESPACE, false); // Read-write
  preferences.putFloat(KEY_ACID_VOLT, PH_ACID_VOLTAGE);
  preferences.putFloat(KEY_NEUTRAL_VOLT, PH_NEUTRAL_VOLTAGE);
  preferences.putFloat(KEY_ALKALINE_VOLT, PH_ALKALINE_VOLTAGE);
  preferences.end();
  Serial.println("[OK] Kalibrasi pH berhasil DISIMPAN ke NVS.");
}

void saveTdsCalibration() {
  preferences.begin(NVS_NAMESPACE, false);
  preferences.putFloat(KEY_TDS_KOREKSI, TDS_CORRECTION_FACTOR);
  preferences.end();
  Serial.println("[OK] Kalibrasi TDS berhasil DISIMPAN ke NVS.");
}

void printCalibrationStatus() {
  Serial.println("--- Nilai Kalibrasi Saat Ini ---");
  Serial.printf("  pH 4.0 Volt (Acid): %.2f mV\n", PH_ACID_VOLTAGE);
  Serial.printf("  pH 7.0 Volt (Neutral): %.2f mV\n", PH_NEUTRAL_VOLTAGE);
  Serial.printf("  pH 10.0 Volt (Alkaline): %.2f mV\n", PH_ALKALINE_VOLTAGE);
  Serial.printf("  TDS Correction Factor: %.4f\n", TDS_CORRECTION_FACTOR);
  Serial.println("--------------------------------");
}

// ============================================
// FUNGSI HANDLER PERINTAH SERIAL
// ============================================
void handleCalibrationCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n'); command.trim();
    Serial.print("Perintah Serial Diterima: "); Serial.println(command);
    
    if (command.equals("help")) {
        Serial.println("--- Perintah Kalibrasi ---");
        Serial.println("status     : Tampilkan nilai kalibrasi saat ini");
        Serial.println("ph4        : Kalibrasi pH 4.0 (celupkan di buffer 4.0)");
        Serial.println("ph7        : Kalibrasi pH 7.0 (celupkan di buffer 7.0)");
        Serial.println("ph10       : Kalibrasi pH 10.0 (celupkan di buffer 10.0)");
        Serial.println("tdsXXXX    : Kalibrasi TDS (mis: tds1382) (celupkan di larutan standar)");
        Serial.println("--------------------------");
    }
    else if (command.equals("status")) {
        printCalibrationStatus();
    }
    else if (command.equals("ph4")) { 
        float newVolt = readPHVoltage();
        PH_ACID_VOLTAGE = newVolt; 
        savePhCalibration(); 
        Serial.printf("-> PH_ACID_VOLTAGE (pH 4.0) diset ke: %.2f mV\n", newVolt);
    }
    else if (command.equals("ph7")) { 
        float newVolt = readPHVoltage();
        PH_NEUTRAL_VOLTAGE = newVolt; 
        savePhCalibration();
        Serial.printf("-> PH_NEUTRAL_VOLTAGE (pH 7.0) diset ke: %.2f mV\n", newVolt);
    }
    else if (command.equals("ph10")) { 
        float newVolt = readPHVoltage();
        PH_ALKALINE_VOLTAGE = newVolt; 
        savePhCalibration();
        Serial.printf("-> PH_ALKALINE_VOLTAGE (pH 10.0) diset ke: %.2f mV\n", newVolt);
    }
    else if (command.startsWith("tds")) {
        float targetPPM = command.substring(3).toFloat();
        if (targetPPM > 0) {
            Serial.printf("Memulai Kalibrasi TDS ke %0.f ppm...\n", targetPPM);
            float rawTDS = readRawTDS();
            Serial.printf("Bacaan mentah (sebelum koreksi): %.2f ppm\n", rawTDS);
            if (rawTDS > 0) {
                TDS_CORRECTION_FACTOR = targetPPM / rawTDS;
                saveTdsCalibration();
                Serial.printf("-> Faktor Koreksi baru: %.4f\n", TDS_CORRECTION_FACTOR);
                currentTdsValue = rawTDS * TDS_CORRECTION_FACTOR;
                Serial.printf("-> Nilai TDS terkoreksi saat ini: %.0f ppm\n", currentTdsValue);
            } else { Serial.println("Gagal! Nilai TDS mentah 0 atau negatif."); }
        } else { Serial.println("Format salah. Cth: tds1000"); }
    }
    else {
        Serial.println("Perintah tidak dikenal. Ketik 'help' untuk daftar perintah.");
    }
  }
}
