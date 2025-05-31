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

int distanciaMinima = 20; // en cm
int distanciaMaxima = 50; // en cm
int capacidadLitros = 15;
bool mostrarEnGalones = false; // Comienza mostrando en litros
bool usarMetros = false; // Variable para seleccionar entre metros y centímetros

unsigned long tiempoPresionadoEncender = 0;
unsigned long tiempoPresionadoApagar = 0;
const unsigned long tiempoActivacion = 5000;

bool enMenu = false;
bool editando = false;
int opcionMenu = 0;
const int totalOpciones = 5; // Total de opciones en el menú (incluye la opción de unidad)

unsigned long ultimoInputMenu = 0;
const unsigned long tiempoInactividadMenu = 10000; // 10 segundos
unsigned long tiempoUltimaEdicion = 0; // Tiempo de la última edición
unsigned long tiempoUltimaEdicionRapida = 0; // Tiempo para edición rápida
const unsigned long tiempoRapido = 200; // Tiempo para incrementar rápidamente

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
  float volumenGalones = litrosAGalones(volumenLitros); // Conversión a galones

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
  unsigned long ahora = millis(); // Declarar ahora aquí
  lcd.setCursor(0, 0);
  switch (opcionMenu) {
    case 0:
      lcd.print("Mostrar: ");
      lcd.print(mostrarEnGalones ? "Galones" : "Litros ");
      break;
    case 1:
      lcd.print("Dist Max: ");
      lcd.print(distanciaMaxima);
      lcd.print(usarMetros ? " m     " : " cm     "); // Mostrar unidad
      break;
    case 2:
      lcd.print("Capacidad: ");
      if (mostrarEnGalones) {
        lcd.print(litrosAGalones(capacidadLitros), 1); // Mostrar en galones
        lcd.print("G    ");
      } else {
        lcd.print(capacidadLitros);
        lcd.print("L     ");
      }
      break;
    case 3:
      lcd.print("Unidad: ");
      lcd.print(usarMetros ? "Metros" : "Centim");
      break;
    case 4:
      lcd.print("Guardando..."); // Opción para guardar configuración
      break;
  }

  lcd.setCursor(0, 1);
  if (editando) {
    lcd.print("Enc=Sub Apg=Baj");
  } else {
    lcd.print("Enc=Sig Apg=Ed ");
  }

  // BOTÓN ENCENDER (subir o siguiente)
  if (digitalRead(botonEncender) == HIGH) {
    delay(200);
    while (digitalRead(botonEncender) == HIGH);
    
    if (!editando) {
      opcionMenu = (opcionMenu + 1) % totalOpciones;
    } else {
      modificarOpcionActual(true);  // subir valor
      tiempoUltimaEdicion = millis(); // Reinicia el temporizador de inactividad
    }
    lcd.clear();
  }

  // BOTÓN APAGAR (bajar valor o editar)
  if (editando && digitalRead(botonApagar) == HIGH) {
    delay(200);
    while (digitalRead(botonApagar) == HIGH);
    modificarOpcionActual(false);  // bajar valor
    tiempoUltimaEdicion = millis(); // Reinicia el temporizador de inactividad
    lcd.clear();
  }

  // Verifica si ha pasado el tiempo de inactividad
  if (editando && (millis() - tiempoUltimaEdicion >= tiempoInactividadMenu)) {
    guardarConfiguracion(); // Llama a la función para guardar la configuración
    editando = false; // Sale del modo de edición
    lcd.clear();
    lcd.print("Configuracion"); // Mensaje de guardado
    lcd.setCursor(0, 1);
    lcd.print("Guardada!");
    delay(1000); // Espera 1 segundo antes de volver al menú
    enMenu = false; // Sale del menú
  }

  // Manejo de incremento/decremento rápido
  if (editando) {
    if (digitalRead(botonEncender) == HIGH) {
      if (ahora - tiempoUltimaEdicionRapida >= tiempoRapido) {
        modificarOpcionActual(true); // Incrementar rápido
        tiempoUltimaEdicionRapida = ahora; // Reinicia el tiempo
      }
    } else {
      tiempoUltimaEdicionRapida = 0; // Resetea si no está presionado
    }

    if (digitalRead(botonApagar) == HIGH) {
      if (ahora - tiempoUltimaEdicionRapida >= tiempoRapido) {
        modificarOpcionActual(false); // Decrementar rápido
        tiempoUltimaEdicionRapida = ahora; // Reinicia el tiempo
      }
    } else {
      tiempoUltimaEdicionRapida = 0; // Resetea si no está presionado
    }
  }
}

void entrarOpcionMenu() {
  if (!editando) {
    editando = true; // Activa el modo de edición
    tiempoUltimaEdicion = millis(); // Reinicia el temporizador de inactividad
  } else {
    editando = false; // Desactiva el modo de edición
    if (opcionMenu == 4) { // Si se selecciona la opción de guardar
      guardarConfiguracion(); // Llama a la función para guardar la configuración
      lcd.clear();
      lcd.print("Guardado!");
      delay(500);
    }
    lcd.clear();
    enMenu = false; // Sale del menú
  }
}

// Nueva función para guardar la configuración
void guardarConfiguracion() {
  // Aquí puedes agregar el código necesario para guardar la configuración
  // Por ejemplo, guardar en la EEPROM si es necesario
}

void modificarOpcionActual(bool subir) {
  switch (opcionMenu) {
    case 0:
      mostrarEnGalones = !mostrarEnGalones; // Cambia entre litros y galones
      break;
    case 1:
      // Incremento y decremento rápido para distancia máxima
      if (subir) {
        if (usarMetros) {
          distanciaMaxima += 1; // Aumenta 1 metro
        } else {
          distanciaMaxima += 5; // Aumenta 5 cm
        }
      } else {
        if (usarMetros) {
          distanciaMaxima -= 1; // Disminuye 1 metro
        } else {
          distanciaMaxima -= 5; // Disminuye 5 cm
        }
        if (distanciaMaxima < 0) distanciaMaxima = 0; // Evita valores negativos
      }
      break;
    case 2:
      // Incremento y decremento de uno en uno para capacidad
      if (subir) {
        capacidadLitros++; // Aumenta la capacidad en litros de uno en uno
      } else {
        capacidadLitros--; // Disminuye la capacidad en litros de uno en uno
        if (capacidadLitros < 0) capacidadLitros = 0; // Evita valores negativos
      }
      break;
    case 3:
      usarMetros = !usarMetros; // Alterna entre metros y centímetros
      // Convertir la distancia máxima a la unidad correspondiente
      if (usarMetros) {
        distanciaMaxima = cmAMetros(distanciaMaxima); // Convertir a metros
      } else {
        distanciaMaxima = metrosACm(distanciaMaxima); // Convertir a centímetros
      }
      break;
  }
}

float litrosAGalones(float litros) {
  return litros / 3.785; // Conversión de litros a galones
}

float galonesALitros(float galones) {
  return galones * 3.785; // Conversión de galones a litros
}

void mostrarDatosLCD(int porcentaje, float temperatura, int distancia, float litros, float galones) {
  lcd.backlight();
  lcd.clear();
  
  // Mostrar porcentaje de llenado y temperatura en la primera línea
  lcd.setCursor(0, 0);
  lcd.print("TQ:");
  lcd.print(porcentaje);
  lcd.print("% ");
  lcd.print("Temp:");
  lcd.print(temperatura, 1); // Muestra la temperatura con un decimal
  lcd.print("C");

  // Mostrar distancia en la segunda línea
  lcd.setCursor(0, 1);
  if (distancia <= distanciaMinima) {
    lcd.print("      Lleno");
  } else if (distancia >= distanciaMaxima) {
    lcd.print("      Vacio");
  } else {
    lcd.print("Dist: ");
    lcd.print(distancia);
    lcd.print(usarMetros ? " m" : " cm");
  }

  // Mostrar volumen en litros o galones
  lcd.setCursor(0, 1);
  if (mostrarEnGalones) {
    lcd.print(galones, 1);
    lcd.print("G");
  } else {
    lcd.print(litros, 1);
    lcd.print("L");
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
      float distancia = duracion * 0.0343 / 2; // Calcula la distancia en cm
      if (usarMetros) {
        distancia = cmAMetros(distancia); // Convierte a metros si es necesario
      }
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

// Funciones de conversión
float cmAMetros(float cm) {
  return cm / 100.0; // Conversión de centímetros a metros
}

float metrosACm(float metros) {
  return metros * 100.0; // Conversión de metros a centímetros
}
