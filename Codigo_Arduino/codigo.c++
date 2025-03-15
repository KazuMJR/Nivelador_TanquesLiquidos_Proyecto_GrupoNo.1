

//Codigo para el funcionamiento de Arduino

#include <Wire.h>
#include <LiquidCrystal_I2C.h>


// Configuración del LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);


// Pines del sensor ultrasónico
const int trigPin = 7;
const int echoPin = 6;

// Sensor de temperatura
const int tempSensor = A4;

// Botón con resistencia pull-down
const int botonLCD = A2;

// Pines del buzzer
const int buzzer = A1;

// LEDs
const int leds[] = {2, 3, 5, 8, 9, 10, 11, 12, 13, A0};
const int numLeds = sizeof(leds) / sizeof(leds[0]);

// Parámetros del tanque
const int distanciaMaxima = 400;
const int distanciaMinima = 5;

// Estado del LCD
bool lcdEncendido = true;

// Notas para la melodia (frecuencia en Hz)
const int melodia[] = {262, 294, 330, 349, 392, 440, 494, 523};
const int duraciones[] = {200, 200, 200, 200, 200, 200, 200, 400};

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
  delay(1000);
  lcd.clear();
  lcd.print("Listo");

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(tempSensor, INPUT);
  pinMode(botonLCD, INPUT);
  pinMode(buzzer, OUTPUT);

  for (int i = 0; i < numLeds; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }
}

void loop() {
  int distancia = medirDistancia();
  float temperatura = leerTemperatura();
  int porcentajeLlenado = calcularLlenado(distancia);

  if (lcdEncendido) {
    mostrarDatosLCD(porcentajeLlenado, temperatura);
  } else {
    lcd.noBacklight();
  }

  controlarLEDs(porcentajeLlenado);
  manejarBotonLCD();
  controlarBuzzer(porcentajeLlenado);

  delay(500);
}

// Funciones auxiliares
int medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duracion = pulseIn(echoPin, HIGH, 30000);
  return (duracion > 0) ? duracion * 0.034 / 2 : -1;
}

float leerTemperatura() {
  int tempValue = analogRead(tempSensor);
  return (tempValue * 5.0 / 1023.0) * 100;
}

int calcularLlenado(int distancia) {
  if (distancia <= distanciaMinima) return 100;
  if (distancia >= distanciaMaxima) return 0;
  return (1 - (float)(distancia - distanciaMinima) / (distanciaMaxima - distanciaMinima)) * 100;
}

void mostrarDatosLCD(int porcentajeLlenado, float temperatura) {
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tanque: ");
  lcd.print(porcentajeLlenado);
  lcd.print("%");
  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(temperatura, 1);
  lcd.print("C");
}

void controlarLEDs(int porcentajeLlenado) {
  int ledsEncendidos = map(porcentajeLlenado, 0, 100, 0, numLeds);
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(leds[i], (i < ledsEncendidos) ? HIGH : LOW);
  }
}

void manejarBotonLCD() {
  static bool lastState = LOW;
  bool estadoBoton = digitalRead(botonLCD);
  if (estadoBoton == HIGH && lastState == LOW) {
    lcdEncendido = !lcdEncendido;
  }
  lastState = estadoBoton;
}

void controlarBuzzer(int porcentajeLlenado) {
  if (porcentajeLlenado <= 5 || porcentajeLlenado >= 95) {
    reproducirMelodia();
  } else {
    noTone(buzzer);
  }
}

void reproducirMelodia() {
  for (int i = 0; i < 8; i++) {
    tone(buzzer, melodia[i], duraciones[i]);
    delay(duraciones[i] + 50);
  }
  noTone(buzzer);
}
