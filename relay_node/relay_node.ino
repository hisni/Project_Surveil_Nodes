#include <TinyGPS++.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <Hash.h>

WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 19800;
NTPClient timeClient(ntpUDP,"pool.ntp.org",utcOffsetInSeconds);

TinyGPSPlus gps;  // The TinyGPS++ object

SoftwareSerial ss(D2, D1);

ESP8266WiFiMulti WiFiMulti;
SocketIOclient socketIO;

// Internet router credentials
const char* ssid = "ESP8266Server";
const char* password = "";

float latitude , longitude;
int year , month , date, hour , minute , second;
String date_str , time_str , lat_str , lng_str;
int pm;

int count = 0;

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
  Serial.begin(9600);
  ss.begin(9600);
  WiFi.mode(WIFI_AP_STA);
  setupAccessPoint();
}

// Handling the / root web page from my server
void handle_index() {
  server.send(200, "text/plain", "This Relay Node Server!");
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
    digitalWrite(D4, LOW);
  }else{
    digitalWrite(D4, HIGH);
    count++;
    if( count == 1 ){
      sendSMS("Box is not stabel "); 
    }
  }

  if( t.toInt()<20 || t.toInt()>30 ){
    digitalWrite(D4, HIGH);
    count++;
    if( count == 1 ){
      sendSMS("Temperature is Unstable in " + enID); 
    }
  }else{
    digitalWrite(D4, LOW);
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
  WiFi.begin("Lumia 550", "asdf@fdsa");
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

  delay(500);
  readgps();

   if( lat_str && lng_str ){
      param["lat"] = lat_str;
      param["lng"] = lng_str;
   }

   // JSON to String (serializion)
   String output;
   serializeJson(doc, output);

   HTTPClient http; //Declare object of class HTTPClient

   http.begin("http://192.168.137.180:8000/feed"); //Specify request destination
   http.addHeader("Content-Type", "application/json"); //Specify content-type header
   http.POST(output); //Send the request
   String payload = http.getString(); //Get the response payload
   Serial.println(payload); //Print request response payload
   http.end(); //Close connection

}

void loop() {
  server.handleClient();  
}

void readgps(){
  while (ss.available() > 0)
    if ( gps.encode(ss.read()) ){  
      if (gps.location.isValid()){
        latitude = gps.location.lat();
        lat_str = String(latitude , 6);
        longitude = gps.location.lng();
        lng_str = String(longitude , 6);
      }

      if (gps.date.isValid()){
        date_str = "";
        date = gps.date.day();
        month = gps.date.month();
        year = gps.date.year();

        if (date < 10)
          date_str = '0';
        date_str += String(date);
        date_str += " / ";

        if (month < 10)
          date_str += '0';
        date_str += String(month);
        date_str += " / ";

        if (year < 10)
          date_str += '0';
        date_str += String(year);
      }

      if (gps.time.isValid()){
        
        time_str = "";
        hour = gps.time.hour();
        minute = gps.time.minute();
        second = gps.time.second();

        minute = (minute + 30);
        if (minute > 59){
          minute = minute - 60;
          hour = hour + 1;
        }
        hour = (hour + 5) ;
        if (hour > 23)
          hour = hour - 24;

        if (hour >= 12)
          pm = 1;
        else
          pm = 0;

        hour = hour % 12;

        if (hour < 10)
          time_str = '0';
        time_str += String(hour);

        time_str += " : ";

        if (minute < 10)
          time_str += '0';
        time_str += String(minute);

        time_str += " : ";

        if (second < 10)
          time_str += '0';
        time_str += String(second);

        if (pm == 1)
          time_str += " PM ";
        else
          time_str += " AM ";

      }
    }
    Serial.println(lat_str);
    Serial.println(lng_str);
}

void sendSMS(String str ){
  
    Serial.print("AT");  //Start Configuring GSM Module
    delay(1000);         //One second delay
    
    Serial.println();
    Serial.println("AT+CMGF=1");  // Set GSM in text mode
    
    delay(1000);                  // One second delay
    Serial.println();
    
    Serial.print("AT+CMGS=");     // Enter the receiver number
    Serial.print("\"+94766289828\"");
    Serial.println();
    delay(1000);
    
    Serial.print(str); // SMS body - Sms Text
    delay(1000);
    Serial.println();
    
    Serial.write(26);                //CTRL+Z Command to send text and end session

    delay(500);
//    Serial.end();
}

