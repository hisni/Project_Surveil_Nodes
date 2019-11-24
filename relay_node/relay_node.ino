#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <Hash.h>

WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 19800;
NTPClient timeClient(ntpUDP,"pool.ntp.org",utcOffsetInSeconds);

ESP8266WiFiMulti WiFiMulti;
SocketIOclient socketIO;

// Internet router credentials
const char* ssid = "ESP8266Server";
const char* password = "";

ESP8266WebServer server(80);

void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case sIOtype_DISCONNECT:
            Serial.printf("[IOc] Disconnected!\n");
            break;
        case sIOtype_CONNECT:
            Serial.printf("[IOc] Connected to url: %s\n", payload);
            break;
        case sIOtype_EVENT:
            Serial.printf("[IOc] get event: %s\n", payload);
            break;
        case sIOtype_ACK:
            Serial.printf("[IOc] get ack: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_ERROR:
            Serial.printf("[IOc] get error: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_BINARY_EVENT:
            Serial.printf("[IOc] get binary: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_BINARY_ACK:
            Serial.printf("[IOc] get binary ack: %u\n", length);
            hexdump(payload, length);
            break;
    }
}

void setup() {
  pinMode(14, OUTPUT);
  Serial.begin(74880);
  WiFi.mode(WIFI_AP_STA);
  setupAccessPoint();
}

// Handling the / root web page from my server
void handle_index() {
  server.send(200, "text/plain", "Get the f**k out from my server!");
}

// Handling the /feed page from my server
void handle_feed() {
  String rnID = "rn1001";
  String enID = server.arg("enID");
  String t = server.arg("temp");
  String h = server.arg("hum");
  String p = server.arg("pres");
  String st = server.arg("Stable");

  if( st.equals("Stable") ){
    digitalWrite(14, LOW);
  }else{
    digitalWrite(14, HIGH);  
  }
  
  Serial.print("Temperature : "+t);
  Serial.print(" | Humidity : "+h); 
  Serial.print(" | Pressure : "+p);
  Serial.println(" | Stability : "+st); 
  server.send(200, "text/plain", "Data Received");
  setupStMode(rnID, enID, t, h, p, st);
}

void setupAccessPoint(){
  Serial.println("** SETUP ACCESS POINT **");
  Serial.println("- disconnect from any other modes");
  WiFi.disconnect();
  Serial.println("- start ap with SID: "+ String(ssid));
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("- AP IP address is :");
  Serial.print(myIP);
  setupServer();
}

void setupServer(){
  Serial.println("** SETUP SERVER **");
  Serial.println("- starting server :");
  server.on("/", handle_index);
  server.on("/feed", handle_feed);
  server.begin();
};

unsigned long getTimeStamp(){
  timeClient.update();
  unsigned long epcohTime =  timeClient.getEpochTime();
  Serial.println(epcohTime);
  return epcohTime;
}


void setupStMode(String rnID, String enID, String t, String h, String p, String st){
  Serial.println("** SETUP STATION MODE **");
  Serial.println("- disconnect from any other modes");
  WiFi.disconnect();
  Serial.println();
  Serial.println("- connecting to Home Router SID: **********");
  WiFi.begin("Lumia 550", "asd@fgh@qwe");
  timeClient.begin();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("- succesfully connected");
  Serial.println("- starting client");

   DynamicJsonDocument doc(1024);
   JsonArray array = doc.to<JsonArray>();
   
   JsonObject param = array.createNestedObject();
   param["rnID"] = rnID;
   param["enID"] = enID;
   param["tem"] = t;
   param["humid"] = h;
   param["pres"] = p;
   param["Stab"] = String(st);
   param["epoch"] = String(getTimeStamp());

   // JSON to String (serializion)
   String output;
   serializeJson(doc, output);

   HTTPClient http; //Declare object of class HTTPClient

   http.begin("http://192.168.43.140:8000/feed"); //Specify request destination
   http.addHeader("Content-Type", "application/json"); //Specify content-type header
   http.POST(output); //Send the request
   String payload = http.getString(); //Get the response payload
   Serial.println(payload); //Print request response payload
   http.end(); //Close connection
}

void loop() {
  server.handleClient();  
}
