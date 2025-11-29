/*
  Opciones:
   TOUCH0 -> Opción 1: Activar/Desactivar alarma
   TOUCH1 -> Opción 2: Configurar PIN
   TOUCH2 -> Opción 3: Sonido on/off
   TOUCH3 -> Opción 4: Guardar configuración (sobrescibir)
   TOUCH4 -> Opción 5: Cargar configuración desde SD
  MQTT topics:
   - alarma/cmd  (recibir comandos)
   - alarma/estado   (publicar estados)
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
const char topicCmd[] = "alarma/cmd";
const char topicEstado[] = "alarma/estado";

const char mqttServer[] = "test.mosquitto.org"; // broker.hivemq.com   test.mosquitto.org
int port = 1883;


WiFiServer server(80);



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
  // Conexon wifi
  conectarWiFi();
  //Conexion servidor mqtt
  conectarMqtt();
  mqttClient.onMessage(onMqttMessage);
  // Iniciar servidor web
  server.begin();
  delay(1500);
}

void loop() {
  // MQTT
  if (!mqttClient.connected()) {
    conectarMqtt();
  }
  mqttClient.poll();


  // Servidor web
  serverweb();



  // Codigo alarma activada
  if (alarmaActiva) {
    pantallaLogin();
    return;
  }
  
  // Alarma
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
    if (carrier.Buttons.onTouchDown(TOUCH4)) { //Cargar la configuracion en el archivo conf.csv 
      leerConfiguracion();
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
  estado=0;
  delay (4000);
}



bool leerConfiguracion() {
  File file = SD.open("config.csv");

  if (!file) {
    Serial.println("config.csv no existe. Usando valores por defecto.");
    carrier.display.setTextSize(3);
    carrier.display.setCursor(3,50);
    carrier.display.print("Conf. no cargada");
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
  Serial.println("Cargado correctamente config.");
  carrier.display.setTextSize(3);
  carrier.display.setCursor(3,50);
  carrier.display.print("Conf. cargada correctamente");
  return true;
  estado = 0;
  delay (3000);
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

    Serial.println("Topic comandos: ");
    Serial.println(topicCmd);
    mqttClient.subscribe(topicCmd);

    Serial.println("Para recibir estado:");
    Serial.println(topicEstado);
}



void onMqttMessage(int messageSize) {
  String mensaje = mqttClient.readString();

  Serial.println("==================================");
  Serial.println("Comando recibido.");
  Serial.print("Tamaño: ");
  Serial.print(messageSize);
  Serial.println(" bytes");
  Serial.print("Contenido → ");
  Serial.println(mensaje);
  Serial.println("==================================");

  // Procesar comandos
  mensaje.trim(); // Elimina espacios y \n al inicio/final

  // Configuracion pin
  //pin1 3

  if (mensaje.startsWith("pin")) {

      // Extraer el indice después de 'pin'
      int id = mensaje.substring(3, 4).toInt();  // 1 dígito

      // Validar índice correcto
      if (id >= 0 && id < 4) {

          // Obtener el número después del espacio
          int espacio = mensaje.indexOf(' ');
          if (espacio != -1) {

              int valor = mensaje.substring(espacio + 1).toInt();

              // Validar valor (0–9)
              if (valor >= 0 && valor <= 9) {

                  pinCode[id] = valor;

                  Serial.print("PIN[");
                  Serial.print(id);
                  Serial.print("] actualizado a: ");
                  Serial.println(valor);

              } else {
                  Serial.println("ERROR: Valor fuera de rango (0-9)");
              }

          } else {
              Serial.println("ERROR: Falta el valor en el comando");
          }

      } else {
          Serial.println("ERROR: Índice fuera de rango (0-3)");
      }
    enviarEstado();
  }

  else if (mensaje == "activar") {
    estado = 1;
    Serial.println("→ Comando: activar");
  }
  else if (mensaje == "SOUND_ON") {
    sonido = true;
    Serial.println("→ Sonido activado por MQTT");
  }
  else if (mensaje == "SOUND_OFF") {
    sonido = false;
    Serial.println("→ Sonido desactivado por MQTT");
  }
  else if (mensaje == "save") {
    guardarConfiguracion();
    estado = 0;
    Serial.println("→ Configuración guardada por MQTT");
  }
  else if (mensaje == "load") {
    leerConfiguracion();
    estado = 0;
    Serial.println("→ Configuración cargada por MQTT");
  }
  else if (mensaje == "estado") {
    enviarEstado();
    Serial.println("→ Enviar estado a cliente MQTT");
  }
  else {
    Serial.println("→ Comando MQTT no reconocido");
    mqttClient.beginMessage(topicEstado);
    mqttClient.print("Error, comando incorrecto, para ver estado: 'estado' ");
    mqttClient.endMessage();
  }
}



void enviarEstado() {
  mqttClient.beginMessage(topicEstado);

  // Enviar estado arduino
  mqttClient.print("Estado Arduino: ");
  mqttClient.print(estado);
  mqttClient.print("\n");
  
  // Enviar pin configurado
  mqttClient.print(pinCode[0]);
  mqttClient.print(",");
  mqttClient.print(pinCode[1]);
  mqttClient.print(",");
  mqttClient.print(pinCode[2]);
  mqttClient.print(",");
  mqttClient.print(pinCode[3]);

  // Mandar sonido
  mqttClient.print("\nSonido:");
  mqttClient.print(sonido);
  
  mqttClient.endMessage();
  Serial.println("==================================================================");
  Serial.println("Estado actual enviado al cliente mqtt");
  Serial.print("Topic estado: ");
  Serial.println(topicEstado);
  Serial.println("==================================================================");
}




// -------------------Servidor web
void serverweb() {

  WiFiClient client = server.available(); 

  if (client) {
    Serial.println("Cliente conectado");

    String req = client.readString();
    client.flush();

    Serial.println("Petición: " + req);

    // --- ACCIONES DE LOS BOTONES ---
    if (req.indexOf("GET /activar") != -1) {
      Serial.println("Opción Activar alarma");
      activarAlarma();
      // Enviar una página HTML
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<body>");
      

      client.println("<meta http-equiv='refresh' content='0; url=/' />");
      
      client.println("</body></html>");
      client.stop();
    }
      
    // -------------------------------------------------
    // Manejo de /pin y /pin?guardar=1&d1=...&d2=... etc.
    // -------------------------------------------------
    if (req.indexOf("GET /pin") != -1) {
      // Extraer la primera línea completa (la línea Request-Line)
      // req contiene ya todas las cabeceras, pero vamos a sacar la primera línea:
      int lineEnd = req.indexOf('\n'); // índice del fin de la primera línea
      String firstLine;
      if (lineEnd != -1) {
        firstLine = req.substring(0, lineEnd);
      } else {
        firstLine = req; // fallback
      }
      firstLine.trim(); // limpia espacios

      // Ejemplo de firstLine: "GET /pin?guardar=1&d1=3&d2=4&d3=1&d4=9 HTTP/1.1"
      Serial.print("Request-Line: ");
      Serial.println(firstLine);

      // Obtener la ruta (entre "GET " y " HTTP")
      int p1 = firstLine.indexOf(' ');
      int p2 = firstLine.indexOf(" HTTP");
      String path = "/";
      if (p1 != -1 && p2 != -1 && p2 > p1) {
        path = firstLine.substring(p1 + 1, p2);
      }
      Serial.print("Path: ");
      Serial.println(path);

      // Si la path NO contiene "guardar=1", mostramos el formulario
      if (path.indexOf("guardar=1") == -1) {
        // Mostrar formulario
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<!DOCTYPE HTML>");
        client.println("<html><body style='font-family:Arial;text-align:center;'>");
        client.println("<h2>Configurar PIN</h2>");
        // OJO: añadimos un campo hidden para asegurar guardar=1
        client.println("<form action='/pin' method='GET'>");
        client.println("<input type='hidden' name='guardar' value='1'>");
        client.println("Digito 1: <input type='number' name='d1' min='0' max='9'><br><br>");
        client.println("Digito 2: <input type='number' name='d2' min='0' max='9'><br><br>");
        client.println("Digito 3: <input type='number' name='d3' min='0' max='9'><br><br>");
        client.println("Digito 4: <input type='number' name='d4' min='0' max='9'><br><br>");
        client.println("<input type='submit' value='Guardar PIN' style='width:200px;height:50px;font-size:20px;'>");
        client.println("</form></body></html>");
        client.stop();
        return; // importante salir para no procesar la sección "guardar" a continuación
      }

      // Si llegamos aquí, path contiene guardar=1 -> procesar parámetros
      // Buscamos los parámetros en la parte de query (después de '?')
      int qpos = path.indexOf('?');
      String query = "";
      if (qpos != -1) {
        query = path.substring(qpos + 1); // todo lo que hay después de '?'
      }
      Serial.print("Query: ");
      Serial.println(query);

      // Función lambda corta para extraer el valor de un parámetro (si existe)
      auto getParam = [&](const char* name) -> String {
        String key = String(name) + "=";
        int i = query.indexOf(key);
        if (i == -1) return String(""); // no existe
        int start = i + key.length();
        int amp = query.indexOf('&', start);
        if (amp == -1) amp = query.length();
        return query.substring(start, amp);
      };

      String sd1 = getParam("d1");
      String sd2 = getParam("d2");
      String sd3 = getParam("d3");
      String sd4 = getParam("d4");

      // Si vienen vacíos, toInt() dará 0; si no vienen, también ponemos 0 por seguridad
      int d1 = (sd1.length() > 0) ? sd1.toInt() : 0;
      int d2 = (sd2.length() > 0) ? sd2.toInt() : 0;
      int d3 = (sd3.length() > 0) ? sd3.toInt() : 0;
      int d4 = (sd4.length() > 0) ? sd4.toInt() : 0;

      // Guarda en el array global
      pinCode[0] = constrain(d1, 0, 9);
      pinCode[1] = constrain(d2, 0, 9);
      pinCode[2] = constrain(d3, 0, 9);
      pinCode[3] = constrain(d4, 0, 9);

      Serial.println("Nuevo PIN configurado:");
      Serial.print(pinCode[0]); Serial.print(", ");
      Serial.print(pinCode[1]); Serial.print(", ");
      Serial.print(pinCode[2]); Serial.print(", ");
      Serial.println(pinCode[3]);

      // Respuesta de confirmación y volver al menú
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<html><body style='text-align:center;font-family:Arial;'>");
      client.println("<h2>PIN Actualizado Correctamente</h2>");
      client.print("<p>Nuevo PIN: ");
      client.print(pinCode[0]); client.print(pinCode[1]); client.print(pinCode[2]); client.print(pinCode[3]);
      client.println("</p>");
      client.println("<a href='/'><button style='width:200px;height:50px;font-size:20px;'>Volver al menu</button></a>");
      client.println("</body></html>");
      client.stop();
      return;
    }

    if (req.indexOf("GET /sonido") != -1) {
      Serial.println("Opción alternar conf sonido");

      // Alternar el valor del sonido
      sonido = !sonido;
      // Enviar una página HTML
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<body>");

      // Mostrar el estado actual
      client.print("<script>alert('Sonido = ");
      client.print(sonido ? "ACTIVO" : "DESACTIVADO");
      client.println("');</script>");

      // Después del alert, volver al menú automáticamente
      client.println("<meta http-equiv='refresh' content='0; url=/' />");

      client.println("</body></html>");

      client.stop();
    }

    
    if (req.indexOf("GET /guardar") != -1) {
      Serial.println("Guardar configuracion");

      guardarConfiguracion();

      // Enviar una página HTML
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<body>");

      client.println("<h2>Configuracion guardada en SD</h2>");
      client.println("<a href='/'><button>Volver</button></a><br>");

      client.println("</body></html>");
      client.stop();
    }
    if (req.indexOf("GET /cargar") != -1) {
      leerConfiguracion();

      Serial.println("Cargar configuracion");

      // Enviar una página HTML
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<body>");

      client.println("<h2>Configuracion cargada de SD</h2>");
      client.println("<a href='/'><button>Volver</button></a><br>");
      
      client.println("</body></html>");
      client.stop();
    }

    // -----------------------------
    // DESBLOQUEO DE ALARMA (PIN)
    // -----------------------------
    if (req.indexOf("GET /off")!= -1) {

      // Extraemos primera línea
      int lineEnd = req.indexOf('\n');
      String firstLine = (lineEnd != -1) ? req.substring(0, lineEnd) : req;
      firstLine.trim();
      Serial.print("Request-Line: ");
      Serial.println(firstLine);

      // Obtener la ruta completa (sin "GET " ni " HTTP")
      int p1 = firstLine.indexOf(' ');
      int p2 = firstLine.indexOf(" HTTP");
      String path = "/";
      if (p1 != -1 && p2 != -1 && p2 > p1) {
        path = firstLine.substring(p1 + 1, p2);
      }
      Serial.print("Path: ");
      Serial.println(path);


      if (path.indexOf("guardar=1") == -1) {
        // ---------- MOSTRAR FORMULARIO ----------
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<!DOCTYPE HTML><html><body style='font-family:Arial;text-align:center;'>");
        client.println("<h2>Introduce el PIN</h2>");

        client.println("<form action='/off' method='GET'>");
        client.println("<input type='hidden' name='guardar' value='1'>");

        client.println("Digito 1: <input type='number' name='d1' min='0' max='9'><br><br>");
        client.println("Digito 2: <input type='number' name='d2' min='0' max='9'><br><br>");
        client.println("Digito 3: <input type='number' name='d3' min='0' max='9'><br><br>");
        client.println("Digito 4: <input type='number' name='d4' min='0' max='9'><br><br>");

        client.println("<input type='submit' value='Enviar' style='width:200px;height:50px;font-size:20px;'>");
        client.println("</form></body></html>");
        client.stop();
        return;
      }

      
      int qpos = path.indexOf('?');
      String query = (qpos != -1) ? path.substring(qpos + 1) : "";
      Serial.print("Query: ");
      Serial.println(query);

      // Función auxiliar para obtener parámetros
      auto getParam = [&](String name) {
        name += "=";
        int idx = query.indexOf(name);
        if (idx == -1) return String("");
        int start = idx + name.length();
        int end = query.indexOf('&', start);
        if (end == -1) end = query.length();
        return query.substring(start, end);
      };

      // Leer cada dígito
      int d1 = getParam("d1").toInt();
      int d2 = getParam("d2").toInt();
      int d3 = getParam("d3").toInt();
      int d4 = getParam("d4").toInt();

      // Guardarlos en el array global pinSelect[]
      pinSelect[0] = constrain(d1, 0, 9);
      pinSelect[1] = constrain(d2, 0, 9);
      pinSelect[2] = constrain(d3, 0, 9);
      pinSelect[3] = constrain(d4, 0, 9);

      Serial.println("PIN introducido por el usuario:");
      Serial.print(pinSelect[0]); Serial.print(",");
      Serial.print(pinSelect[1]); Serial.print(",");
      Serial.print(pinSelect[2]); Serial.print(",");
      Serial.println(pinSelect[3]);

      // Validar PIN introducido
      validarPin();

      // Enviar una página HTML
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<body>");

      client.println("<h2>Validando pin...</h2>");
      // Si quieres: tras validar, cambia estado del programa
      if (!alarmaActiva) {
        client.println("<h2>PIN correcto</h2>");
      } else {
        client.println("<h2>PIN incorrecto</h2>");
      }

      delay(2000);
      client.println("<meta http-equiv='refresh' content='0; url=/' />");

      client.println("</body></html>");
      client.stop();

      return;
    }


    if (!alarmaActiva) {
      // --- GENERAR LA PÁGINA HTML ---
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<head><meta name='viewport' content='width=device-width, initial-scale=1'/>");
      client.println("<style>");
      client.println("button { width:200px; height:60px; font-size:24px; margin:10px; }");
      client.println("</style>");
      client.println("</head>");
      client.println("<body style='text-align:center; font-family:Arial;'>");

      client.println("<h1>MENU PRINCIPAL</h1>");

      client.println("<a href='/activar'><button>Activar</button></a><br>");
      client.println("<a href='/pin'><button>Configurar PIN</button></a><br>");
      client.println("<a href='/sonido'><button>Alternar conf sonido alarma</button></a><br>");
      client.println("<a href='/guardar'><button>Guardar configuracion</button></a><br>");
      client.println("<a href='/cargar'><button>Cargar configuracion</button></a><br>");

      client.println("</body>");
      client.println("</html>");

      delay(1);
      client.stop();
    } else {
        // --- Generar pagina web alarma activa ---
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<head><meta name='viewport' content='width=device-width, initial-scale=1'/>");
      client.println("<style>");
      client.println("button { width:200px; height:60px; font-size:24px; margin:10px; }");
      client.println("</style>");
      client.println("</head>");
      client.println("<body style='text-align:center; font-family:Arial;'>");

      client.println("<h1>ALARMA ACTIVADA</h1>");

      client.println("<a href='/off'><button>Desbloquear alarma</button></a><br>");
     
      client.println("</body>");
      client.println("</html>");

      delay(1);
      client.stop();
    }
    Serial.println("Cliente desconectado");
  }

  delay (50);
}
