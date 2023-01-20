#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "SPIFFS.h"
#include <coap-simple.h>

// Descomentar caso tenha o sensor Dallas DS18B20 
#include <OneWire.h>
#include <DallasTemperature.h>

const char* ssid = "ARRUDA_2G";
const char* password = "DD5W#zeqLa";

// const char* ssid = "763B_Fibra";
// const char* password = "31@dugil";

// Descomentar caso tenha o sensor Dallas DS18B20
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const char* ntpServer = "br.pool.ntp.org";
const long gmtOffset_sec = -14400;
const int daylightOffset_sec = 3600;

#ifdef __cplusplus
  extern "C" {
#endif

  uint16_t temprature_sens_read();

#ifdef __cplusplus
}
#endif

uint16_t temprature_sens_read();


AsyncWebServer server(80);

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port);

// CoAP server endpoint url callback
void callback_light(CoapPacket &packet, IPAddress ip, int port);

// CoAP server do servidor OTA
void callback_server(CoapPacket &packet, IPAddress ip, int port);

// CoAP Chamada para temperatura
void callback_temperatura(CoapPacket &packet, IPAddress ip, int port);

// CoAP chamada para ver data e hora
void callback_time(CoapPacket &packet, IPAddress ip, int port);

void callback_texto(CoapPacket &packet, IPAddress ip, int port);

float temp_esp();
float temp_ambiente();

WiFiUDP udp;
Coap coap(udp);

// Variáveis de estado
bool LEDSTATE = false;
bool SERVERSTATE = false;

// Função que liga/desliga o Led (encontrada na lista de comandos)
void callback_light(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Light] ON/OFF");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  String message(p);

  if (message.equals("0"))
    LEDSTATE = false;
  else if(message.equals("1"))
    LEDSTATE = true;
      
  if (LEDSTATE) {
    digitalWrite(2, HIGH) ; 
    coap.sendResponse(ip, port, packet.messageid, "LIGHT ON");
  } else { 
    digitalWrite(2, LOW) ; 
    coap.sendResponse(ip, port, packet.messageid, "LIGHT OFF");
  }
}
// Função que liga/desliga o servidor Web (encontrada na lista de comandos)
void callback_server(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[SERVER] ON/OFF");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  String message(p);

  if (message.equals("0"))
    SERVERSTATE = false;
  else if(message.equals("1"))
    SERVERSTATE = true;
      
  if (SERVERSTATE) {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "Hi! This is a sample response.");
    });
    AsyncElegantOTA.begin(&server, "teste", "teste");    // Start AsyncElegantOTA
    server.begin();
    Serial.println("HTTP server started");
    coap.sendResponse(ip, port, packet.messageid, "SERVER ON");
  } else {    // Start AsyncElegantOTA
    server.end();
    coap.sendResponse(ip, port, packet.messageid, "SERVER OFF");
  }
}

// Função que retorna a temperatura escolhida(esp/ambiente encontrada na lista de comandos) para o servidor
void callback_temperatura(CoapPacket &packet, IPAddress ip, int port){
  
  char i[packet.options[1].length];
  memcpy(i, packet.options[1].buffer, packet.options[1].length);
  i[packet.options[1].length] = NULL;
  String message(i);

  char str[8];
  float temp = 0;
  if(message =="esp"){
    temp = temp_esp();
  } else if (message == "ambiente"){
    temp = temp_ambiente();
  }
 
  sprintf(str, "%.2f C", temp);
  coap.sendResponse(ip, port, packet.messageid, str);

}

// Função que retorna a data e hora
void callback_time(CoapPacket &packet, IPAddress ip, int port){
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  char timeWeekDay[25];
  strftime(timeWeekDay,25, "%d/%m/%Y - %H:%M:%S", &timeinfo);
  coap.sendResponse(ip, port, packet.messageid, timeWeekDay);

}

void callback_texto(CoapPacket &packet, IPAddress ip, int port){
  File file = SPIFFS.open("/text.txt");
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  char *texto;
  Serial.println("File Content:");
  while(file.available()){
    Serial.write(file.read());
    sprintf(texto, "%s", file.read());
  }
  file.close();
  coap.sendResponse(ip, port, packet.messageid, texto);

}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Coap Response got]");
  
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  Serial.println(p);
}
// Função que retorna a conversão a temperatura em celsius
float temp_esp(){
  return (temprature_sens_read() - 32) / 1.8;
}

// Descomentar caso tenha o sensor Dallas DS18B20
// Função que sensor usado para capitação da tempetura ambiente - sensor: Dallas DS18B20
float temp_ambiente(){
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);

}

void setup(void) {

  pinMode(2,OUTPUT);
  digitalWrite(2, HIGH);
  LEDSTATE = true;

  //Descomentar caso tenha o sensor Dallas DS18B20
  sensors.begin();

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

// Inicializa o SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
}

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // add server url endpoints.
  // can add multiple endpoint urls.
  // exp) coap.server(callback_switch, "switch");
  //      coap.server(callback_env, "env/temp");
  //      coap.server(callback_env, "env/humidity");
  Serial.println("Setup Callback Light");
  coap.server(callback_light, "light");
  coap.server(callback_server, "server");
  coap.server(callback_temperatura, "temp/esp");
  coap.server(callback_temperatura, "temp/ambiente");
  coap.server(callback_time, "time");
  coap.server(callback_texto, "texto");
  

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  coap.start();

   File file = SPIFFS.open("/text.txt");
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  char *texto;
  Serial.println("File Content:");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();

}

void loop(void) {
  delay(1000);
  coap.loop();

}
//criar função que pisca led em x parametro
//mandar valor do paramentro por arquivo

/*
LISTA DE COMANDOS

-LED 
coap-client -m get coap://(arduino ip addr)/light
coap-client -e "1" -m put coap://(arduino ip addr)/light
coap-client -e "0" -m put coap://(arduino ip addr)/light

-SERVER
coap-client -m get coap://(arduino ip addr)/server
coap-client -e "1" -m put coap://(arduino ip addr)/server
coap-client -e "0" -m put coap://(arduino ip addr)/server

-TEMPERATURA
coap-client -m get coap://(arduino ip addr)/temp/esp
coap-client -m get coap://(arduino ip addr)/temp/ambiente - caso tenha

-DATA e HORA
coap-client -m get coap://(arduino ip addr)/time

*/