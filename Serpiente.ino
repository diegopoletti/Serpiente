#include <WiFi.h>                // Biblioteca para manejar WiFi en ESP32
#include <WebServer.h>           // Biblioteca para crear un servidor web
#include <ESPmDNS.h>             // Biblioteca para mDNS (para OTA)
#include <ArduinoOTA.h>          // Biblioteca para actualizaciones OTA
#include "AudioFileSourceSD.h"   // Biblioteca para fuente de audio desde la tarjeta SD
#include "AudioGeneratorMP3.h"   // Biblioteca para generar audio MP3
#include "AudioOutputI2SNoDAC.h" // Biblioteca para salida de audio sin DAC
#include "SD.h"                  // Biblioteca para manejar la tarjeta SD
#include "SPI.h"                 // Biblioteca para comunicación SPI

#define EnableA 15  // Pin para habilitar motores derecha
#define EnableB 4   // Pin para habilitar motores izquierda
#define IN1 13      // Pin IN1 del L298N para motores derecha
#define IN2 12      // Pin IN2 del L298N para motores derecha
#define IN3 14      // Pin IN3 del L298N para motores izquierda
#define IN4 27      // Pin IN4 del L298N para motores izquierda

#define SCK 18   // Pin de reloj para SPI
#define MISO 19  // Pin de salida de datos para SPI
#define MOSI 23  // Pin de entrada de datos para SPI
#define CS 5     // Pin de selección de chip para SPI

const char* ssid = "RobotWiFi";     // Nombre de la red WiFi del robot
const char* password = "password";  // Contraseña de la red WiFi (para OTA)
WebServer server(80);               // Creación del servidor web en el puerto 80

String comando;             // Almacena el comando del estado de la aplicación
int velocidadCoche = 800;   // Velocidad del coche (400 - 1023)
int coeficiente_velocidad = 3; // Coeficiente para ajustar la velocidad en giros

AudioGeneratorMP3 *mp3;       // Generador de audio MP3
AudioFileSourceSD *fuente;    // Fuente de archivo de audio desde la tarjeta SD
AudioOutputI2SNoDAC *salida;  // Salida de audio sin DAC
unsigned long ultimoTiempoSonido = 0; // Último tiempo en que se reprodujo un sonido
const unsigned long intervaloSonido = 10000;  // Intervalo de 10 segundos entre sonidos
int cantidadArchivosAudio = 22; // Cantidad de archivos de audio en la SD (configurable)

bool OTAhabilitado = false; // Variable para habilitar/deshabilitar OTA

void setup() {
  pinMode(EnableA, OUTPUT); // Configura EnableA como salida
  pinMode(EnableB, OUTPUT); // Configura EnableB como salida
  pinMode(IN1, OUTPUT);     // Configura IN1 como salida
  pinMode(IN2, OUTPUT);     // Configura IN2 como salida
  pinMode(IN3, OUTPUT);     // Configura IN3 como salida
  pinMode(IN4, OUTPUT);     // Configura IN4 como salida
  
  Serial.begin(115200);     // Inicia la comunicación serial a 115200 baudios
  
  WiFi.mode(WIFI_AP_STA);   // Configura el WiFi en modo AP y estación
  WiFi.softAP(ssid);        // Inicia el punto de acceso WiFi
  WiFi.begin(ssid, password); // Conecta a la red WiFi (para OTA)

  while (WiFi.waitForConnectResult() != WL_CONNECTED) { // Espera la conexión WiFi
    Serial.println("Conexión fallida! Reiniciando..."); // Mensaje de error
    delay(5000);            // Espera 5 segundos
    ESP.restart();          // Reinicia el ESP32
  }

  IPAddress miIP = WiFi.softAPIP(); // Obtiene la dirección IP del punto de acceso
  Serial.print("Dirección IP del punto de acceso: "); // Imprime mensaje
  Serial.println(miIP);     // Imprime la dirección IP
 
  server.on("/", HTTP_handleRoot); // Configura la ruta raíz del servidor
  server.on("/toggleOTA", HTTP_toggleOTA); // Configura la ruta para activar/desactivar OTA
  server.onNotFound(HTTP_handleRoot); // Configura el manejo de rutas no encontradas
  server.begin();           // Inicia el servidor web
  
  if (!SD.begin(CS)) {      // Inicializa la tarjeta SD
    Serial.println("Error al inicializar la tarjeta SD"); // Mensaje de error
    return;                 // Termina la configuración si hay error
  }
  
  mp3 = new AudioGeneratorMP3();      // Crea una instancia del generador de audio MP3
  salida = new AudioOutputI2SNoDAC(); // Crea una instancia de la salida de audio sin DAC
  fuente = new AudioFileSourceSD();   // Crea una instancia de la fuente de audio desde la tarjeta SD

  initOTA();                // Inicializa la configuración OTA
}

void initOTA() {
  ArduinoOTA.setHostname("RobotESP32"); // Establece el nombre de host para OTA

  ArduinoOTA.onStart([]() { // Función que se ejecuta al inicio de OTA
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Inicio de actualización " + type); // Mensaje de inicio
  });
  
  ArduinoOTA.onEnd([]() { // Función que se ejecuta al finalizar OTA
    Serial.println("\nFin"); // Mensaje de finalización
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { // Función para mostrar el progreso
    Serial.printf("Progreso: %u%%\r", (progress / (total / 100))); // Muestra el progreso
  });
  
  ArduinoOTA.onError([](ota_error_t error) { // Función que se ejecuta en caso de error
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Fallo de autenticación");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Fallo de inicio");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Fallo de conexión");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Fallo de recepción");
    else if (error == OTA_END_ERROR) Serial.println("Fallo de finalización");
  });

  ArduinoOTA.begin(); // Inicia el servicio OTA
}

void adelante() {
  digitalWrite(IN1, LOW);   // Configura IN1 en bajo
  digitalWrite(IN2, HIGH);  // Configura IN2 en alto
  analogWrite(EnableA, velocidadCoche); // Aplica velocidad al motor derecho

  digitalWrite(IN3, LOW);   // Configura IN3 en bajo
  digitalWrite(IN4, HIGH);  // Configura IN4 en alto
  analogWrite(EnableB, velocidadCoche); // Aplica velocidad al motor izquierdo
}

void atras() {
  digitalWrite(IN1, HIGH);  // Configura IN1 en alto
  digitalWrite(IN2, LOW);   // Configura IN2 en bajo
  analogWrite(EnableA, velocidadCoche); // Aplica velocidad al motor derecho

  digitalWrite(IN3, HIGH);  // Configura IN3 en alto
  digitalWrite(IN4, LOW);   // Configura IN4 en bajo
  analogWrite(EnableB, velocidadCoche); // Aplica velocidad al motor izquierdo
}

void derecha() {
  digitalWrite(IN1, HIGH);  // Configura IN1 en alto
  digitalWrite(IN2, LOW);   // Configura IN2 en bajo
  analogWrite(EnableA, velocidadCoche); // Aplica velocidad al motor derecho

  digitalWrite(IN3, LOW);   // Configura IN3 en bajo
  digitalWrite(IN4, HIGH);  // Configura IN4 en alto
  analogWrite(EnableB, velocidadCoche); // Aplica velocidad al motor izquierdo
}

void izquierda() {
  digitalWrite(IN1, LOW);   // Configura IN1 en bajo
  digitalWrite(IN2, HIGH);  // Configura IN2 en alto
  analogWrite(EnableA, velocidadCoche); // Aplica velocidad al motor derecho

  digitalWrite(IN3, HIGH);  // Configura IN3 en alto
  digitalWrite(IN4, LOW);   // Configura IN4 en bajo
  analogWrite(EnableB, velocidadCoche); // Aplica velocidad al motor izquierdo
}

void detener() {
  digitalWrite(IN1, LOW);   // Configura IN1 en bajo
  digitalWrite(IN2, LOW);   // Configura IN2 en bajo
  analogWrite(EnableA, 0);  // Aplica velocidad 0 al motor derecho

  digitalWrite(IN3, LOW);   // Configura IN3 en bajo
  digitalWrite(IN4, LOW);   // Configura IN4 en bajo
  analogWrite(EnableB, 0);  // Aplica velocidad 0 al motor izquierdo
}

void reproducirRespuestaAleatoria() {
  char ruta[15];            // Buffer para almacenar la ruta del archivo
  int numeroRespuesta = random(1, cantidadArchivosAudio + 1); // Genera un número aleatorio
  snprintf(ruta, sizeof(ruta), "/resp%d.mp3", numeroRespuesta); // Formatea la ruta del archivo
  reproducirAudio(ruta);    // Llama a la función de reproducción de audio
}

void reproducirAudio(const char *ruta) {
  if (!SD.exists(ruta)) {   // Verifica si el archivo existe en la tarjeta SD
    Serial.println("Archivo no encontrado"); // Mensaje de error
    return;                 // Termina la función si el archivo no existe
  }

  if (!fuente->open(ruta)) { // Abre el archivo de audio
    Serial.println("Error al abrir el archivo"); // Mensaje de error
    return;                 // Termina la función si no se puede abrir el archivo
  }

  mp3->begin(fuente, salida); // Inicia la reproducción del audio
}

void loop() {
  if (OTAhabilitado) {      // Si OTA está habilitado
    ArduinoOTA.handle();    // Maneja las actualizaciones OTA
  }
  
  server.handleClient();    // Maneja las solicitudes del cliente web
  
  unsigned long tiempoActual = millis(); // Obtiene el tiempo actual
  if (tiempoActual - ultimoTiempoSonido >= intervaloSonido) { // Verifica si es tiempo de reproducir
    reproducirRespuestaAleatoria(); // Reproduce un sonido aleatorio
    ultimoTiempoSonido = tiempoActual; // Actualiza el último tiempo de reproducción
  }
  
  comando = server.arg("State"); // Obtiene el comando del argumento "State"
  if (comando == "F") adelante();       // Si el comando es "F", va hacia adelante
  else if (comando == "B") atras();     // Si el comando es "B", va hacia atrás
  else if (comando == "L") izquierda(); // Si el comando es "L", gira a la izquierda
  else if (comando == "R") derecha();   // Si el comando es "R", gira a la derecha
  else if (comando == "S") detener();   // Si el comando es "S", se detiene
  else if (comando == "0") velocidadCoche = 400; // Ajusta la velocidad a 400
  else if (comando == "1") velocidadCoche = 470; // Ajusta la velocidad a 470
  else if (comando == "2") velocidadCoche = 540; // Ajusta la velocidad a 540
  else if (comando == "3") velocidadCoche = 610; // Ajusta la velocidad a 610
  else if (comando == "4") velocidadCoche = 680; // Ajusta la velocidad a 680
  else if (comando == "5") velocidadCoche = 750; // Ajusta la velocidad a 750
  else if (comando == "6") velocidadCoche = 820; // Ajusta la velocidad a 820
  else if (comando == "7") velocidadCoche = 890; // Ajusta la velocidad a 890
  else if (comando == "8") velocidadCoche = 960; // Ajusta la velocidad a 960
  else if (comando == "9") velocidadCoche = 1023; // Ajusta la velocidad a 1023
  
  if (mp3->isRunning()) {   // Verifica si el MP3 está reproduciéndose
    if (!mp3->loop()) {     // Procesa el audio
      mp3->stop();          // Detiene la reproducción si ha terminado
    }
  }
}

void HTTP_handleRoot() {
  if (server.hasArg("State")) { // Verifica si hay un argumento "State"
    Serial.println(server.arg("State")); // Imprime el valor del argumento
  }
  server.send(200, "text/html", ""); // Envía una respuesta vacía
  delay(1);                   // Pequeña pausa
}

void HTTP_toggleOTA() {
  OTAhabilitado = !OTAhabilitado; // Cambia el estado de OTA
  String respuesta = OTAhabilitado ? "OTA activado" : "OTA desactivado"; // Prepara la respuesta
  server.send(200, "text/plain", respuesta); // Envía la respuesta
}