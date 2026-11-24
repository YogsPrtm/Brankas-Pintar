#include <Arduino.h>
#include <Wire.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ESP32Servo.h>
#include <math.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// ================= KONFIGURASI API LARAVEL LOKAL (WOKWI VS CODE) =================
const char* WIFI_SSID         = "Wokwi-GUEST";
const char* WIFI_PASS         = "";
const char* API_DATA_URL      = "http://host.wokwi.internal:8000/api/brankas/data";
const char* API_HEARTBEAT_URL = "http://host.wokwi.internal:8000/api/brankas/heartbeat";
const int   ID_PERANGKAT      = 1;
const unsigned long HEARTBEAT_INTERVAL = 15000; // Kirim heartbeat tiap 15 detik

// ================= PIN ESP32 =================
#define SERVO_PIN   19
#define BUZZER_PIN  23
#define LED_RED     4
#define LED_GREEN   5
#define BUTTON_PIN  16

// I2C: OLED + MPU6050
#define I2C_SDA 21
#define I2C_SCL 22

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= SENSOR & AKTUATOR =================
Adafruit_MPU6050 mpu;
Servo lockServo;

// ================= KEYPAD 4x4 =================
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {13, 14, 27, 26};
byte colPins[COLS] = {25, 33, 32, 18};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ================= PENGATURAN KEAMANAN =================
const String correctPIN = "1234";
String enteredPIN = "";

const unsigned long unlockDuration = 10000; // 10 detik
uint32_t unlockStartedAt = 0;

const float vibrationThreshold = 3.0; // m/s^2
const unsigned long sensorInterval = 50;
unsigned long lastSensorRead = 0;

int wrongAttempts = 0;
bool alarmActive = false;
bool unlocked = false;
bool mpuReady = false;
bool firstReading = true;

float previousX = 0;
float previousY = 0;
float previousZ = 0;

bool lastButtonState = HIGH;
bool stableButtonState = HIGH;
unsigned long lastButtonChange = 0;
const unsigned long debounceMs = 40;

// ================= ANTREAN EVENT HTTP (FREERTOS) =================
enum class AlarmReason { None, WrongPin, Vibration };
enum class SafeEvent { Opened, AutoLocked, WrongPin, Alarm, Reset };

struct EventRecord {
  SafeEvent event;
  AlarmReason alarm;
  bool unlocked;
  uint8_t attempts;
};

QueueHandle_t eventQueue = nullptr;
AlarmReason alarmReason = AlarmReason::None;

void recordEvent(SafeEvent event) {
  if (!eventQueue) return;
  const EventRecord entry = {event, alarmReason, unlocked, (uint8_t)wrongAttempts};
  if (xQueueSend(eventQueue, &entry, 0) != pdPASS) {
    EventRecord discarded;
    xQueueReceive(eventQueue, &discarded, 0);
    xQueueSend(eventQueue, &entry, 0);
  }
}

void sendEventHttp(const EventRecord &entry) {
  const char* statusPintu = entry.unlocked ? "terbuka" : "terkunci";
  const char* statusAlarm = (entry.alarm != AlarmReason::None) ? "bahaya" : "aman";

  char aktivitas[64];
  char hasil[64];

  switch (entry.event) {
    case SafeEvent::Opened:
      snprintf(aktivitas, sizeof(aktivitas), "Validasi PIN");
      snprintf(hasil, sizeof(hasil), "Berhasil");
      break;
    case SafeEvent::AutoLocked:
      snprintf(aktivitas, sizeof(aktivitas), "Kunci Otomatis");
      snprintf(hasil, sizeof(hasil), "Terkunci");
      break;
    case SafeEvent::WrongPin:
      snprintf(aktivitas, sizeof(aktivitas), "Validasi PIN");
      snprintf(hasil, sizeof(hasil), "Gagal (%u/3)", (unsigned)entry.attempts);
      break;
    case SafeEvent::Alarm:
      if (entry.alarm == AlarmReason::Vibration) {
        snprintf(aktivitas, sizeof(aktivitas), "Deteksi Getaran");
        snprintf(hasil, sizeof(hasil), "Bahaya - Alarm Aktif");
      } else {
        snprintf(aktivitas, sizeof(aktivitas), "Validasi PIN (3x Salah)");
        snprintf(hasil, sizeof(hasil), "Gagal - Alarm Aktif");
      }
      break;
    case SafeEvent::Reset:
      snprintf(aktivitas, sizeof(aktivitas), "Reset Alarm");
      snprintf(hasil, sizeof(hasil), "Berhasil");
      break;
  }

  char payload[256];
  snprintf(payload, sizeof(payload),
      R"({"id_perangkat":%d,"aktivitas":"%s","hasil":"%s","status_pintu":"%s","status_alarm":"%s"})",
      ID_PERANGKAT, aktivitas, hasil, statusPintu, statusAlarm);

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(5000);
  http.begin(client, API_DATA_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  int httpCode = http.POST(payload);
  Serial.printf("[HTTP DATA] %s (%s) -> HTTP %d\n", aktivitas, hasil, httpCode);
  if (httpCode > 0 && httpCode != 201) {
    Serial.println("[RESPON] " + http.getString());
  }
  http.end();
}

void sendHeartbeatHttp() {
  char payload[48];
  snprintf(payload, sizeof(payload), R"({"id_perangkat":%d})", ID_PERANGKAT);

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(5000);
  http.begin(client, API_HEARTBEAT_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  int httpCode = http.POST(payload);
  Serial.printf("[HTTP HEARTBEAT] -> HTTP %d\n", httpCode);
  http.end();
}

// Task terpisah menjaga keypad, servo, dan alarm tetap responsif saat kirim HTTP
void networkTask(void *parameter) {
  (void)parameter;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  Serial.println("Menghubungkan Wi-Fi Wokwi-GUEST...");

  bool wifiReported = false;
  uint32_t lastWiFiAttempt = millis();
  uint32_t lastHeartbeat = 0;

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if (wifiReported) {
        wifiReported = false;
        Serial.println("Wi-Fi terputus.");
      }
      if (millis() - lastWiFiAttempt >= 10000) {
        lastWiFiAttempt = millis();
        WiFi.reconnect();
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    if (!wifiReported) {
      wifiReported = true;
      Serial.println("Wi-Fi tersambung! Mengirim heartbeat awal...");
      sendHeartbeatHttp();
      lastHeartbeat = millis();
    }

    // Kirim semua event yang masuk antrean
    EventRecord pendingEvent;
    if (xQueueReceive(eventQueue, &pendingEvent, 0) == pdPASS) {
      sendEventHttp(pendingEvent);
      lastHeartbeat = millis(); // storeData sudah memperbarui last_seen di Laravel
    }

    // Kirim heartbeat berkala jika perangkat sedang standby
    if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      sendHeartbeatHttp();
      lastHeartbeat = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ================= FUNGSI OLED =================
void showMessage(const String &line1, const String &line2) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 12);
  display.println(line1);
  display.setCursor(0, 34);
  display.println(line2);
  display.display();
}

void showPinEntry() {
  String masked = "";
  for (unsigned int i = 0; i < enteredPIN.length(); i++) masked += '*';
  showMessage("SMART SAFE - ENTER PIN", masked + "  (# = OK)");
}

// ================= KUNCI PINTU =================
void lockDoor() {
  lockServo.write(0);
  unlocked = false;
  firstReading = true;
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);
  showMessage("SMART SAFE BOX", "STATUS: LOCKED");
}

void unlockDoor() {
  lockServo.write(90);
  unlocked = true;
  unlockStartedAt = millis();
  wrongAttempts = 0;
  enteredPIN = "";

  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);
  tone(BUZZER_PIN, 1500, 120);
  showMessage("ACCESS GRANTED", "DOOR UNLOCKED");
  recordEvent(SafeEvent::Opened);
}

// ================= ALARM =================
void activateAlarm(AlarmReason reason) {
  if (alarmActive) return;

  alarmActive = true;
  alarmReason = reason;
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, HIGH);
  tone(BUZZER_PIN, 2200);
  showMessage("SECURITY ALERT!", reason == AlarmReason::Vibration
      ? "VIBRATION DETECTED" : "3 WRONG PIN");
  recordEvent(SafeEvent::Alarm);
}

void resetAlarm() {
  alarmActive = false;
  alarmReason = AlarmReason::None;
  wrongAttempts = 0;
  enteredPIN = "";
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_RED, LOW);

  if (unlocked) {
    showMessage("ACCESS GRANTED", "DOOR UNLOCKED");
  } else {
    lockDoor();
    showPinEntry();
  }
  recordEvent(SafeEvent::Reset);
}

// ================= DETEKSI GETARAN =================
void checkVibration() {
  if (!mpuReady || unlocked || alarmActive) return;
  if (millis() - lastSensorRead < sensorInterval) return;
  lastSensorRead = millis();

  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  float x = accel.acceleration.x;
  float y = accel.acceleration.y;
  float z = accel.acceleration.z;

  if (firstReading) {
    previousX = x;
    previousY = y;
    previousZ = z;
    firstReading = false;
    return;
  }

  float delta = sqrtf(
    (x - previousX) * (x - previousX) +
    (y - previousY) * (y - previousY) +
    (z - previousZ) * (z - previousZ)
  );

  previousX = x;
  previousY = y;
  previousZ = z;

  if (delta > vibrationThreshold) {
    activateAlarm(AlarmReason::Vibration);
  }
}

// ================= INPUT KEYPAD =================
const unsigned int wrongPinBeepHz = 1800;
const unsigned long wrongPinBeepMs = 120;
const unsigned long wrongPinPauseMs = 100;
const unsigned long wrongPinMessageMs = 700;

void checkKeypad() {
  if (alarmActive || unlocked) return;

  char key = keypad.getKey();
  if (!key) return;

  if (key == '*') {
    enteredPIN = "";
    showPinEntry();
    return;
  }

  if (key == '#') {
    if (enteredPIN == correctPIN) {
      unlockDoor();
    } else {
      wrongAttempts++;
      enteredPIN = "";

      if (wrongAttempts >= 3) {
        // Salah ke-3: alarm bunyi & status perangkat berubah jadi "bahaya"
        activateAlarm(AlarmReason::WrongPin);
      } else {
        // Salah ke-1 & ke-2: kirim log Gagal, status_alarm tetap "aman"
        recordEvent(SafeEvent::WrongPin);
        showMessage("WRONG PIN", "ATTEMPT " + String(wrongAttempts) + "/3");
        tone(BUZZER_PIN, wrongPinBeepHz, wrongPinBeepMs);
        delay(wrongPinBeepMs + wrongPinPauseMs);
        tone(BUZZER_PIN, wrongPinBeepHz, wrongPinBeepMs);
        delay(wrongPinMessageMs - wrongPinBeepMs - wrongPinPauseMs);
        showPinEntry();
      }
    }
    return;
  }

  if (key >= '0' && key <= '9' && enteredPIN.length() < 4) {
    enteredPIN += key;
    showPinEntry();
  }
}

// ================= PUSH BUTTON RESET =================
void checkResetButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastButtonChange = millis();
    lastButtonState = reading;
  }

  if (reading != stableButtonState && millis() - lastButtonChange >= debounceMs) {
    stableButtonState = reading;
    if (stableButtonState == LOW && alarmActive) resetAlarm();
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED tidak terdeteksi. Periksa wiring SDA/SCL dan alamat I2C.");
    while (true) delay(10);
  }

  display.clearDisplay();
  display.display();

  mpuReady = mpu.begin(0x68, &Wire);
  if (mpuReady) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } else {
    Serial.println("MPU6050 tidak terdeteksi. Periksa wiring I2C.");
    showMessage("MPU6050 ERROR", "CHECK CONNECTION");
    while (true) delay(10);
  }

  ESP32PWM::allocateTimer(1);
  lockServo.setPeriodHertz(50);
  lockServo.attach(SERVO_PIN, 500, 2400);
  lockDoor();

  delay(1000);
  showPinEntry();
  Serial.println("SMART SAFE BOX READY");

  eventQueue = xQueueCreate(24, sizeof(EventRecord));
  if (!eventQueue) {
    Serial.println("Antrean event gagal dibuat.");
    return;
  }

  if (xTaskCreate(networkTask, "http_sync", 8192, nullptr, 1, nullptr) != pdPASS) {
    Serial.println("Task HTTP gagal dibuat.");
  }
}

// ================= LOOP =================
void loop() {
  checkResetButton();

  if (alarmActive) {
    delay(5);
    return;
  }

  checkVibration();
  checkKeypad();

  if (unlocked && millis() - unlockStartedAt >= unlockDuration) {
    lockDoor();
    showPinEntry();
    recordEvent(SafeEvent::AutoLocked);
  }

  delay(5);
}