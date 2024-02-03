#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Dirección I2C 0x27, 16 columnas y 2 filas
Servo servoMotor;  // Instancia del objeto Servo

const int sensorIR = 2;
const int buttonPin = 7;    // Pin del botón
const int exitButtonPin = 8; // Pin del botón de salida
const int potPin = A0;      // Pin del potenciómetro
const int rpmRange = 3000;

int contador = 0;
unsigned long tiempoAhora = 0;
unsigned long periodo = 5000;  // Periodo de un minuto en milisegundos
int valorFijado = 0;
int velocidadDeseada = 0;  // Velocidad deseada para el control por histéresis
int velocidadReal = 0;  // Velocidad real medida por el sensor IR
int acumulado = 0;

bool iniciado = false;
bool aplicarHisteresis = false;

// Definir un margen de error permitido
int margenError = 100;

void resetProgram() {
  iniciado = false;
  aplicarHisteresis = false;
  contador = 0;
  tiempoAhora = 0;
  acumulado=0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ingrese Su");
  lcd.setCursor(0, 1);
  lcd.print("Velocidad: ");

  servoMotor.write(40); // Posición inicial del servomotor
}

void setup() {
  pinMode(sensorIR, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);  // Configurar el botón con resistencia pull-up interna
  pinMode(exitButtonPin, INPUT_PULLUP); // Configurar el botón de salida con resistencia pull-up interna
  Serial.begin(9600);

  lcd.begin(16, 2);  // Inicializar el LCD
  lcd.backlight();   // Encender la luz de fondo del LCD
  lcd.setCursor(0, 0);
  lcd.print("Ingrese Su");
  lcd.setCursor(0, 1);
  lcd.print("Velocidad: ");
  resetProgram();
  servoMotor.write(40); // Posición inicial del servomotor
  servoMotor.attach(9);  // Conectar el servo al pin 9
}

void loop() {
  // Leer el valor del potenciómetro y mapearlo al rango de 500 a 1000 RPM
  velocidadDeseada = map(analogRead(potPin), 0, 1023, 500, 1000);
  
  delay(100);

  // Mostrar la velocidad deseada en el LCD solo si no se ha iniciado
  if (!iniciado) {
    lcd.setCursor(11, 1);
    lcd.print("    "); // Borrar caracteres anteriores
    lcd.setCursor(11, 1);
    lcd.print(velocidadDeseada);
  }

  // Esperar a que se presione el botón para comenzar el conteo y ajustar la velocidad
  if (!iniciado && digitalRead(buttonPin) == LOW) {
    iniciado = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Set:");
    lcd.print(velocidadDeseada);
    lcd.setCursor(0, 1);
    lcd.print("C:");

    // Fijar la posición inicial del servomotor con la cual se acciona levemente el gatillo 
    servoMotor.write(50);
  }

  // Verificar si se presionó el botón de salida
  if (digitalRead(exitButtonPin) == LOW) {
    resetProgram();
  }

  // Si ha sido iniciado, mostrar el contador y contar las vueltas
  if (iniciado) {
    // Muestra el contador en el LCD
    lcd.setCursor(2, 1);
    lcd.print(contador);

    // Cada un minuto reinicia el contador
    if (millis() > (periodo + tiempoAhora)) {
      valorFijado = contador;
      Serial.println(contador);
      lcd.clear();
      lcd.setCursor(10, 0);
      lcd.print("R:");
      contador=contador*12;
      lcd.print(contador);
      tiempoAhora = millis();
      acumulado=acumulado+contador;
      lcd.setCursor(7, 1);  // Ajustar la posición del cursor en el LCD
      lcd.print("     ");   // Borrar caracteres anteriores
      lcd.setCursor(7, 1);
      lcd.print("V:");
      lcd.print(acumulado);
      Serial.println(acumulado);
      lcd.setCursor(0, 0);
      lcd.print("Set:");
      lcd.print(velocidadDeseada);
      lcd.setCursor(0, 1);
      lcd.print("C:");
      velocidadReal = contador;  // Actualizar velocidadReal al final del conteo
      aplicarHisteresis = true;
      contador = 0;
    }

    // Detecto la cinta lo cual es igual a las vueltas
    if (digitalRead(sensorIR) == 0) {
      contador+=3;
    }

    // Control por histéresis después de reiniciar el contador
    if (aplicarHisteresis) {
    int diferenciaVelocidad = int(velocidadReal) - int(velocidadDeseada);

    if (diferenciaVelocidad < -margenError) {
        // Si la velocidad real es significativamente menor que la deseada, aumentar la velocidad
        servoMotor.write(servoMotor.read() + 1);
    } else if (diferenciaVelocidad > margenError) {
        // Si la velocidad real es significativamente mayor que la deseada, disminuir la velocidad
        servoMotor.write(servoMotor.read() - 1);
    } else {
        // Si la velocidad real está dentro del margen de error, no varía
        servoMotor.write(servoMotor.read());
    }
    // Desactivar la histéresis para el siguiente minuto
    aplicarHisteresis = false;
    }
  }
}
