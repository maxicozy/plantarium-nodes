#include <Arduino.h>

#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include <NTPClient.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <Wire.h>

#include <Hash.h>

#define MODULES_LENGTH 6

const char* host = "192.168.2.112";
const uint16_t port = 3030;

/*const char* host = "plantarium.srv1.hfgiot.cloud";
const uint16_t port = 443;*/

const char* ssid = "ssid";
const char* pass = "pass";


//positionsdefinitionen der module im system
String modules[MODULES_LENGTH] = {"", "herbs", "", "", "tomatos", "chilis"};

//i2c addresse des led moduls
int ledAddr = 6;

byte receiveData[10];

bool ledState;

int lastWaterLevel;
float lastPhVal;
float lastTdsVal;

//aktuelle Zeitzone
const long utcOffsetInSeconds = 3600;

const int offTime = 0;
const int duration = 8;

int timeStamp;

//hier wird die current time gespeichert, stunden, minuten und sekunden an der jeweiligen stelle im array
int ct[3] = {0, 0, 0};
int lh;
int lm;

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

//von der websocket library
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      {
      Serial.printf("[WSc] Connected to url: %s\n",  payload);
      }
      break;
    case WStype_TEXT:
       
      Serial.printf("[WSc] get text: %s\n", payload);
      break;
      
    case WStype_BIN:
    
      Serial.printf("[WSc] get binary length: %u\n", length);
      hexdump(payload, length);
      break;
      
    case WStype_PING:
      // pong will be send automatically
      Serial.printf("[WSc] get ping\n");
      break;
            
    case WStype_PONG:
      // answer to a ping we send
      Serial.printf("[WSc] get pong\n");
      break;
      
    }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  timeClient.begin();

  Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  for(uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFiMulti.addAP(ssid, pass);
  
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  
  Serial.println("connected to wifi, connecting to server.");

  //der header ist mit dem backend server vereinbart
  webSocket.setExtraHeaders("Sec-WebSocket-Protocol: plantarium");

  //so sieht irgendwann vllt mal die authorization aus damit das backend weiß von welchem garten die daten kommen
  //webSocket.setExtraHeaders("Authorization: 1");
  webSocket.beginSSL(host, port);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);  
  
}

void loop() {
  //versorgt das zeit-array mit neuen daten aus dem internet
  updateTime();
  
  webSocket.loop();

  //checkt ob eine stunde um ist um sensordaten im topf abzufragen
  if (checkAfter(0)){
    for(int i = 0; i < MODULES_LENGTH; i++) {

      if (modules[i] != "") {
        //wenn an der stelle im array ein modul ist werden jetzt daten abgefragt,
        requestSlaveData(i, 10);

        //in ein object verpackt
        DynamicJsonDocument sendDoc (1024);

        sendDoc["time"] = timeStamp;
        sendDoc["plant"] = modules[i];
        sendDoc["position"] = i;
        sendDoc["waterLevel"] = lastWaterLevel;
        sendDoc["ph"] = lastPhVal;
        sendDoc["tds"] = lastTdsVal;
        sendDoc["messageType"] = "sensorData";

        char string[128];
        serializeJson(sendDoc, string);

        //und ans backend gesendet
        webSocket.sendTXT(string);
      }
    }
  }
  
  //checkt ob eine minute um ist um wasserstand im topf abzufragen
  if (checkAfter(1)) {
    for(int i = 0; i < MODULES_LENGTH; i++) {

      if (modules[i] != "") {
        //wenn an der stelle im array ein modul ist werden jetzt daten abgefragt,
        requestSlaveData(i, 2);

        //in ein object verpackt
        DynamicJsonDocument sendDoc (1024);

        sendDoc["time"] = timeStamp;
        sendDoc["plant"] = modules[i];
        sendDoc["position"] = i;
        sendDoc["waterLevel"] = lastWaterLevel;
        sendDoc["messageType"] = "sensorData";

        char string[128];
        serializeJson(sendDoc, string);

        //und ans backend gesendet
        webSocket.sendTXT(string);
      }
    }
  }

  ledTimer();
  
  delay(500);
}

void updateTime() {
  //hier werden unsere zeitvariablen mit neuen daten aus dem internet upgedatet
  timeClient.update();
  ct[0] = timeClient.getHours();
  ct[1] = timeClient.getMinutes();
  ct[2] = timeClient.getSeconds();
  timeStamp = timeClient.getEpochTime();
}

//diese funktion checkt ob seit dem letzten mal checken eine minute bzw stunde vergangen ist
bool checkAfter(int unit) {
  //je nach angegebenem unit wird eine andere variable verwendet
  int lastUnit;
  if (unit == 0) {
    lastUnit = lh;
  } else if (unit == 1) {
    lastUnit = lm;
  }
  if (ct[unit] == lastUnit) {
    return false;
  } else {
    if (unit == 0) {
      lh = ct[unit];
    } else if (unit == 1) {
      lm = ct[unit];
    }
    return true;
  }
}

void ledTimer() {
  if (ct[0] >= offTime && ct[0] < offTime + duration && ledState == true) {
    Wire.beginTransmission(ledAddr);
    Wire.write(1);
    Wire.endTransmission();
    ledState = false;
    Serial.println(ledState);
  }
  if (ct[0] >= offTime + duration && ledState == false) {
    Wire.beginTransmission(ledAddr);
    Wire.write(2);
    Wire.endTransmission();
    ledState = true ;
    Serial.println(ledState);
  }
}

void requestSlaveData(int address, int bytes) {
  Wire.requestFrom(address, bytes);
  int receiveWaterLevel;
  receiveData[0] = Wire.read();
  receiveData[1] = Wire.read(); 

  //da man per i2c nur einzelne bytes sende kann, muss man diese wieder zu einem int zusammensetzen
  receiveWaterLevel = receiveData[0];
  receiveWaterLevel = (receiveWaterLevel << 8) | receiveData[1];

  //sanity check ob der wert sinn macht
  if (receiveWaterLevel <= 1000) {
    lastWaterLevel = receiveWaterLevel;
  }
  //ph und ec sensorwerte werden über einen union wert mit einem buffer gleichgeseutzt, hier muss der buffer wieder als float wert ausgelesen werden
  union floatToBytes {
  
    char buffer[4];
    float value;
      
  } ph, tds;

  for (int i = 0; i < 4; i++){
    ph.buffer[i] = Wire.read();    
  }
  lastPhVal = ph.value;

  for (int i = 0; i < 4; i++){
    tds.buffer[i] = Wire.read();    
  }
  lastTdsVal = tds.value;
  
}
