#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "DHT.h"

//DHT Temperature Sensor init
#define DHTPIN D2       //DHT sensor Connected to D2
#define DHTTYPE DHT22   //DHT 22 (AM2302), AM2321

//BMP Pressure Sensor init
#define BMP_SCK 13
#define BMP_MISO 12
#define BMP_MOSI 11 
#define BMP_CS 10

Adafruit_BMP280 bme; // I2C

//MPU6050 Gyro init
// MPU6050 Slave Device Address
const uint8_t MPU6050SlaveAddress = 0x68;
// Select SDA and SCL pins for I2C communication 
const uint8_t scl = D6;
const uint8_t sda = D7;
// sensitivity scale factor respective to full scale setting provided in datasheet 
const uint16_t AccelScaleFactor = 16384;
const uint16_t GyroScaleFactor = 131;
// MPU6050 few configuration register addresses
const uint8_t MPU6050_REGISTER_SMPLRT_DIV   =  0x19;
const uint8_t MPU6050_REGISTER_USER_CTRL    =  0x6A;
const uint8_t MPU6050_REGISTER_PWR_MGMT_1   =  0x6B;
const uint8_t MPU6050_REGISTER_PWR_MGMT_2   =  0x6C;
const uint8_t MPU6050_REGISTER_CONFIG       =  0x1A;
const uint8_t MPU6050_REGISTER_GYRO_CONFIG  =  0x1B;
const uint8_t MPU6050_REGISTER_ACCEL_CONFIG =  0x1C;
const uint8_t MPU6050_REGISTER_FIFO_EN      =  0x23;
const uint8_t MPU6050_REGISTER_INT_ENABLE   =  0x38;
const uint8_t MPU6050_REGISTER_ACCEL_XOUT_H =  0x3B;
const uint8_t MPU6050_REGISTER_SIGNAL_PATH_RESET  = 0x68;
int16_t AccelX, AccelY, AccelZ, Temperature, GyroX, GyroY, GyroZ;

// AP Wi-Fi credentials
const char* ssid = "ESP8266Server";
const char* password = "";

// Local ESP web-server address
String serverHost = "http://192.168.4.1/feed";
String data;

// DEEP_SLEEP Timeout interval
int sleepInterval = .5;

// DEEP_SLEEP Timeout interval when connecting to AP fails
int failConnectRetryInterval = 2;
int counter = 0;

double h,t;
double Ax, Ay, Az, T, Gx, Gy, Gz;
double temp,p,a;
String st;
double temperature;

// Static network configuration
IPAddress ip(192, 168, 4, 4);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

DHT dht(DHTPIN, DHT22);
WiFiClient client;

void setup() {
  ESP.eraseConfig();
  WiFi.persistent(false);
  Serial.begin(74880);
  Serial.println();
  Serial.println("**************************");
  Serial.println("******** BEGIN ***********");
  Serial.println("***** START SSENSORS *****");
  Serial.println("**************************");
  Serial.println();

  Wire.begin(sda, scl);
  dht.begin();
  bme.begin();
  MPU6050_Init();

  delay(500);
  
  connectWIFI();

}

void loop(){
  readDHTSensor();
  readMPUSensor();
  readBMPSensor();
  processData();
  printData();

  if(WiFi.status() != WL_CONNECTED){
    connectWIFI();
  }
  
  Serial.println("- build DATA stream string");
  buildDataStream();

  Serial.println("- send POST request");
  sendHttpRequest();

  delay(10000);
//  hibernate(sleepInterval);
}

void connectWIFI(){
  Serial.println("- set ESP STA mode");
  WiFi.mode(WIFI_STA);
  Serial.println("- connecting to wifi");
  WiFi.config(ip, gateway, subnet); 
  WiFi.begin(ssid, password);
  Serial.println("");
  while (WiFi.status() != WL_CONNECTED) {
    if(counter > 20){
       Serial.println("- can't connect, going to sleep");    
       hibernate(failConnectRetryInterval);
    }
    delay(500);
    Serial.print(".");
    counter++;
  }
  Serial.println("- wifi connected");
}

void buildDataStream() {
  data = "enID=en1001";
  data += "&temp=";
  data += String(temperature);
  data += "&hum=";
  data += String(h);
  data += "&pres=";
  data += String(p);
  data += "&Stable=";
  data += String(st);
}

void sendHttpRequest() {
  HTTPClient http;
  http.begin(serverHost);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(data);
  http.writeToStream(&Serial);
  http.end();
}

void printData(){
  
    Serial.print("Temperature : "+String(temperature));
    Serial.print(" | Humidity : "+String(h)); 
    Serial.println(" | Pressure : "+String(p)); 

    Serial.print("Gyro ");
    Serial.print("Ax: "); Serial.print(Ax);
    Serial.print(" Ay: "); Serial.print(Ay);
    Serial.print(" Az: "); Serial.print(Az);
    Serial.print(" T: "); Serial.print(T);
    Serial.print(" Gx: "); Serial.print(Gx);
    Serial.print(" Gy: "); Serial.print(Gy);
    Serial.print(" Gz: "); Serial.println(Gz);

    Serial.print("Stability: "); Serial.print(st);
    
    Serial.println();
}

void processData(){
  temperature = (t+temp)/2;

  if( Ax>1.3 || Ax<.7 || Ay>-.1 || Ay<-.7 || Az>-.7 || Az<-103 ){
    st="Not Stable";
  }else{
    st="Stable";
  }
}

void readDHTSensor() {
  h = dht.readHumidity();
  t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    t = 0.00;
    h = 0.00;
  }
}

void readBMPSensor(){
  temp = bme.readTemperature();
  p = bme.readPressure();
  a = bme.readAltitude(1013.25);
  if (isnan(temp) || isnan(p)) {
    temp = 0.00;
    p = 0.00;
  }
}

void readMPUSensor(){
  Read_RawValue(MPU6050SlaveAddress, MPU6050_REGISTER_ACCEL_XOUT_H);
  //divide each with their sensitivity scale factor
  Ax = (double)AccelX/AccelScaleFactor;
  Ay = (double)AccelY/AccelScaleFactor;
  Az = (double)AccelZ/AccelScaleFactor;
  T = (double)Temperature/340+36.53; //temperature formula
  Gx = (double)GyroX/GyroScaleFactor;
  Gy = (double)GyroY/GyroScaleFactor;
  Gz = (double)GyroZ/GyroScaleFactor;
}

//read all 14 register
void Read_RawValue(uint8_t deviceAddress, uint8_t regAddress){
  Wire.beginTransmission(deviceAddress);
  Wire.write(regAddress);
  Wire.endTransmission();
  Wire.requestFrom(deviceAddress, (uint8_t)14);
  AccelX = (((int16_t)Wire.read()<<8) | Wire.read());
  AccelY = (((int16_t)Wire.read()<<8) | Wire.read());
  AccelZ = (((int16_t)Wire.read()<<8) | Wire.read());
  Temperature = (((int16_t)Wire.read()<<8) | Wire.read());
  GyroX = (((int16_t)Wire.read()<<8) | Wire.read());
  GyroY = (((int16_t)Wire.read()<<8) | Wire.read());
  GyroZ = (((int16_t)Wire.read()<<8) | Wire.read());
}

//configure MPU6050
void MPU6050_Init(){
  delay(250);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_SMPLRT_DIV, 0x07);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_PWR_MGMT_1, 0x01);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_PWR_MGMT_2, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_CONFIG, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_GYRO_CONFIG, 0x00);//set +/-250 degree/second full scale
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_ACCEL_CONFIG, 0x00);// set +/- 2g full scale
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_FIFO_EN, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_INT_ENABLE, 0x01);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_SIGNAL_PATH_RESET, 0x00);
  I2C_Write(MPU6050SlaveAddress, MPU6050_REGISTER_USER_CTRL, 0x00);
}

void I2C_Write(uint8_t deviceAddress, uint8_t regAddress, uint8_t data){
  Wire.beginTransmission(deviceAddress);
  Wire.write(regAddress);
  Wire.write(data);
  Wire.endTransmission();
}

void hibernate(int pInterval) {
  WiFi.disconnect();
  ESP.deepSleep(10 * 600000 * pInterval, WAKE_RFCAL);
  delay(100);
}
