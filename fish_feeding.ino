#define BLYNK_TEMPLATE_ID " "
#define BLYNK_TEMPLATE_NAME " "
#define BLYNK_DEVICE_NAME " "
#define BLYNK_AUTH_TOKEN " "

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <WidgetRTC.h>
#include <NewPing.h>
#include <Preferences.h>

#define NAMESPACE "smart_feed"

// === PIN DEFINISI ===
#define PH_SENSOR_PIN     4
#define TDS_SENSOR_PIN    5
#define TEMP_SENSOR_PIN   6
#define SERVO_PIN         1
#define ULTRASONIC_TRIG   17
#define ULTRASONIC_ECHO   16
#define BLOWER_PIN        19

#define MAX_DISTANCE 600
#define SERVO_OPEN_ANGLE 120
#define SERVO_CLOSE_ANGLE 0
#define SERVO_OPEN_DURATION 1000
#define BLOWER_DURATION 5000

// === OBJEK ===
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);
Servo feedingServo;
BlynkTimer timer;
WidgetRTC rtc;
NewPing sonar(ULTRASONIC_TRIG, ULTRASONIC_ECHO, MAX_DISTANCE);
Preferences preferences;

// === VARIABEL GLOBAL ===
String ssid, pass;
float currentPhValue = 7.0, currentTdsValue = 0.0, currentTemperature = 25.0;
int FEED_HOURS[3] = {9, 15, 21};
int FEED_MINUTES[3] = {0, 0, 0};
bool feedingDone[3] = {false, false, false};

bool isFeeding = false;
unsigned long feedStartTime = 0;

// === BLYNK VPIN ===
#define VPIN_PH           V1
#define VPIN_TDS          V2
#define VPIN_TEMPERATURE  V3
#define VPIN_DISTANCE     V4
#define VPIN_MANUAL_FEED  V5
#define VPIN_STATUS       V6
#define VPIN_FEED1_TIME   V10
#define VPIN_FEED2_TIME   V11
#define VPIN_FEED3_TIME   V12

// === PROTOTIPE ===
void connectWiFi();
void sendSensorData();
void checkFeedingSchedule();
void startFeeding();
void updateFeeding();
void saveFeedingTimeToNVS();
void loadFeedingTimeFromNVS();
void setupWiFiViaSerial();

// === SETUP ===
void setup() {
  Serial.begin(115200);
  preferences.begin(NAMESPACE, false);

  ssid = preferences.getString("wifi_ssid", "");
  pass = preferences.getString("wifi_pass", "");

  // Jika kosong, minta input SSID dan password
  if (ssid == "" || pass == "") {
    setupWiFiViaSerial();
  }

  loadFeedingTimeFromNVS();

  feedingServo.attach(SERVO_PIN);
  feedingServo.write(SERVO_CLOSE_ANGLE);
  pinMode(BLOWER_PIN, OUTPUT);
  digitalWrite(BLOWER_PIN, LOW);

  tempSensor.begin();

  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi gagal tersambung. Masukkan ulang kredensial.");
    setupWiFiViaSerial();
    connectWiFi();
  }

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid.c_str(), pass.c_str());
  rtc.begin();

  // Interval hemat kuota
  timer.setInterval(60000L, sendSensorData);     // setiap 1 menit
  timer.setInterval(60000L, checkFeedingSchedule);

  Serial.println("\nSistem siap. Jadwal pakan:");
  for (int i = 0; i < 3; i++) {
    Serial.printf("  [%d] %02d:%02d\n", i + 1, FEED_HOURS[i], FEED_MINUTES[i]);
  }

  Serial.println("\nKetik 'SETWIFI' di Serial untuk ubah SSID & password WiFi.");
}

// === LOOP ===
void loop() {
  Blynk.run();
  timer.run();
  updateFeeding();

  // Cek perintah Serial
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("SETWIFI")) {
      setupWiFiViaSerial();
      ESP.restart();
    }
  }
}

// === Setup WiFi via Serial (aman & tidak nge-freeze) ===
void setupWiFiViaSerial() {
  Serial.println("\n=== Konfigurasi WiFi Baru ===");

  Serial.print("Masukkan SSID (20 detik timeout): ");
  unsigned long start = millis();
  while (!Serial.available() && millis() - start < 20000);
  if (Serial.available()) {
    ssid = Serial.readStringUntil('\n');
    ssid.trim();
  } else {
    Serial.println("\nTimeout! Gunakan SSID lama jika ada.");
  }

  Serial.print("Masukkan Password (20 detik timeout): ");
  start = millis();
  while (!Serial.available() && millis() - start < 20000);
  if (Serial.available()) {
    pass = Serial.readStringUntil('\n');
    pass.trim();
  } else {
    Serial.println("\nTimeout! Gunakan password lama jika ada.");
  }

  preferences.putString("wifi_ssid", ssid);
  preferences.putString("wifi_pass", pass);

  Serial.println("\nWiFi baru disimpan:");
  Serial.printf("SSID: %s\nPassword: %s\n", ssid.c_str(), pass.c_str());
}

// === Koneksi WiFi ===
void connectWiFi() {
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("\nMenghubungkan ke WiFi: %s\n", ssid.c_str());
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Terhubung!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nGagal konek WiFi!");
  }
}

// === Kirim data sensor ke Blynk (hemat kuota) ===
void sendSensorData() {
  static float lastTemp = -100, lastPh = -100, lastTds = -100, lastDist = -100;
  tempSensor.requestTemperatures();
  currentTemperature = tempSensor.getTempCByIndex(0);
  float distance = sonar.ping_cm();

  // hanya kirim jika ada perubahan signifikan
  if (abs(currentPhValue - lastPh) > 0.1) {
    Blynk.virtualWrite(VPIN_PH, currentPhValue);
    lastPh = currentPhValue;
  }
  if (abs(currentTdsValue - lastTds) > 5) {
    Blynk.virtualWrite(VPIN_TDS, currentTdsValue);
    lastTds = currentTdsValue;
  }
  if (abs(currentTemperature - lastTemp) > 0.5) {
    Blynk.virtualWrite(VPIN_TEMPERATURE, currentTemperature);
    lastTemp = currentTemperature;
  }
  if (abs(distance - lastDist) > 1) {
    Blynk.virtualWrite(VPIN_DISTANCE, distance);
    lastDist = distance;
  }
}

// === Jadwal Pemberian Pakan ===
void startFeeding() {
  if (isFeeding) return;
  Serial.println(">>> FEEDING <<<");
  Blynk.virtualWrite(VPIN_STATUS, "Memberi Pakan...");
  feedingServo.write(SERVO_OPEN_ANGLE);
  digitalWrite(BLOWER_PIN, HIGH);
  feedStartTime = millis();
  isFeeding = true;
}

void updateFeeding() {
  if (!isFeeding) return;
  unsigned long elapsed = millis() - feedStartTime;
  if (elapsed >= SERVO_OPEN_DURATION && elapsed < BLOWER_DURATION) {
    feedingServo.write(SERVO_CLOSE_ANGLE);
  }
  if (elapsed >= BLOWER_DURATION) {
    digitalWrite(BLOWER_PIN, LOW);
    Blynk.virtualWrite(VPIN_STATUS, "Selesai memberi pakan");
    Serial.println(">>> SELESAI <<<");
    isFeeding = false;
  }
}

BLYNK_WRITE(VPIN_MANUAL_FEED) {
  if (param.asInt() == 1) startFeeding();
}

void checkFeedingSchedule() {
  int h = hour();
  int m = minute();
  if (h == 0 && m == 1) {
    for (int i = 0; i < 3; i++) feedingDone[i] = false;
  }
  for (int i = 0; i < 3; i++) {
    if (h == FEED_HOURS[i] && m == FEED_MINUTES[i] && !feedingDone[i]) {
      feedingDone[i] = true;
      startFeeding();
    }
  }
}

// === Blynk TimeInput ===
BLYNK_WRITE(VPIN_FEED1_TIME) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    FEED_HOURS[0] = t.getStartHour();
    FEED_MINUTES[0] = t.getStartMinute();
    saveFeedingTimeToNVS();
    Serial.printf("[Blynk] Jadwal 1 diatur ke %02d:%02d\n", FEED_HOURS[0], FEED_MINUTES[0]);
  }
}

BLYNK_WRITE(VPIN_FEED2_TIME) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    FEED_HOURS[1] = t.getStartHour();
    FEED_MINUTES[1] = t.getStartMinute();
    saveFeedingTimeToNVS();
    Serial.printf("[Blynk] Jadwal 2 diatur ke %02d:%02d\n", FEED_HOURS[1], FEED_MINUTES[1]);
  }
}

BLYNK_WRITE(VPIN_FEED3_TIME) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    FEED_HOURS[2] = t.getStartHour();
    FEED_MINUTES[2] = t.getStartMinute();
    saveFeedingTimeToNVS();
    Serial.printf("[Blynk] Jadwal 3 diatur ke %02d:%02d\n", FEED_HOURS[2], FEED_MINUTES[2]);
  }
}

// === Simpan / Muat Jadwal dari NVS ===
void saveFeedingTimeToNVS() {
  preferences.putInt("f1h", FEED_HOURS[0]);
  preferences.putInt("f1m", FEED_MINUTES[0]);
  preferences.putInt("f2h", FEED_HOURS[1]);
  preferences.putInt("f2m", FEED_MINUTES[1]);
  preferences.putInt("f3h", FEED_HOURS[2]);
  preferences.putInt("f3m", FEED_MINUTES[2]);
}

void loadFeedingTimeFromNVS() {
  FEED_HOURS[0] = preferences.getInt("f1h", 9);
  FEED_MINUTES[0] = preferences.getInt("f1m", 0);
  FEED_HOURS[1] = preferences.getInt("f2h", 15);
  FEED_MINUTES[1] = preferences.getInt("f2m", 0);
  FEED_HOURS[2] = preferences.getInt("f3h", 21);
  FEED_MINUTES[2] = preferences.getInt("f3m", 0);
}
