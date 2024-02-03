#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif
#include <MPU6050.h>


// Definición de pines para HX711
const int HX711_dout = 4; // mcu > HX711 dout pin
const int HX711_sck = 5; // mcu > HX711 sck pin

// Definición de pines para LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Definición de pines para MPU6050
MPU6050 mpu;

// Definición de pines para relay y botón de paro
const int relayPin = 8;
const int stopButtonPin = 9;

// Variables para celda de carga
//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);
const int calVal_eepromAdress = 0;
unsigned long t = 0;

// Variables para torque
const float distancia = 0.14; // Distancia en metros

// Variables para MPU6050
const float vibracionUmbral = 2.5;
bool relayActivado = false;
bool buttonState = HIGH;
bool lastButtonState = HIGH;

void setup() {
  //Inicialización de los componentes 
  lcd.begin(16, 2);
  mpu.initialize();
  pinMode(relayPin, OUTPUT);
  pinMode(stopButtonPin, INPUT_PULLUP);
  lcd.backlight();
  Serial.begin(57600); delay(10);
  Serial.println();
  Serial.println("Starting...");
  float calibrationValue; // valor de calibración
  calibrationValue = 696.0; 
#if defined(ESP8266) || defined(ESP32)
  //EEPROM.begin(512); // Descomenta esto si estás utilizando ESP8266 y deseas obtener este valor desde la memoria EEPROM.
#endif
  EEPROM.get(calVal_eepromAdress, calibrationValue); // Descomenta esto si deseas obtener este valor desde la memoria EEPROM.

  LoadCell.begin();
  //LoadCell.setReverseOutput();
  unsigned long stabilizingtime = 2000; // La precisión de la tarea puede mejorarse agregando unos segundos de tiempo de estabilización.
  boolean _tare = true; // Establece esto como falso si no deseas que la tara se realice en el próximo paso.
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
  }
  else {
    LoadCell.setCalFactor(calibrationValue); // establece el factor de calibración (float)
    Serial.println("Startup is complete");
  }
  while (!LoadCell.update());
  Serial.print("Valor de calibración: ");
  Serial.println(LoadCell.getCalFactor());
  Serial.print("Tiempo de conversión medido por HX711 en milisegundos (ms): ");
  Serial.println(LoadCell.getConversionTime());
  Serial.print("Frecuencia de muestreo medida por HX711 en Hertzios (Hz): ");
  Serial.println(LoadCell.getSPS());
  Serial.print("Tiempo de estabilización medido por HX711 en milisegundos (ms): ");
  Serial.println(LoadCell.getSettlingTime());
  Serial.println("Ten en cuenta que el tiempo de estabilización puede aumentar significativamente si utilizas delay() en tu programa.");
  if (LoadCell.getSPS() < 7) {
    Serial.println("!!La tasa de muestreo es más baja que la especificación; verifica la conexión MCU>HX711, así como las designaciones de pines.");
  }
  else if (LoadCell.getSPS() > 100) {
    Serial.println("!!La tasa de muestreo es más alta que la especificación; verifica la conexión MCU>HX711, así como las designaciones de pines.");
  }

  lcd.setCursor(11, 0);
  lcd.print("R:OFF"); //mostramos el estado inicial del relé

}

void loop() {
  // Lectura del botón de paro
  buttonState = digitalRead(stopButtonPin);

  // Manejo del rebote del botón y activación inmediata
  if (buttonState == LOW && lastButtonState == HIGH) {
    relayActivado = !relayActivado; // Cambiar el estado del relay
    digitalWrite(relayPin, relayActivado ? LOW : HIGH);
    lcd.setCursor(11, 0);
    lcd.print(relayActivado ? "R: ON" : "R:OFF");
    Serial.println(relayActivado ? "Peligro, Relé activado" : "Relé apagado");
  }

  lastButtonState = buttonState; //Se actualiza el estado del botón

  static boolean newDataReady = 0;
  const int serialPrintInterval = 500; //Aumenta el valor para reducir la actividad de impresión en serie.

  // revisar por nueva información/comenzar conversión:
  if (LoadCell.update()) newDataReady = true;

  // obtener el valor suavizado del conjunto de datos:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float peso = LoadCell.getData()/1000;//Obtenemos el valor en kilogramos
      Serial.print("Peso kg ");
      Serial.println(peso);
      newDataReady = 0;
      t = millis();
      // Mostrar peso en el LCD
      lcd.setCursor(0, 0);
      lcd.print("Peso:");
      lcd.print(peso);

      // Calcular y mostrar torque en el LCD y por serial
      float fuerza = peso * 9.8; // Convertir peso a fuerza en newtons
      float torque = fuerza * distancia;//Calcular el torque

      lcd.setCursor(0, 1);
      lcd.print("Torque:");
      lcd.print(torque,2);

      Serial.print("Torque: ");
      Serial.println(torque,2);
    }
  }

  // Recibe el comando desde el terminal serie, envía 't' para iniciar la operación de tara
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  // Verifica si la última operación de tara ha sido completada:
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }

  // Lectura de MPU6050
  float accelX_gravity  = mpu.getAccelerationX();
  float vibracionX = accelX_gravity / 16384.0; // Convertir a unidades de gravedad
  float accelY_gravity  = mpu.getAccelerationY();
  float vibracionY = accelY_gravity / 16384.0; // Convertir a unidades de gravedad
  float accelZ_gravity  = mpu.getAccelerationZ();
  float vibracionZ = accelZ_gravity / 16384.0; // Convertir a unidades de gravedad

  // Mostrar los valores en el monitor serial
  Serial.print("Vibraciones (g): ");
  Serial.print("X: ");
  Serial.print(vibracionX);
  Serial.print(", Y: ");
  Serial.print(vibracionY);
  Serial.print(", Z: ");
  Serial.println(vibracionZ);

  // Verificar vibración y activar relay si es necesario
  if (abs(vibracionX) > vibracionUmbral || abs(vibracionY) > vibracionUmbral || abs(vibracionZ) > vibracionUmbral) {
    digitalWrite(relayPin, LOW);
    relayActivado = true;
    lcd.setCursor(11, 0);
    lcd.print("R: ON");
    Serial.println("Peligro, Relé activado");
  }
  delay(500); // Esperar medio segundo
}

