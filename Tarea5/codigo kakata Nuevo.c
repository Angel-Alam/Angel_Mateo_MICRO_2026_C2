/*
 * Proyecto: Control KAKATA-433
 * Materia: Micro
 *: Ángel Alam Mateo 2024-0686
 * 
 * Funciones añadidas:
 * - Mapeo de joysticks y giroscopio de -100 a +100.
 * - Calibración de punto 0 manteniendo L1 y L2 por 3 segundos.
 * - Envío de datos (Joysticks, Botones, MPU, Batería) vía MQTT.
 * - Pantalla gráfica (LCD I2C 16x2) con rotación de menús.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "USB.h"
#include "USBHIDGamepad.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ---------- Configuración Wi-Fi y MQTT ----------
const char* ssid = "CELINA";
const char* password = "Pcfe0629";
const char* mqtt_server = "broker.hivemq.com"; 
const char* mqtt_topic = "kakata433/telemetria";

WiFiClient espClient;
PubSubClient client(espClient);

// ---------- Mapa de pines ----------
#define PIN_JOY1_Y      1
#define PIN_JOY1_X      2
#define PIN_JOY0_BTN    3
#define PIN_JOY0_X      4
#define PIN_JOY0_Y      5
#define PIN_I2C_SDA     6
#define PIN_I2C_SCL     7
#define PIN_VBAT        8
#define PIN_BTN0        9
#define PIN_BTN2        10
#define PIN_BTN1        11
#define PIN_BTN3        12
#define PIN_MPU_INT     16
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

#define VBAT_DIVIDER    2.0f
#define VBAT_ADC_VREF   3.3f
#define VBAT_EMPTY      3.0f
#define VBAT_FULL       4.2f

// ---------- Variables Globales y Calibración ----------
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
USBHIDGamepad Gamepad;

volatile bool mpuDataReady = false;

int16_t axAxis = 0, ayAxis = 0, azAxis = 0;
int16_t gxAxis = 0, gyAxis = 0, gzAxis = 0;
float batteryVoltage = 0.0f;
uint8_t batteryPercent = 0;

// Offsets para calibración a 0
int16_t offset_jx0 = 0, offset_jy0 = 0;
int16_t offset_jx1 = 0, offset_jy1 = 0;
int16_t offset_gx = 0, offset_gy = 0, offset_gz = 0;

unsigned long calibracionTimer = 0;
bool calibrando = false;

// ---------- Inicialización de MPU6050 ----------
void mpuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void mpuInit() {
  mpuWriteReg(0x6B, 0x00); 
  mpuWriteReg(0x1C, 0x00); 
  mpuWriteReg(0x1B, 0x00); 
  mpuWriteReg(0x38, 0x01); 
}

bool mpuReadAccelGyro(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14, (uint8_t)true);
  if (Wire.available() < 14) return false;

  *ax = (Wire.read() << 8) | Wire.read();
  *ay = (Wire.read() << 8) | Wire.read();
  *az = (Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read(); 
  *gx = (Wire.read() << 8) | Wire.read();
  *gy = (Wire.read() << 8) | Wire.read();
  *gz = (Wire.read() << 8) | Wire.read();
  return true;
}

void IRAM_ATTR mpuISR() { mpuDataReady = true; }

// ---------- Lectura y Mapeo (-100 a +100) ----------
int8_t readAxis(int pin, int16_t offset) {
  int raw = analogRead(pin);
  int delta = raw - ADC_CENTER - offset;
  
  if (abs(delta) < JOY_DEADZONE) delta = 0;

  long mapped;
  if (delta < 0) {
    mapped = map(delta, -ADC_CENTER, 0, -100, 0); // Mapeo de 0 a -100
  } else {
    mapped = map(delta, 0, ADC_MAX - ADC_CENTER, 0, 100); // Mapeo de 0 a +100
  }
  return (int8_t)constrain(mapped, -100, 100);
}

int8_t mapGyro(int16_t raw_gyro, int16_t offset) {
  int32_t val = raw_gyro - offset;
  // Escala aproximada del giroscopio a +/- 100
  long mapped = map(val, -32768, 32767, -100, 100);
  return (int8_t)constrain(mapped, -100, 100);
}

// ---------- Batería ----------
void readBattery() {
  int raw = analogRead(PIN_VBAT);
  float pinVoltage = (raw / (float)ADC_MAX) * VBAT_ADC_VREF;
  batteryVoltage = pinVoltage * VBAT_DIVIDER;
  float pct = (batteryVoltage - VBAT_EMPTY) / (VBAT_FULL - VBAT_EMPTY) * 100.0f;
  batteryPercent = (uint8_t)constrain(pct, 0.0f, 100.0f);
}

// ---------- Botones y Calibración ----------
uint32_t readButtonMask() {
  uint32_t mask = 0;
  if (digitalRead(PIN_BTN0) == LOW) mask |= (1UL << 0);
  if (digitalRead(PIN_BTN1) == LOW) mask |= (1UL << 1);
  if (digitalRead(PIN_BTN2) == LOW) mask |= (1UL << 2);
  if (digitalRead(PIN_BTN3) == LOW) mask |= (1UL << 3);
  if (digitalRead(PIN_BTN_L1) == LOW) mask |= (1UL << 4);
  if (digitalRead(PIN_BTN_L2) == LOW) mask |= (1UL << 5);
  if (digitalRead(PIN_BTN_L3) == LOW) mask |= (1UL << 6);
  if (digitalRead(PIN_BTN_L4) == LOW) mask |= (1UL << 7);
  if (digitalRead(PIN_JOY0_BTN) == LOW) mask |= (1UL << 8);
  if (digitalRead(PIN_JOY1_BTN) == LOW) mask |= (1UL << 9);
  return mask;
}

void checkCalibration() {
  // Calibración usando los dos botones gatillo superiores (L1 y L2)
  if (digitalRead(PIN_BTN_L1) == LOW && digitalRead(PIN_BTN_L2) == LOW) {
    if (!calibrando) {
      calibracionTimer = millis();
      calibrando = true;
    } else if (millis() - calibracionTimer > 3000) {
      // Registrar nuevos centros a los 3 segundos
      offset_jx0 = analogRead(PIN_JOY0_X) - ADC_CENTER;
      offset_jy0 = analogRead(PIN_JOY0_Y) - ADC_CENTER;
      offset_jx1 = analogRead(PIN_JOY1_X) - ADC_CENTER;
      offset_jy1 = analogRead(PIN_JOY1_Y) - ADC_CENTER;
      offset_gx = gxAxis; offset_gy = gyAxis; offset_gz = gzAxis;
      
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Calibracion OK");
      delay(1000); // Pequeño delay para confirmación visual
      calibracionTimer = millis(); // Prevenir spam de calibración
    }
  } else {
    calibrando = false;
  }
}

// ---------- Wi-Fi y MQTT ----------
void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void reconnectMQTT() {
  if (!client.connected()) {
    if (client.connect("KAKATA_Client")) {
      // Conectado
    }
  }
}

// ---------- Interfaz Gráfica LCD ----------
void updateLCD(int8_t jx0, int8_t jy0, int8_t gx, int8_t gy) {
  static uint8_t pantalla = 0;
  static unsigned long lastSwitch = 0;
  
  if(millis() - lastSwitch > 3000) {
      pantalla = !pantalla;
      lastSwitch = millis();
      lcd.clear();
  }

  if(pantalla == 0) {
    // Pantalla 1: Joysticks y Bateria
    lcd.setCursor(0, 0);
    lcd.printf("Bat:%d%% %.1fV", batteryPercent, batteryVoltage);
    lcd.setCursor(0, 1);
    lcd.printf("J0:%d,%d", jx0, jy0);
  } else {
    // Pantalla 2: MPU y Red
    lcd.setCursor(0, 0);
    lcd.printf("GiroX:%d Y:%d", gx, gy);
    lcd.setCursor(0, 1);
    if(client.connected()) lcd.print("MQTT: Online ");
    else lcd.print("MQTT: Offline");
  }
}

void setup() {
  Serial.begin(115200);

  // Configuración de pines de botones
  int botones[] = {PIN_BTN0, PIN_BTN1, PIN_BTN2, PIN_BTN3, PIN_BTN_L1, PIN_BTN_L2, PIN_BTN_L3, PIN_BTN_L4, PIN_JOY0_BTN, PIN_JOY1_BTN};
  for(int i=0; i<10; i++) pinMode(botones[i], INPUT_PULLUP);
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
  lcd.print("KAKATA-433 Init");
  
  mpuInit();
  attachInterrupt(digitalPinToInterrupt(PIN_MPU_INT), mpuISR, RISING);

  setupWiFi();
  client.setServer(mqtt_server, 1883);

  Gamepad.begin();
  USB.begin();
  delay(1000);
}

void loop() {
  static unsigned long lastSend = 0;
  static unsigned long lastLCD = 0;
  static unsigned long lastMqtt = 0;
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    reconnectMQTT();
    client.loop();
  }

  // 1. Leer MPU6050
  if (mpuDataReady) {
    mpuDataReady = false;
    mpuReadAccelGyro(&axAxis, &ayAxis, &azAxis, &gxAxis, &gyAxis, &gzAxis);
  }
  
  // 2. Comprobar rutina de calibración (Presionar L1 y L2 por 3 segundos)
  checkCalibration();

  // 3. Procesar entradas (Mapeadas de -100 a +100)
  int8_t jx0 = readAxis(PIN_JOY0_X, offset_jx0);
  int8_t jy0 = readAxis(PIN_JOY0_Y, offset_jy0);
  int8_t jx1 = readAxis(PIN_JOY1_X, offset_jx1);
  int8_t jy1 = readAxis(PIN_JOY1_Y, offset_jy1);
  int8_t giro_x = mapGyro(gxAxis, offset_gx);
  int8_t giro_y = mapGyro(gyAxis, offset_gy);
  
  uint32_t buttons = readButtonMask();

  // 4. Leer batería una vez por segundo
  if (now - lastSend >= 1000) readBattery();

  // 5. Enviar comandos HID a la PC por USB (Frecuencia ~66Hz)
  if (now - lastSend >= 15) { 
    lastSend = now;
    Gamepad.leftStick(jx0, jy0);
    Gamepad.rightStick(jx1, jy1);
    Gamepad.buttons(buttons);
    Gamepad.send();
  }

  // 6. Publicar Telemetría por MQTT para el celular (Frecuencia 10Hz)
  if (now - lastMqtt >= 100) {
    lastMqtt = now;
    if(client.connected()){
      char payload[150];
      // Formato JSON para visualizar fácilmente en el celular
      snprintf(payload, sizeof(payload), 
      "{\"J0\":[%d,%d],\"J1\":[%d,%d],\"Giro\":[%d,%d],\"Acel\":[%d,%d,%d],\"Bat\":%d}", 
      jx0, jy0, jx1, jy1, giro_x, giro_y, axAxis, ayAxis, azAxis, batteryPercent);
      client.publish(mqtt_topic, payload);
    }
  }

  // 7. Refrescar la pantalla LCD
  if (now - lastLCD >= 250) {
    lastLCD = now;
    updateLCD(jx0, jy0, giro_x, giro_y);
  }
}