#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// LCD I2C (conectado a A4(SDA) y A5(SCL))
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Sensor de temperatura (DS18B20 en D11)
#define ONE_WIRE_BUS 11
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Pines
const int leds[] = {2, 3, 4, 5, 6, 7, 8, 9, 10}; // D2 a D10
const int numLeds = sizeof(leds) / sizeof(leds[0]);

const int trigPin = 13;      // D13 (RX del sensor ultrasónico)
const int echoPin = 12;      // D12 (TX del sensor ultrasónico)
const int buzzer = A0;       // Buzzer en A0
const int botonLCD = A2;     // Botón para LCD
const int botonMute = A3;    // Botón para silenciar buzzer

// Estado
bool lcdEncendido = true;
bool buzzerActivo = true;

// Parámetros tanque
const int distanciaMaxima = 400;
const int distanciaMinima = 5;

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
  delay(1000);
  lcd.clear();
  lcd.print("Listo");

  sensors.begin();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(botonLCD, INPUT);
  pinMode(botonMute, INPUT);

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
  manejarBotones();
  controlarBuzzer(porcentajeLlenado, temperatura);

  delay(500);
}

// Funciones
int medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duracion = pulseIn(echoPin, HIGH, 30000);
  if (duracion == 0) {
    return -1;  // No eco detectado
  } else {
    return duracion * 0.034 / 2;
  }
}

float leerTemperatura() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  return (tempC == DEVICE_DISCONNECTED_C) ? -127.0 : tempC;
}

int calcularLlenado(int distancia) {
  if (distancia == -1) return 0;
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

  if (temperatura >= 40.0) {
    lcd.setCursor(0, 1);
    lcd.print("Temp ALTA!     ");
  }
}

void controlarLEDs(int porcentajeLlenado) {
  int ledsEncendidos = map(porcentajeLlenado, 0, 100, 0, numLeds);
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(leds[i], (i < ledsEncendidos) ? HIGH : LOW);
  }
}

void manejarBotones() {
  static bool lastEstadoLCD = LOW;
  static bool lastEstadoMute = LOW;

  bool estadoLCD = digitalRead(botonLCD);
  bool estadoMute = digitalRead(botonMute);

  // Botón LCD
  if (estadoLCD == HIGH && lastEstadoLCD == LOW) {
    lcdEncendido = !lcdEncendido;
  }
  lastEstadoLCD = estadoLCD;

  // Botón Mute
  if (estadoMute == HIGH && lastEstadoMute == LOW) {
    buzzerActivo = !buzzerActivo;
  }
  lastEstadoMute = estadoMute;
}

void controlarBuzzer(int porcentajeLlenado, float temperatura) {
  static unsigned long ultimaAlarma = 0;
  unsigned long ahora = millis();

  if (!buzzerActivo) {
    noTone(buzzer);
    return;
  }

  if (temperatura >= 40.0) {
    // Sonido para temperatura alta: tono más agudo
    if (ahora - ultimaAlarma >= 1000) {
      tone(buzzer, 2000, 200);
      delay(150);
      tone(buzzer, 2000, 200);
      ultimaAlarma = ahora;
    }
  }
  else if (porcentajeLlenado <= 5 || porcentajeLlenado >= 95) {
    // Sonido para nivel de tanque crítico
    if (ahora - ultimaAlarma >= 1000) {
      tone(buzzer, 1000, 200);
      delay(250);
      tone(buzzer, 1000, 200);
      ultimaAlarma = ahora;
    }
  } else {
    noTone(buzzer);
  }
}
