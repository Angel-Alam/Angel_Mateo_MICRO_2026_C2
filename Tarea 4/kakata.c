/*
 * Control tipo gamepad con ESP32-S3
 * 2 joysticks analogicos, 4 botones centrales, 4 botones laterales,
 * LCD I2C, MPU6050 (accel+giro), lectura de bateria, HID USB nativo.
 *
 * Librerias necesarias (Arduino Library Manager):
 *  - LiquidCrystal I2C (fdebrabander)
 *
 * Placa: ESP32S3 Dev Module
 * Tools > USB Mode: "USB-OTG (TinyUSB)"
 * Tools > USB CDC On Boot: "Enabled" (para ver Serial por el mismo puerto USB-C)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "USB.h"
#include "USBHIDGamepad.h"

// ---------- Mapa de pines ----------
#define PIN_JOY1_Y      1   // ADC joy1 MD
#define PIN_JOY1_X      2   // ADC joy1 MT
#define PIN_JOY0_BTN    3   // joy0 btn
#define PIN_JOY0_X      4   // ADC joy0 MT
#define PIN_JOY0_Y      5   // ADC joy0 MD
#define PIN_I2C_SDA     6   // I2C SDA (LCD + MPU6050)
#define PIN_I2C_SCL     7   // I2C SCL (LCD + MPU6050)
#define PIN_VBAT        8   // ADC lectura de bateria
#define PIN_BTN0        9
#define PIN_BTN2        10
#define PIN_BTN1        11
#define PIN_BTN3        12
#define PIN_MPU_INT     16  // INT del MPU6050
// io19 (USB D-) e io20 (USB D+) los usa el USB nativo, no se tocan
#define PIN_BTN_L4      39
#define PIN_BTN_L3      40
#define PIN_BTN_L2      41
#define PIN_BTN_L1      42
#define PIN_JOY1_BTN    46

// ---------- Constantes ----------
#define LCD_ADDR        0x27
#define LCD_COLS        16
#define LCD_ROWS        2

#define MPU_ADDR        0x68
#define ADC_MAX         4095
#define ADC_CENTER      2048
#define JOY_DEADZONE    120

// Ajustar segun el divisor de voltaje real de la bateria
#define VBAT_DIVIDER    2.0f
#define VBAT_ADC_VREF   3.3f
#define VBAT_EMPTY      3.0f
#define VBAT_FULL       4.2f

// Indices de botones para el reporte HID (1..N)
enum {
  BTN_IDX_0 = 1,
  BTN_IDX_1,
  BTN_IDX_2,
  BTN_IDX_3,
  BTN_IDX_L1,
  BTN_IDX_L2,
  BTN_IDX_L3,
  BTN_IDX_L4,
  BTN_IDX_JOY0,
  BTN_IDX_JOY1
};

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
USBHIDGamepad Gamepad;

volatile bool mpuDataReady = false;

int16_t axAxis = 0, ayAxis = 0, azAxis = 0;
int16_t gxAxis = 0, gyAxis = 0, gzAxis = 0;
float   batteryVoltage = 0.0f;
uint8_t batteryPercent = 0;

// ---------- MPU6050 (registro directo, sin libreria externa) ----------
void mpuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void mpuInit() {
  mpuWriteReg(0x6B, 0x00); // PWR_MGMT_1: salir de sleep, clock interno
  mpuWriteReg(0x1C, 0x00); // ACCEL_CONFIG: +-2g
  mpuWriteReg(0x1B, 0x00); // GYRO_CONFIG: +-250 dps
  mpuWriteReg(0x38, 0x01); // INT_ENABLE: data ready interrupt
}

bool mpuReadAccelGyro(int16_t *ax, int16_t *ay, int16_t *az,
                       int16_t *gx, int16_t *gy, int16_t *gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14, (uint8_t)true);
  if (Wire.available() < 14) return false;

  *ax = (Wire.read() << 8) | Wire.read();
  *ay = (Wire.read() << 8) | Wire.read();
  *az = (Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read(); // temperatura, no se usa
  *gx = (Wire.read() << 8) | Wire.read();
  *gy = (Wire.read() << 8) | Wire.read();
  *gz = (Wire.read() << 8) | Wire.read();
  return true;
}

void IRAM_ATTR mpuISR() {
  mpuDataReady = true;
}

// ---------- Joysticks ----------
int8_t readAxis(int pin) {
  int raw = analogRead(pin);
  int delta = raw - ADC_CENTER;
  if (abs(delta) < JOY_DEADZONE) delta = 0;

  long mapped;
  if (delta < 0) {
    mapped = map(delta, -ADC_CENTER, 0, -127, 0);
  } else {
    mapped = map(delta, 0, ADC_MAX - ADC_CENTER, 0, 127);
  }
  return (int8_t)constrain(mapped, -127, 127);
}

// ---------- Bateria ----------
void readBattery() {
  int raw = analogRead(PIN_VBAT);
  float pinVoltage = (raw / (float)ADC_MAX) * VBAT_ADC_VREF;
  batteryVoltage = pinVoltage * VBAT_DIVIDER;

  float pct = (batteryVoltage - VBAT_EMPTY) / (VBAT_FULL - VBAT_EMPTY) * 100.0f;
  batteryPercent = (uint8_t)constrain(pct, 0.0f, 100.0f);
}

// ---------- Botones -> mascara de 32 bits ----------
uint32_t readButtonMask() {
  uint32_t mask = 0;
  if (digitalRead(PIN_BTN0)    == LOW) mask |= (1UL << (BTN_IDX_0    - 1));
  if (digitalRead(PIN_BTN1)    == LOW) mask |= (1UL << (BTN_IDX_1    - 1));
  if (digitalRead(PIN_BTN2)    == LOW) mask |= (1UL << (BTN_IDX_2    - 1));
  if (digitalRead(PIN_BTN3)    == LOW) mask |= (1UL << (BTN_IDX_3    - 1));
  if (digitalRead(PIN_BTN_L1)  == LOW) mask |= (1UL << (BTN_IDX_L1   - 1));
  if (digitalRead(PIN_BTN_L2)  == LOW) mask |= (1UL << (BTN_IDX_L2   - 1));
  if (digitalRead(PIN_BTN_L3)  == LOW) mask |= (1UL << (BTN_IDX_L3   - 1));
  if (digitalRead(PIN_BTN_L4)  == LOW) mask |= (1UL << (BTN_IDX_L4   - 1));
  if (digitalRead(PIN_JOY0_BTN)== LOW) mask |= (1UL << (BTN_IDX_JOY0 - 1));
  if (digitalRead(PIN_JOY1_BTN)== LOW) mask |= (1UL << (BTN_IDX_JOY1 - 1));
  return mask;
}

// ---------- LCD ----------
void updateLCD(int8_t jx0, int8_t jy0, int8_t jx1, int8_t jy1) {
  lcd.setCursor(0, 0);
  lcd.print("Bat:");
  lcd.print(batteryPercent);
  lcd.print("%  ");

  lcd.setCursor(9, 0);
  lcd.print(batteryVoltage, 2);
  lcd.print("V");

  lcd.setCursor(0, 1);
  lcd.print("J0:");
  lcd.print(jx0);
  lcd.print(",");
  lcd.print(jy0);
  lcd.print(" J1:");
  lcd.print(jx1);
  lcd.print(",");
  lcd.print(jy1);
  lcd.print("   ");
}

void setup() {
  Serial.begin(115200);

  // Botones con pull-up interno (activo en LOW)
  pinMode(PIN_BTN0, INPUT_PULLUP);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_BTN3, INPUT_PULLUP);
  pinMode(PIN_BTN_L1, INPUT_PULLUP);
  pinMode(PIN_BTN_L2, INPUT_PULLUP);
  pinMode(PIN_BTN_L3, INPUT_PULLUP);
  pinMode(PIN_BTN_L4, INPUT_PULLUP);
  pinMode(PIN_JOY0_BTN, INPUT_PULLUP);
  pinMode(PIN_JOY1_BTN, INPUT_PULLUP);

  pinMode(PIN_MPU_INT, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_JOY0_X, ADC_11db);
  analogSetPinAttenuation(PIN_JOY0_Y, ADC_11db);
  analogSetPinAttenuation(PIN_JOY1_X, ADC_11db);
  analogSetPinAttenuation(PIN_JOY1_Y, ADC_11db);
  analogSetPinAttenuation(PIN_VBAT, ADC_11db);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ESP32-S3 Gamepad");
  delay(1000);
  lcd.clear();

  mpuInit();
  attachInterrupt(digitalPinToInterrupt(PIN_MPU_INT), mpuISR, RISING);

  Gamepad.begin();
  USB.begin();
}

void loop() {
  static unsigned long lastSend = 0;
  static unsigned long lastLCD = 0;
  static unsigned long lastBattery = 0;
  unsigned long now = millis();

  if (mpuDataReady) {
    mpuDataReady = false;
    mpuReadAccelGyro(&axAxis, &ayAxis, &azAxis, &gxAxis, &gyAxis, &gzAxis);
  }

  if (now - lastBattery >= 1000) {
    lastBattery = now;
    readBattery();
  }

  int8_t jx0 = readAxis(PIN_JOY0_X);
  int8_t jy0 = readAxis(PIN_JOY0_Y);
  int8_t jx1 = readAxis(PIN_JOY1_X);
  int8_t jy1 = readAxis(PIN_JOY1_Y);
  uint32_t buttons = readButtonMask();

  if (now - lastSend >= 15) { // ~66 Hz
    lastSend = now;
    Gamepad.leftStick(jx0, jy0);
    Gamepad.rightStick(jx1, jy1);
    Gamepad.buttons(buttons);
    Gamepad.send();
  }

  if (now - lastLCD >= 250) {
    lastLCD = now;
    updateLCD(jx0, jy0, jx1, jy1);
  }
}