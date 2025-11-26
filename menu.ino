/*
  Menú + PIN + SD + MQTT para MKR WiFi 1010 + MKR IoT Carrier
  Opciones:
   TOUCH0 -> Opción 1: Activar/Desactivar alarma
   TOUCH1 -> Opción 2: Configurar PIN
   TOUCH2 -> Opción 3: Sonido on/off
   TOUCH3 -> Opción 4: Guardar configuración (sobrescibir)
   TOUCH4 -> Opción 5: Cargar configuración desde SD
  MQTT topics:
   - casa/arduino/comando  (recibir comandos)
   - casa/arduino/estado   (publicar estados)
*/

#include <Arduino_MKRIoTCarrier.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>

MKRIoTCarrier carrier;

//  Estado actual del programa para ejecutar diferentes tareas.
int estado = 0;   // 0 = menú, 1 = ON/OFF, 2 = PIN, etc.


bool mostrarPIN = true;
// PIN por defecto:
int pinCode[4] = {0, 0, 0, 0}; 

int digitoSel = 0; // Dígito seleccionado (0-3)

int pinSelect[4] ={0,0,0,0};

bool alarmaActiva = false; // Estado de alarma


// Sensor PIR
int pir = A5;      // Conectado al pin A5
bool pirState = LOW;
// giroscopio
float Gx, Gy, Gz;

// Activar alarma del sonido o desactivar
bool sonido = true;

// file object
File dataFile;
File file;


//Conf de red inalambrica
char ssid [] = "";
char pass[] = "";

//creates a Wi-Fi client.
WiFiClient wifiClient;
// conectarse el cliente wifi a mqtt cliente
MqttClient mqttClient(wifiClient);


// MQTT topics
const char topicCmd[] = "aimar06_comando";

const char mqttServer[] = "broker.hivemq.com"; // broker.hivemq.com   test.mosquitto.org
int port = 1883;





void setup() {
  pinMode(pir, INPUT);     // Configurar pin PIR como entrada
  Serial.begin(9600);
  CARRIER_CASE = false;
  carrier.begin();

  // iniciar SD 
  if (!SD.begin(SD_CS)) {
    carrier.display.setTextSize(2);
    carrier.display.setCursor(35, 70);
    carrier.display.print("Error del SD");
    while (1);
  }
  conectarWiFi();
  conectarMqtt();
  delay(1500);
}

void loop() {
  //conexión MQTT
  if (mqttClient.connected()) {
    mqttClient.poll();
  }


  // Codigo alarma activada
  if (alarmaActiva) {
    pantallaLogin();
    return;
  }
  
  

  if (estado == 0) {
    menu();
    estado = 25; // asi no se muestra más el menu
  } 
  else if (estado == 1) {
    activarAlarma();
    Serial.println("Estado: Activar Alarma");
  }
  else if (estado == 2) {
    configurarPin();
    Serial.println("Estado: Configurar Pin");
  }
  else if (estado == 3) {
    configurarsonido();
    Serial.println("Estado: Configurar Sonido");
  }


  carrier.Buttons.update();
  
  // Condicion para elegir opciones del menu
  if (estado == 25) {
    if (carrier.Buttons.onTouchDown(TOUCH0)) { //ON/OFF
      estado = 1;
    }
    if (carrier.Buttons.onTouchDown(TOUCH1)) { //Config pin
      estado = 2; 
    }
    
    if (carrier.Buttons.onTouchDown(TOUCH2)) { //Config sonido de alarma
      estado = 3; 
    }
 
    if (carrier.Buttons.onTouchDown(TOUCH3)) { //Guardar la configuracion en el archivo conf.csv 
      guardarConfiguracion();
    }
  }
}

void menu() {
  carrier.display.fillScreen(ST77XX_BLACK); //Color del fondo de la pantalla
  carrier.display.setTextColor(ST77XX_WHITE); //Color del texto en blanco
  carrier.display.setTextSize(2);
  
  // Dibuja el cuadrado (x, y, ancho, alto, color)
  carrier.display.drawRect(10,180,100,40,ST77XX_WHITE);  //Dibujar un cuadrado
  // Opcion 1
  carrier.display.setCursor(20, 190);
  carrier.display.print("Activar");
  // Opcion 2
  carrier.display.drawRect(0,70,125,40,ST77XX_WHITE);  //Dibujar un cuadrado
  carrier.display.setCursor(2, 80);
  carrier.display.print("Config PIN");
  // Opcion 3
  carrier.display.drawRect(70,20,90,40,ST77XX_WHITE);  //Dibujar un cuadrado
  carrier.display.setCursor(75, 30);
  carrier.display.print("Sonido");
  // Opcion 4
  carrier.display.drawRect(140,70,90,40,ST77XX_WHITE);  //Dibujar un cuadrado
  carrier.display.setCursor(150, 80);
  carrier.display.print("Guardar");
  // Opcion 5
  carrier.display.drawRect(140,180,90,40,ST77XX_WHITE);  //Dibujar un cuadrado
  carrier.display.setCursor(150, 190);
  carrier.display.print("Cargar");
}





void pin() {
  carrier.display.fillScreen(ST77XX_BLACK); //Color del fondo de la pantalla
  carrier.display.setTextColor(ST77XX_WHITE); //Color del texto en blanco
  carrier.display.setTextSize(2);
  
  // Dibuja el cuadrado (x, y, ancho, alto, color)
  carrier.display.drawRect(10,180,50,40,ST77XX_WHITE);  //Dibujar un cuadrado
  // Sumar 1
  carrier.display.setCursor(13, 190);
  carrier.display.print("+1");

  // Restar 1
  carrier.display.drawRect(10,50,50,40,ST77XX_WHITE);  //Dibujar un cuadrado
  carrier.display.setCursor(13, 50);
  carrier.display.print("-1");

  // Confirmar
  carrier.display.drawRect(100,50,120,40,ST77XX_WHITE);  //Dibujar un cuadrado
  carrier.display.setCursor(110, 60);
  carrier.display.print("Siguiente");

  
 // volver al menu
  carrier.display.drawRect(140, 180, 90, 40, ST77XX_WHITE);
  carrier.display.setCursor(150, 190);
  carrier.display.print("Volver");
  
  
  // PIN actual centrado
  carrier.display.setTextSize(3);
  
  carrier.display.drawRect(40, 110, 160, 60, ST77XX_WHITE);
  
  carrier.display.setCursor(57, 115);

  for (int i = 0; i < 4; i++) {
    if (i == digitoSel) {
      carrier.display.setTextColor(ST77XX_GREEN);  // dígito activo
    } else {
      carrier.display.setTextColor(ST77XX_WHITE);  // otros dígitos
    }
    carrier.display.print(pinCode[i]);
    carrier.display.print(" ");
    }


  delay(200);

}




void configurarPin() {
  if (mostrarPIN) {
    pin();
    mostrarPIN = false;
  }

  carrier.Buttons.update();
  // botones para modificar el PIN
  // +1
  if (carrier.Buttons.onTouchDown(TOUCH0)) {
    if (pinCode[digitoSel] < 9) {
      pinCode[digitoSel]++;
    }
    mostrarPIN = true;
  }

  // -1
  if (carrier.Buttons.onTouchDown(TOUCH1)) {
    if (pinCode[digitoSel] > 0) {
      pinCode[digitoSel]--;
    }
    mostrarPIN = true;
  }

  // Cambiar digito activo
  if (carrier.Buttons.onTouchDown(TOUCH2)) {
    digitoSel++;
    if (digitoSel > 3) digitoSel = 0;
    mostrarPIN = true;
  }
  // Volver al menu
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    estado = 0;
    mostrarPIN = true;
    delay(200);
    return;
  }

  delay(80);
}



void activarAlarma() {

  // Comprobar si el PIN configurado es 0000
  bool pinEsDefault = 
    pinCode[0] == 0 &&
    pinCode[1] == 0 &&
    pinCode[2] == 0 &&
    pinCode[3] == 0;

  if (pinEsDefault) {
    // Mostrar mensaje de error
    carrier.display.fillScreen(ST77XX_BLACK);
    carrier.display.setTextColor(ST77XX_RED);
    carrier.display.setTextSize(5);
    carrier.display.setCursor(50, 70);
    carrier.display.print("AVISO");
    carrier.display.setTextSize(3);
    carrier.display.setCursor(3, 130);
    carrier.display.print("TU PIN ES 0000");
    delay(2000);
    estado = 0; // Volver al menu
    return;
  }

  // La alarma se activa aquí
  alarmaActiva = true;
  // Mostrar mensaje de activación
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_GREEN);
  carrier.display.setTextSize(5);
  carrier.display.setCursor(20, 100);
  carrier.display.print("ALARMA ACTIVADA");
  delay(2000);
  carrier.IMUmodule.readGyroscope(Gx, Gy, Gz);
  Serial.println(Gx);
  Serial.println(Gy);
  Serial.println(Gz);



//mostrarPIN=true;

//reiniciar el pin actual y digito sel

}



void pantallaLogin() {

  if (mostrarPIN) {
    dibujarPantallaLogin();
    mostrarPIN = false;
  }

  carrier.Buttons.update();

  // +1 al dígito actual
  if (carrier.Buttons.onTouchDown(TOUCH0)) {
    if (pinSelect[digitoSel] < 9)
      pinSelect[digitoSel]++;
    mostrarPIN = true;
  }

  // -1 al dígito actual
  if (carrier.Buttons.onTouchDown(TOUCH1)) {
    if (pinSelect[digitoSel] > 0)
      pinSelect[digitoSel]--;
    mostrarPIN = true;
  }

  // Cambiar dígito activo
  if (carrier.Buttons.onTouchDown(TOUCH2)) {
    digitoSel++;
    if (digitoSel > 3) digitoSel = 0;
    mostrarPIN = true;
  }

  // Confirmar PIN
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    validarPin();
    mostrarPIN = true;
  }

  // Detectores externos para alertar
  detectorMovimiento();
  giroscopio();

  delay(80);
}

void dibujarPantallaLogin() {
  carrier.display.fillScreen(ST77XX_BLACK);

  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(2);
  carrier.display.setCursor(50, 20);
  carrier.display.print("Introducir PIN");

  carrier.display.drawRect(40, 90, 160, 60, ST77XX_WHITE);
  carrier.display.setCursor(60, 105);
  carrier.display.setTextSize(3);

  for (int i = 0; i < 4; i++) {
    if (i == digitoSel)
      carrier.display.setTextColor(ST77XX_GREEN);
    else
      carrier.display.setTextColor(ST77XX_WHITE);

    carrier.display.print(pinSelect[i]);
    carrier.display.print(" ");
  }

  // Botón confirmar
  carrier.display.setTextSize(2);
  carrier.display.drawRect(140, 180, 80, 40, ST77XX_WHITE);
  carrier.display.setCursor(150, 190);
  carrier.display.print("OK");
}



void validarPin() {

  // Comprobar si coincide
  bool correcto = true;
  for (int i = 0; i < 4; i++) {
    if (pinSelect[i] != pinCode[i]) correcto = false;
  }

  if (correcto) {
    // mostrar un mensaje que el pin es correcto
    carrier.display.fillScreen(ST77XX_BLACK); //Color del fondo de la pantalla
    carrier.display.setTextColor(ST77XX_GREEN); //Color del texto en verde
    carrier.display.setTextSize(4);
    
    carrier.display.setCursor(20, 100);
    carrier.display.print("PIN CORRECTO");
    alarmaActiva = false;
    estado = 0; // volver al menu
    delay(2000);
    // Reiniciar entrada
    for (int i = 0; i < 4; i++) pinSelect[i] = 0;
    digitoSel = 0;
  } else {
    // mostrar un mensaje que el pin es correcto
    carrier.display.fillScreen(ST77XX_BLACK); //Color del fondo de la pantalla
    carrier.display.setTextColor(ST77XX_RED); //Color del texto en rojo
    carrier.display.setTextSize(4);
    carrier.display.setCursor(20, 100);
    carrier.display.print("PIN INCORRECTO");
    delay(2000);
    // Reiniciar entrada
    for (int i = 0; i < 4; i++) pinSelect[i] = 0;
    digitoSel = 0;
  }
}




void detectorMovimiento() {

  carrier.IMUmodule.readGyroscope(Gx, Gy, Gz);
  pirState = digitalRead(pir);
  if (pirState == HIGH) {
    carrier.display.fillScreen(ST77XX_BLACK);
    carrier.display.setTextColor(ST77XX_WHITE);
    carrier.display.setTextSize(2);
    carrier.display.setCursor(20, 100);
    carrier.display.print("Movimiento detectado");
    alarmar();
    delay(3000);
    mostrarPIN=true;
  }

}



void giroscopio() {
  float ActualGx, ActualGy, ActualGz;
  carrier.IMUmodule.readGyroscope(ActualGx, ActualGy, ActualGz);
  //ActualGx+=1.0;
  //ActualGy+=1.0;
  //ActualGz+=1.0;


  float umbral = 1.5; // ajustable, detecta movimiento mínimo


  Serial.println(ActualGx);
  Serial.println(ActualGy);
  Serial.println(ActualGz);


  if (abs(ActualGx - Gx) > umbral || abs(ActualGy - Gy) > umbral || abs(ActualGz - Gz) > umbral) {
    carrier.display.fillScreen(ST77XX_BLACK);
    carrier.display.setTextColor(ST77XX_WHITE);
    carrier.display.setTextSize(2);
    carrier.display.setCursor(20, 100);
    carrier.display.print("Giro detectado");
    alarmar();
    delay(3000);
    mostrarPIN=true;
    // actualizar referencia
    Gx = ActualGx;
    Gy = ActualGy;
    Gz = ActualGz;
  }
}


//Solo se muestra la configuracion del sonido y los paneles
void mostrarConfSonido() {
  carrier.display.fillScreen(ST77XX_BLACK); //Color del fondo de la pantalla
  carrier.display.setTextColor(ST77XX_WHITE); //Color del texto en blanco
  carrier.display.setTextSize(2);

  // Dibuja el cuadrado (x, y, ancho, alto, color)
  carrier.display.drawRect(15,20,200,50,ST77XX_WHITE);  //Dibujar un cuadrado
  carrier.display.setCursor(30,30);
  carrier.display.print("Alternar sonido");
  
  
  carrier.display.setCursor(10,110);
  if (sonido) {
    carrier.display.print("Sonido activado");
  } else {
    carrier.display.print("Sonido desactivado");
  }
  

 // volver al menu
  carrier.display.drawRect(140, 180, 90, 40, ST77XX_WHITE);
  carrier.display.setCursor(150, 190);
  carrier.display.print("Volver");
  
}

//Configurar el sonido, si se aprietan los botones se cambia la confguracion del sonido
void configurarsonido() {
  if (mostrarPIN) {
    mostrarConfSonido();
    mostrarPIN = false;
  }

  carrier.Buttons.update();
  if (carrier.Buttons.onTouchDown(TOUCH2)) {
    if (sonido) {
      sonido=false;
      mostrarPIN=true;
    } else {
      sonido =true;
      mostrarPIN=true;
    }

  }
 
  //Volver al menu
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    estado = 0;
    mostrarPIN = true;
    delay(200);
    return;
  }
    
  delay(80);
}


void alarmar() {

  for (int i=0; i < 5; i++) {
    carrier.leds.setPixelColor(i,255,0,0);
  }
  carrier.leds.show();


  if (sonido) {
    carrier.Buzzer.sound(1000); //turn on the sound buzzer with frequency=1000
    delay(1000); // wait for 1 second
    carrier.Buzzer.noSound(); //turn off the sound buzzer
  }

  delay(2000);
 
  // Apagar las luces
  for (int i=0; i < 5; i++) {
    carrier.leds.setPixelColor(i,0,0,0);
  }
  carrier.leds.show();
}



void guardarConfiguracion() {
  //SI existe el archivo se debe de borrar la antigua configuracion
  if (SD.exists("config.csv")) {
    SD.remove("config.csv");
  }

  File file = SD.open("config.csv", FILE_WRITE);

  if (!file) {
    Serial.println("Error al abrir config.csv para escribir");
    carrier.display.setTextSize(2);
    carrier.display.setCursor(3,50);
    carrier.display.print("Error al abrir archivo");
    return;
  }

  // Guardar PIN
  file.print("pin:");
  file.print(pinCode[0]); file.print(",");
  file.print(pinCode[1]); file.print(",");
  file.print(pinCode[2]); file.print(",");
  file.print(pinCode[3]); file.println();

  // Guardar sonido
  file.print("sonido:");
  file.println(sonido ? 1 : 0);

  file.close();
  Serial.println("Configuración guardada correctamente");
  carrier.display.setTextSize(3);
  carrier.display.setCursor(3,50);
  carrier.display.print("Conf guardada");
  delay (5000);
}



bool leerConfiguracion() {
  File file = SD.open("config.csv");

  if (!file) {
    Serial.println("config.csv no existe. Usando valores por defecto.");
    return false;
  }

  while (file.available()) {

    String line = file.readStringUntil('\n');

    // ---- Leer PIN ----
    if (line.startsWith("pin:")) {
      line.remove(0, 4); // quitar "pin:"

      for (int i = 0; i < 4; i++) {
        int coma = line.indexOf(',');
        if (coma == -1 && i < 3) break;

        pinCode[i] = line.substring(0, coma).toInt();
        line = line.substring(coma + 1);
      }
    }

    // ---- Leer sonido ----
    if (line.startsWith("sonido:")) {
      line.remove(0, 7);
      sonido = (line.toInt() == 1);
    }
  }

  file.close();
  Serial.println("config.csv no existe. Usando valores por defecto.");
  return true;
}



// ------------------------------  MQTT -----------------------------

void conectarWiFi() {
  Serial.print("Conectando a WiFi...");
  int status = WiFi.begin(ssid, pass);
  while (status != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    status = WiFi.status();
  }
  if (status == WL_CONNECTED) {
    Serial.print("WiFi conectado.");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.print("No se pudo conectar a WiFi.");
  }
}

void conectarMqtt() {
  Serial.print("Conectando a MQTT… ");
    if (!mqttClient.connect(mqttServer, port)) {
      Serial.print("Error: ");
      Serial.println(mqttClient.connectError());
      delay(1000);
      return;
    }
    Serial.println(" Conectado!");

    Serial.println("Subscribing to topic: ");
    Serial.println(topicCmd);
    mqttClient.subscribe(topicCmd);
}



void onMqttMessage(int messageSize) {
  Serial.println("Esperando mensajes...");
  // we received a message, print out the topic and contents
  Serial.println("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  String mensaje = "";
  while (mqttClient.available()) {
    //Serial.print((char)mqttClient.read());
    char c = (char)mqttClient.read();
    mensaje += c;    // Construye el mensaje carácter a carácter
  }
  mensaje.trim();
  Serial.print(" → Mensaje recibido: ");
  Serial.println(mensaje);


  Serial.println("Mensaje: " + mensaje);


  // Comandos simples (muy directos)
  if (mensaje == "MENU") {
    estado = 0;
  } else if (mensaje == "ALARMA_TOGGLE") {
    estado = 1;
  } else if (mensaje == "SOUND_ON") {
    sonido = true;
    Serial.println("Sonido on");
  } else if (mensaje == "SOUND_OFF") {
    Serial.println("Sonido off");
    sonido = false;
  } else if (mensaje == "SAVE") {
    guardarConfiguracion();
    estado = 0;
  } else if (mensaje == "LOAD") {
    estado = 0;
  }

}


// void enviarEstado() {
//   // Mandar PIN
//   mqttClient.beginMessage(topicPinConfig);
//   mqttClient.print(pinCode[0]);
//   mqttClient.print(",");
//   mqttClient.print(pinCode[1]);
//   mqttClient.print(",");
//   mqttClient.print(pinCode[2]);
//   mqttClient.print(",");
//   mqttClient.print(pinCode[3]);
//   mqttClient.endMessage();

//   // Mandar sonido
//   mqttClient.beginMessage(topicSonido);
//   mqttClient.print(sonido);
//   mqttClient.endMessage();
// }
