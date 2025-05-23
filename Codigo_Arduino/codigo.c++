#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// LCD I2C (A4 = SDA, A5 = SCL)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Sensor de temperatura DS18B20 en D11
#define ONE_WIRE_BUS 11
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Pines
const int leds[] = {10, 9, 8, 7, 6, 5, 4, 3, 2}; // D10 a D2
const int numLeds = sizeof(leds) / sizeof(leds[0]);

const int trigPin = 13;
const int echoPin = 12;
const int buzzer = A0;
const int botonEncender = A2;
const int botonApagar = A3;

// Estados
bool sistemaEncendido = false;
bool buzzerActivo = true;

// Parámetros del tanque
const int distanciaMaxima = 100;
const int distanciaMinima = 7; // Altura mínima para 100%

// Debounce
unsigned long ultimoTiempoBotonEnc = 0;
unsigned long ultimoTiempoBotonApag = 0;
const unsigned long debounceDelay = 200;

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistema listo");

  sensors.begin();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(botonEncender, INPUT);
  pinMode(botonApagar, INPUT);

  for (int i = 0; i < numLeds; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }

  Serial.begin(9600);
}

void loop() {
  manejarBotones();

  if (!sistemaEncendido) {
    lcd.noBacklight();
    for (int i = 0; i < numLeds; i++) {
      digitalWrite(leds[i], LOW);
    }
    noTone(buzzer);
    return;
  }

  int distancia = medirDistanciaFiltrada();
  float temperatura = leerTemperatura();
  int porcentajeLlenado = calcularLlenado(distancia);

  mostrarDatosLCD(porcentajeLlenado, temperatura, distancia);
  controlarLEDs(porcentajeLlenado);
  controlarBuzzer(porcentajeLlenado, temperatura);

  delay(500);
}

// ---------------- FUNCIONES ----------------

int medirDistancia() {
  long suma = 0;
  int muestras = 5;
  int lecturasValidas = 0;

  for (int i = 0; i < muestras; i++) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duracion = pulseIn(echoPin, HIGH, 30000); // Timeout de 30 ms
    if (duracion > 0) {
      float distancia = duracion * 0.0343 / 2;
      if (distancia > 0 && distancia <= 200) {
        suma += distancia;
        lecturasValidas++;
      }
    }
    delay(5);
  }

  if (lecturasValidas == 0) return -1;

  return suma / lecturasValidas;
}

int medirDistanciaFiltrada() {
  static int ultimaDistancia = 100;
  int nuevaDistancia = medirDistancia();

  if (nuevaDistancia != -1 && abs(nuevaDistancia - ultimaDistancia) > 1) {
    ultimaDistancia = nuevaDistancia;
  }

  return ultimaDistancia;
}

float leerTemperatura() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  return (tempC == DEVICE_DISCONNECTED_C) ? -127.0 : tempC;
}

int calcularLlenado(int distancia) {
  if (distancia <= distanciaMinima) return 100;
  if (distancia >= distanciaMaxima) return 0;

  float porcentaje = 100.0 * (1 - (float)(distancia - distanciaMinima) / (distanciaMaxima - distanciaMinima));

  if (porcentaje < 5) porcentaje = 5;
  if (porcentaje > 95) porcentaje = 95;
  return (int)porcentaje;
}

void mostrarDatosLCD(int porcentajeLlenado, float temperatura, int distancia) {
  lcd.backlight();
  lcd.clear();

  // Línea 1: Porcentaje y barra
  lcd.setCursor(0, 0);
  lcd.print("TQ:");
  lcd.print(porcentajeLlenado);
  lcd.print("% ");

  // Barra de progreso (8 bloques máximo)
  int bloques = map(porcentajeLlenado, 0, 100, 0, 8);
  for (int i = 0; i < 8; i++) {
    lcd.print(i < bloques ? (char)255 : ' '); // 255 = carácter '█'
  }

  // Línea 2: Temp y distancia
  lcd.setCursor(0, 1);
  if (temperatura >= 40.0) {
    lcd.print("Temp ALTA! ");
  } else {
    lcd.print("T:");
    lcd.print(temperatura, 1);
    lcd.print("C ");
  }

  lcd.setCursor(11, 1);
  lcd.print(distancia);
  lcd.print("cm");

  // Serial
  Serial.print("Dist: ");
  Serial.print(distancia);
  Serial.print(" cm | Llenado: ");
  Serial.print(porcentajeLlenado);
  Serial.print("% | Temp: ");
  Serial.println(temperatura);
}

void controlarLEDs(int porcentajeLlenado) {
  int ledsEncendidos = map(porcentajeLlenado, 0, 100, 0, numLeds);
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(leds[i], (i < ledsEncendidos) ? HIGH : LOW);
  }
}

void controlarBuzzer(int porcentajeLlenado, float temperatura) {
  static unsigned long ultimaAlarma = 0;
  unsigned long ahora = millis();

  if (!buzzerActivo) {
    noTone(buzzer);
    return;
  }

  if (temperatura >= 40.0 || porcentajeLlenado <= 5 || porcentajeLlenado >= 95) {
    if (ahora - ultimaAlarma >= 1000) {
      tone(buzzer, temperatura >= 40.0 ? 2000 : 1000, 200);
      ultimaAlarma = ahora;
    }
  } else {
    noTone(buzzer);
  }
}

void manejarBotones() {
  unsigned long ahora = millis();

  if (digitalRead(botonEncender) == HIGH && ahora - ultimoTiempoBotonEnc > debounceDelay) {
    if (!sistemaEncendido) {
      sistemaEncendido = true;
      lcd.backlight();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Encendiendo...");
      animacionEncendido();
      delay(500);
    }
    ultimoTiempoBotonEnc = ahora;
  }

  if (digitalRead(botonApagar) == HIGH && ahora - ultimoTiempoBotonApag > debounceDelay) {
    if (sistemaEncendido) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Apagando...");
      animacionApagado();
      delay(500);
      sistemaEncendido = false;
    }
    ultimoTiempoBotonApag = ahora;
  }
}

void animacionEncendido() {
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(leds[i], HIGH);
    delay(100);
  }
}

void animacionApagado() {
  for (int i = numLeds - 1; i >= 0; i--) {
    digitalWrite(leds[i], LOW);
    delay(100);
  }
}
