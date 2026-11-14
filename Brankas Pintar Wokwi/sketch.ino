#include <Wire.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ESP32Servo.h>
#include <math.h>

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
unsigned long unlockStartedAt = 0;

const float vibrationThreshold = 3.0; // m/s^2, sesuaikan dari hasil simulasi
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
unsigned long lastButtonChange = 0;
const unsigned long debounceMs = 40;

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
  firstReading = true; // ambil baseline baru setelah kembali terkunci
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
}

// ================= ALARM =================
void activateAlarm(const String &reason) {
  if (alarmActive) return;

  alarmActive = true;
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, HIGH);
  tone(BUZZER_PIN, 2200);
  showMessage("SECURITY ALERT!", reason);
}

void resetAlarm() {
  alarmActive = false;
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
    activateAlarm("VIBRATION DETECTED");
  }
}

// ================= INPUT KEYPAD =================
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
        activateAlarm("3 WRONG PIN");
      } else {
        showMessage("WRONG PIN", "ATTEMPT " + String(wrongAttempts) + "/3");
        delay(700);
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

  if ((millis() - lastButtonChange) > debounceMs && reading == LOW) {
    if (alarmActive) {
      resetAlarm();
    }
    // Tunggu tombol dilepas agar hanya satu reset per tekan
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(5);
    }
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

  lockServo.setPeriodHertz(50);
  lockServo.attach(SERVO_PIN, 500, 2400);
  lockDoor();

  delay(1000);
  showPinEntry();
  Serial.println("SMART SAFE BOX READY");
  Serial.println("PIN default: 1234");
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
  }

  delay(5);
}
