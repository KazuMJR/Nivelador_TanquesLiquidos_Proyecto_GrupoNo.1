#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
#define ONE_WIRE_BUS 11
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const int leds[] = {10, 9, 8, 7, 6, 5, 4, 3, 2};
const int numLeds = sizeof(leds) / sizeof(leds[0]);
const int trigPin = 13;
const int echoPin = 12;
const int buzzer = A0;
const int botonEncender = A2;
const int botonApagar = A3;

bool sistemaEncendido = false;
bool buzzerActivo = true;
bool bloquearMedicion = false;

int distanciaMinima = 20;
int distanciaMaxima = 50;
int capacidadLitros = 15;
bool mostrarEnGalones = true;

unsigned long tiempoPresionadoEncender = 0;
unsigned long tiempoPresionadoApagar = 0;
const unsigned long tiempoActivacion = 5000;

bool enMenu = false;
bool editando = false;
int opcionMenu = 0;
const int totalOpciones = 3;

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
    for (int i = 0; i < numLeds; i++) digitalWrite(leds[i], LOW);
    noTone(buzzer);
    return;
  }

  if (enMenu) {
    mostrarMenu();
    return;
  }

  int distancia = medirDistanciaFiltrada();
  float temperatura = leerTemperatura();
  int porcentajeLlenado = calcularLlenado(distancia);
  float volumenLitros = capacidadLitros * (porcentajeLlenado / 100.0);
  float volumenGalones = volumenLitros / 3.785;

  mostrarDatosLCD(porcentajeLlenado, temperatura, distancia, volumenLitros, volumenGalones);
  controlarLEDs(porcentajeLlenado);
  controlarBuzzer(porcentajeLlenado, temperatura);

  delay(500);
}

// ----------- FUNCIONES -------------

void manejarBotones() {
  unsigned long ahora = millis();

  // BOTÓN ENCENDER
  if (digitalRead(botonEncender) == HIGH) {
    if (tiempoPresionadoEncender == 0)
      tiempoPresionadoEncender = ahora;

    if (!sistemaEncendido && (ahora - tiempoPresionadoEncender >= tiempoActivacion)) {
      sistemaEncendido = true;
      lcd.backlight();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Encendiendo...");
      animacionEncendido();
      delay(500);
    }

  } else {
    if ((ahora - tiempoPresionadoEncender > 50) && (ahora - tiempoPresionadoEncender < tiempoActivacion)) {
      if (sistemaEncendido && !enMenu) {
        enMenu = true;
        opcionMenu = 0;
        lcd.clear();
        delay(300); // Antirrebote
      }
    }
    tiempoPresionadoEncender = 0;
  }

  // BOTÓN APAGAR
  if (digitalRead(botonApagar) == HIGH) {
    if (tiempoPresionadoApagar == 0)
      tiempoPresionadoApagar = ahora;

    if (sistemaEncendido && !enMenu && (ahora - tiempoPresionadoApagar >= tiempoActivacion)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Apagando...");
      animacionApagado();
      delay(500);
      sistemaEncendido = false;
    }

  } else {
    if (enMenu && tiempoPresionadoApagar != 0) {
      entrarOpcionMenu();
      tiempoPresionadoApagar = 0;
      delay(300); // Antirrebote
    } else {
      tiempoPresionadoApagar = 0;
    }
  }
}

void mostrarMenu() {
  lcd.setCursor(0, 0);
  switch (opcionMenu) {
    case 0:
      lcd.print("Mostrar: ");
      lcd.print(mostrarEnGalones ? "Galones" : "Litros ");
      break;
    case 1:
      lcd.print("Dist Max: ");
      lcd.print(distanciaMaxima);
      lcd.print("cm     ");
      break;
    case 2:
      lcd.print("Capacidad: ");
      lcd.print(capacidadLitros);
      lcd.print("L      ");
      break;
  }

  lcd.setCursor(0, 1);
  if (editando) {
    lcd.print("Enc=Mod Apg=OK ");
  } else {
    lcd.print("Enc=Sig Apg=Ed ");
  }

  // Control botón Encender (breve)
  if (digitalRead(botonEncender) == HIGH) {
    delay(200);
    while (digitalRead(botonEncender) == HIGH);

    if (!editando) {
      opcionMenu = (opcionMenu + 1) % totalOpciones;
    } else {
      modificarOpcionActual();
    }
    lcd.clear();
  }
}

// Reemplazar la función entrarOpcionMenu por esta:
void entrarOpcionMenu() {
  if (!editando) {
    editando = true;
  } else {
    editando = false;
    lcd.clear();
    lcd.print("Guardado!");
    delay(500);
    lcd.clear();
    enMenu = false;
  }
}

// Nueva función para modificar valores:
void modificarOpcionActual() {
  switch (opcionMenu) {
    case 0:
      mostrarEnGalones = !mostrarEnGalones;
      break;
    case 1:
      distanciaMaxima += 5;
      if (distanciaMaxima > 150) distanciaMaxima = 30;  // límite superior/inferior
      break;
    case 2:
      capacidadLitros += 5;
      if (capacidadLitros > 100) capacidadLitros = 10;
      break;
  }
}

void mostrarDatosLCD(int porcentaje, float temperatura, int distancia, float litros, float galones) {
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TQ:");
  lcd.print(porcentaje);
  lcd.print("% ");
  int bloques = map(porcentaje, 0, 100, 0, 8);
  for (int i = 0; i < 8; i++) lcd.print(i < bloques ? (char)255 : ' ');

  lcd.setCursor(0, 1);
  if (distancia <= distanciaMinima) {
    lcd.print("Tanque Lleno ");
  } else if (distancia >= distanciaMaxima) {
    lcd.print("Tanque Vacio ");
  } else {
    if (temperatura >= 40.0) {
      lcd.print("Temp ALTA ");
    } else {
      lcd.print(temperatura, 1);
      lcd.print("C ");
    }

    if (mostrarEnGalones) {
      lcd.print(galones, 1);
      lcd.print("G");
    } else {
      lcd.print(litros, 1);
      lcd.print("L");
    }
  }
}

int medirDistanciaFiltrada() {
  static int ultimaDistancia = 100;
  int nuevaDistancia = medirDistancia();
  if (nuevaDistancia == -1) return ultimaDistancia;

  if (bloquearMedicion && nuevaDistancia > distanciaMinima + 2) bloquearMedicion = false;
  if (bloquearMedicion) return ultimaDistancia;

  if (abs(nuevaDistancia - ultimaDistancia) > 1) {
    ultimaDistancia = nuevaDistancia;
  }

  if (ultimaDistancia <= distanciaMinima) bloquearMedicion = true;
  return ultimaDistancia;
}

int medirDistancia() {
  long suma = 0;
  int muestras = 5, validas = 0;

  for (int i = 0; i < muestras; i++) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duracion = pulseIn(echoPin, HIGH, 30000);
    if (duracion > 0) {
      float distancia = duracion * 0.0343 / 2;
      if (distancia > 0 && distancia <= 200) {
        suma += distancia;
        validas++;
      }
    }
    delay(5);
  }

  if (validas == 0) return -1;
  return suma / validas;
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
  return (porcentaje < 5) ? 5 : (int)porcentaje;
}

void controlarLEDs(int porcentajeLlenado) {
  int encender = map(porcentajeLlenado, 0, 100, 0, numLeds);
  if (encender == 0) encender = 1;
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(leds[i], (i < encender) ? HIGH : LOW);
  }
}

void controlarBuzzer(int porcentaje, float temp) {
  static unsigned long ultimaAlarma = 0;
  unsigned long ahora = millis();

  if (!buzzerActivo) {
    noTone(buzzer);
    return;
  }

  if (temp >= 40.0 || porcentaje <= 5 || porcentaje >= 95) {
    if (ahora - ultimaAlarma >= 1000) {
      tone(buzzer, 1500, 200);
      ultimaAlarma = ahora;
      animacionAlerta();
    }
  } else {
    noTone(buzzer);
  }
}

void animacionAlerta() {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < numLeds; j++) digitalWrite(leds[j], HIGH);
    delay(200);
    for (int j = 0; j < numLeds; j++) digitalWrite(leds[j], LOW);
    delay(200);
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
