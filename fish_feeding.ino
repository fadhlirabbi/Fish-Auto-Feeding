/*
 * ============================================
 * SMART FISH FEEDING SYSTEM dengan ESP32
 * ============================================
 */

// ============================================
// KONFIGURASI BLYNK IoT (HARUS PALING ATAS!)
// ============================================
#define BLYNK_TEMPLATE_ID "TMPL67fGjS_6t"
#define BLYNK_TEMPLATE_NAME "Smart Feeding IoT"
#define BLYNK_DEVICE_NAME "Smart_Feeding_001"
#define BLYNK_AUTH_TOKEN "z3uKQfMErQW92uMgMzF2umXf_lLa8LF7"

// Nonaktifkan semua print dari Blynk (hilangkan watermark)
// #define BLYNK_PRINT Serial  // <-- DIHAPUS/DICOMMENT

// ============================================
// LIBRARY
// ============================================
#include <WiFi.h> 
#include <BlynkSimpleEsp32.h> 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <WidgetRTC.h>
#include <NewPing.h>  // Library untuk Ultrasonic

// ============================================
// KONFIGURASI WIFI
// ============================================
const char* ssid = "YTTA";
const char* pass = "YTTAytta_13579_WIFI_LOGIN"; 

// ============================================
// PIN CONFIGURATION (GPIO)
// ============================================
#define PH_SENSOR_PIN       4
#define TDS_SENSOR_PIN      5
#define TEMP_SENSOR_PIN     6
#define SERVO_PIN           1
#define ULTRASONIC_TRIG     17   // JSN-SR04T Trigger
#define ULTRASONIC_ECHO     16   // JSN-SR04T Echo

// ============================================
// KONFIGURASI SENSOR pH
// ============================================
const float PH_ACID_VOLTAGE = 1985.0;
const float PH_NEUTRAL_VOLTAGE = 1480.0;
const float PH_ALKALINE_VOLTAGE = 1000.0;

// ============================================
// KONFIGURASI SENSOR TDS
// ============================================
#define TDS_VREF 3.30
#define TDS_SAMPLE_COUNT 30
const float TDS_CORRECTION_FACTOR = 1.0;

int tdsAnalogBuffer[TDS_SAMPLE_COUNT];
int tdsBufferTemp[TDS_SAMPLE_COUNT];

// ============================================
// KONFIGURASI ULTRASONIC (NewPing)
// ============================================
#define MAX_DISTANCE 600  // Jarak maksimal (cm)
NewPing sonar(ULTRASONIC_TRIG, ULTRASONIC_ECHO, MAX_DISTANCE);

// ============================================
// KONFIGURASI SERVO
// ============================================
const int SERVO_OPEN_ANGLE = 120;
const int SERVO_CLOSE_ANGLE = 0;
const int SERVO_OPEN_DURATION = 1000;

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

// ============================================
// DEKLARASI FUNGSI
// ============================================
void connectWiFi();
void initializeSensors();
void dispenseFeed();
void checkFeedingSchedule();
void sendSensorData();
float measurePH();
float measureTDS();
float measureTemperature();
float measureDistance();
float readPHVoltage();
float readRawTDS();
int getMedianValue(int arr[], int size);

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("Smart Fish Feeding System");
  Serial.println("=================================");

  // Inisialisasi Servo
  feedingServo.attach(SERVO_PIN);
  feedingServo.write(SERVO_CLOSE_ANGLE);
  Serial.println("[OK] Servo initialized");

  // Inisialisasi Sensor
  initializeSensors();

  // Koneksi WiFi & Blynk
  connectWiFi();
  
  // PENTING: Gunakan config() untuk disable print
  Blynk.config(BLYNK_AUTH_TOKEN);
  while (Blynk.connect() == false) {
    // Tunggu koneksi
  }
  Serial.println("[OK] Blynk connected");

  // Sinkronisasi RTC
  rtc.begin();
  Serial.println("[OK] RTC synchronized");
  
  // Setup Timer
  timer.setInterval(10000L, sendSensorData);
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
// FUNGSI PEMBERIAN PAKAN
// ============================================
void dispenseFeed() {
  Serial.println("\n>>> FEEDING STARTED <<<");
  
  if (Blynk.connected()) {
    Blynk.logEvent("feed_given", "Pakan telah diberikan!");
    Blynk.virtualWrite(VPIN_STATUS, "Memberi Pakan...");
  }

  feedingServo.write(SERVO_OPEN_ANGLE);
  delay(SERVO_OPEN_DURATION);
  feedingServo.write(SERVO_CLOSE_ANGLE);
  
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
// FUNGSI KIRIM DATA SENSOR
// ============================================
void sendSensorData() {
  if (!Blynk.connected()) return;

  float phValue = measurePH();
  float tdsValue = measureTDS();
  float temperature = measureTemperature();
  float distance = measureDistance();

  Blynk.virtualWrite(VPIN_PH, phValue);
  Blynk.virtualWrite(VPIN_TDS, tdsValue);
  Blynk.virtualWrite(VPIN_TEMPERATURE, temperature);
  Blynk.virtualWrite(VPIN_DISTANCE, distance);

  Serial.println("--- SENSOR READINGS ---");
  Serial.print("pH: "); Serial.print(phValue, 2);
  Serial.print(" | TDS: "); Serial.print(tdsValue, 0); Serial.print(" ppm");
  Serial.print(" | Temp: "); Serial.print(temperature, 1); Serial.print("°C");
  Serial.print(" | Distance: ");
  if (distance < 0) {
    Serial.println("Out of range");
  } else {
    Serial.print(distance, 0); Serial.println(" cm");
  }
  Serial.println("-----------------------\n");
}

// ============================================
// FUNGSI PENGUKURAN pH
// ============================================
float measurePH() {
  float voltage = readPHVoltage();
  float phValue;

  if (voltage >= PH_NEUTRAL_VOLTAGE) { 
    float slope = (7.0 - 4.0) / (PH_NEUTRAL_VOLTAGE - PH_ACID_VOLTAGE);
    phValue = 4.0 + slope * (voltage - PH_ACID_VOLTAGE);
  } else { 
    float slope = (10.0 - 7.0) / (PH_ALKALINE_VOLTAGE - PH_NEUTRAL_VOLTAGE);
    phValue = 7.0 + slope * (voltage - PH_NEUTRAL_VOLTAGE);
  }

  phValue = constrain(phValue, 0.0, 14.0);
  return phValue;
}

// ============================================
// FUNGSI BACA VOLTAGE pH
// ============================================
float readPHVoltage() {
  long totalValue = 0;
  const int samples = 100;

  for(int i = 0; i < samples; i++) {
    totalValue += analogRead(PH_SENSOR_PIN);
    delay(2);
  }

  float avgValue = (float)totalValue / samples;
  float voltage_mV = (avgValue / 4095.0) * 3300.0;
  
  return voltage_mV;
}

// ============================================
// FUNGSI PENGUKURAN TDS
// ============================================
float measureTDS() {
  float rawTDS = readRawTDS();
  return rawTDS * TDS_CORRECTION_FACTOR;
}

// ============================================
// FUNGSI BACA TDS MENTAH
// ============================================
float readRawTDS() {
  for(int i = 0; i < TDS_SAMPLE_COUNT; i++) {
    tdsAnalogBuffer[i] = analogRead(TDS_SENSOR_PIN);
    delay(5);
  }

  for(int i = 0; i < TDS_SAMPLE_COUNT; i++) {
    tdsBufferTemp[i] = tdsAnalogBuffer[i];
  }

  float temperature = measureTemperature();
  int medianValue = getMedianValue(tdsBufferTemp, TDS_SAMPLE_COUNT);
  float voltage = medianValue * TDS_VREF / 4096.0;

  float tempCoefficient = 1.0 + 0.02 * (temperature - 25.0);
  float compensatedVoltage = voltage / tempCoefficient;

  float tdsValue = (133.42 * compensatedVoltage * compensatedVoltage * compensatedVoltage 
                    - 255.86 * compensatedVoltage * compensatedVoltage 
                    + 857.39 * compensatedVoltage) * 0.5;

  return tdsValue;
}

// ============================================
// FUNGSI PENGUKURAN SUHU
// ============================================
float measureTemperature() {
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  
  if (temp == DEVICE_DISCONNECTED_C) {
    return 25.0;
  }
  
  return temp;
}

// ============================================
// FUNGSI PENGUKURAN JARAK (NewPing)
// ============================================
float measureDistance() {
  unsigned int distance = sonar.ping_cm();
  
  if (distance == 0) {
    return -1.0; // Out of range
  }
  
  return (float)distance;
}

// ============================================
// FUNGSI MEDIAN FILTER
// ============================================
int getMedianValue(int arr[], int size) {
  for (int j = 0; j < size - 1; j++) {
    for (int i = 0; i < size - j - 1; i++) {
      if (arr[i] > arr[i + 1]) {
        int temp = arr[i];
        arr[i] = arr[i + 1];
        arr[i + 1] = temp;
      }
    }
  }

  if (size % 2 == 1) {
    return arr[size / 2];
  } else {
    return (arr[size / 2] + arr[size / 2 - 1]) / 2;
  }
}