/*
 * ESP8266 Boards 2.5.2
 * LOLIN(WEMOS) D1 R2 & mini
 * 1M SPIFFS
 * 
 * by Max Kliuba
 */

#include <GyverButton.h>
#include <Ticker.h>
#include <NTPClient.h>
#include <Timezone.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <FS.h> 
#include <ESP8266FtpServer.h>

#define DEBUG false
#define LOG true


#define EVERY_MS(x) \
  static uint32_t tmr;\
  bool flag = abs(millis() - tmr) >= (x);\
  if (flag) tmr += (x);\
  if (flag)
  

#define TX 1
#define RX 3
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15


#define STATE_LED_PIN D1 // (D1)--[R 1 kOm]--[RED LED]----(GND)
#define BUTTON_PIN D2 // (D2)----[BUTTON]----(GND)
#define WIFI_LED_PIN D4 // (D4)--[R 1 kOm]--[GREEN LED]----(GND)
#define SOIL_MOISTURE_SENSOR_PIN A0 // (A0)----[(S) SENSOR]
#define SOIL_MOISTURE_SENSOR_VCC_PIN D0 // (D0)--[R 100 Om]--/--[MOSFET]--(GND)--/--[(GND) SENSOR]
#define SENSOR_DETECTION_PIN D5 // (D5)----[(VCC CONNECTOR (3.3 V)) SENSOR]--[R 10 kOm (pull down)]--(GND)
#define PUMP_PIN D6 // (D6)--[R 100 Om]--/--[MOSFET]--(GND)--/--[(-) PUMP] --[1N4001 |]--[(+) PUMP]
#define ECHO_PIN D7 // (D7)----[(Echo) HC-SR04+]
#define TRIG_PIN D8 // (D8)----[(Trig) HC-SR04+]
                    // (3.3 V)----[(VCC) HC-SR04+]--/--[(GND) HC-SR04+]----(GND)
                    
const String UUID = (String)WiFi.macAddress(); // 84:F3:EB:C9:33:96


#define OTAUSER "admin"
#define OTAPASSWORD "admin"
#define OTAPATH "/firmware"

const char* ssidAP = "PhytoStream";                     
const char* passwordAP = "password";

String ssidSTA = "TP-LINK TL-WR841N";
String passwordSTA = "07101973";

const byte DNS_PORT = 53;
bool resetButtonPressed = false;
bool disconnectingFlag = false;

#define UPDATE_INTERVAL 60 * 60 * 1000
bool isTimeUpdated = false, isNtpUpdated = false;
unsigned long updateTimeTicker = 0;

int timeZoneOffset = 2 * 60;
byte timeZoneID = 40;
bool isDST = true;

int depthPercent = -1;
int soilMoisturePercent = -1;

#define DRAPH_DATA_AMOUNT 21
String graphDataX[DRAPH_DATA_AMOUNT];
int graphDataY[DRAPH_DATA_AMOUNT];
const byte t[3] = {0, 8, 16};


TimeChangeRule DST_Rule = {"DST", Last, Sun, Mar, 3, 0};    // Daylight time = UTC + timeZoneOffset + 1 hour
TimeChangeRule STD_Rule = {"STD", Last, Sun, Oct, 4, 0};     // Standard time = UTC + timeZoneOffset
Timezone timeZone(DST_Rule, STD_Rule);

Ticker AP_ReconnectTimer;
Ticker STA_ReconnectTimer;
Ticker graphTimer;
Ticker waterTimer;
Ticker waterTimerSecondsBackup;
bool waterTimerState = false;
int waterTimerPeriod = 24;
unsigned long waterTimerSeconds = waterTimerPeriod * 3600;

WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;

IPAddress ipAddress;
WiFiClient wifiClient;
DNSServer dnsServer;
ESP8266WebServer HTTP(80);
ESP8266HTTPUpdateServer httpUpdater;
FtpServer FTP;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

GButton button(BUTTON_PIN, HIGH_PULL, NORM_OPEN);

enum ConnectionType {ACCESS_POINT, WIFI_CLIENT};
enum ConnectionState {DISCONNECTED, AP_CONNECTED, STA_CONNECTED};

ConnectionType CONNECTION_TYPE = ACCESS_POINT;
ConnectionState CONNECTION_STATE = DISCONNECTED;

#include "Classes.h"

Led wifiLed(WIFI_LED_PIN, LOW);
Led stateLed(STATE_LED_PIN);

WateringManager wateringManager(PUMP_PIN);
LiquidTank liquidTank(TRIG_PIN, ECHO_PIN);
SoilMoistureSensor soilMoistureSensor(SOIL_MOISTURE_SENSOR_PIN, SOIL_MOISTURE_SENSOR_VCC_PIN, SENSOR_DETECTION_PIN);


void setup(){
  Serial.begin(115200);
  if(DEBUG){
    Serial.println("START WORK :)");
    Serial.println("MAC: " + UUID);
  }
  SPIFFS.begin();
  HTTP.begin();
  FTP.begin(OTAUSER, OTAPASSWORD);
  WiFi.persistent(false);
  if (WiFi.getAutoConnect() == true){
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
  }
  logger.write(Log::LOG_INFO, "start", "MAC: " + UUID);

  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
   
  httpUpdater.setup(&HTTP, OTAPATH, OTAUSER, OTAPASSWORD);
  
  HTTP.on("/last_sta_ip", [](){
      return HTTP.send(200, "text/plain", readFromFlash("/sta_ip.txt"));
  });

  HTTP.on("/get_uuid", [](){
      return HTTP.send(200, "text/plain", (String)UUID);
  });
  
  HTTP.on("/get_wifi_config", [](){
      return HTTP.send(200, "text/plain", wifiConfigToJson());
  });

  HTTP.on("/set_wifi_config", [](){
      if (!HTTP.hasArg("json")) return HTTP.send(428, "text/plain", "[error]: No json");
      // http://192.168.4.1/set_wifi_config?json={"ssid":"TP-LINK TL-WR841N","password":"07101973"}
       
      String json = HTTP.arg("json");  
      if(wifiConfigFromJson(json)){
        if(CONNECTION_TYPE == WIFI_CLIENT) disconnectingFlag = true;     
        return HTTP.send(200, "text/plain", "successfully updated");
      }
      return HTTP.send(501, "text/plain", "update error");
  });

  HTTP.on("/get_time_config", [](){
      HTTP.send(200, "text/plain", timeConfigToJson());
  });

  HTTP.on("/set_time_config", [](){
      if (!HTTP.hasArg("json")) HTTP.send(428, "text/plain", "[error]: No json");
        
      String json = HTTP.arg("json");
      if(timeConfigFromJson(json)){
        return HTTP.send(200, "text/plain", "successfully updated"); 
      }
      return HTTP.send(501, "text/plain", "update error");
  });

  HTTP.on("/get_watering_config", [](){
      HTTP.send(200, "text/plain", wateringConfigToJson());
  });

  HTTP.on("/set_watering_config", [](){
      if (!HTTP.hasArg("json")) HTTP.send(428, "text/plain", "[error]: No json");
        
      String json = HTTP.arg("json");
      if(wateringConfigFromJson(json)){
        return HTTP.send(200, "text/plain", "successfully updated"); 
      }
      return HTTP.send(501, "text/plain", "update error");
  });

  HTTP.on("/watering", [](){
      //if(depthPercent > liquidTank.getMinDepthPercent())
      {
        wateringManager.autoWatering();
        logger.write(Log::LOG_INFO, "Manual watering [web]", "time: " + (String)wateringManager.getWateringDuration() + "s");
        return HTTP.send(200, "text/plain", "success"); 
      }
      return HTTP.send(501, "text/plain", "fail - the liquid tank is empty");
  });

  HTTP.on("/get_status", [](){
      if (!HTTP.hasArg("time")) return HTTP.send(428, "text/plain", "[error]: No time");
      
      if(!isNtpUpdated && (abs(millis() - updateTimeTicker) >= UPDATE_INTERVAL || !isTimeUpdated)){
        updateTimeTicker = millis();    
        char *end;
        setTime(timeZone.toLocal(strtoul((HTTP.arg("time")).c_str(), &end, 10)));
        logger.write(Log::LOG_INFO, "Time updated [web]");
        if(DEBUG){
          Serial.println("[time now UTC]: " + (String)now());
          Serial.println("[formatted time now UTC ]: " + TimeManager::getTimeNow());
        }
        isTimeUpdated = true;
      }         
      return HTTP.send(200, "text/plain", getJsonStatus());
  });

  HTTP.on("/get_timer", [](){        
      return HTTP.send(200, "text/plain", waterTimerToJson());
  });

  HTTP.on("/set_timer", [](){
      if (!HTTP.hasArg("json")) HTTP.send(428, "text/plain", "[error]: No json");
        
      String json = HTTP.arg("json");
      if(waterTimerFromJson(json)){
        return HTTP.send(200, "text/plain", "successfully updated"); 
      }
      return HTTP.send(501, "text/plain", "update error");
  });
  
  HTTP.on("/file_system", [](){
      return HTTP.send(200, "text/plain", getFileSystemList());
  });
  
  HTTP.onNotFound([](){
      if(!handleFileRead(HTTP.uri())){
        if(!handleFileRead("/page404.html")){
          return HTTP.send(404, "text/plain", "404: Not Found\n[URI]: " + HTTP.uri());
        }
      }
  });

  for(int i = 0; i < DRAPH_DATA_AMOUNT; i++){
    graphDataX[i] = "0000.00.00 00:00:00";
    graphDataY[i] = -1;
  }

  stationConnectedHandler = WiFi.onStationModeGotIP(&onStationConnected);
  stationDisconnectedHandler = WiFi.onStationModeDisconnected(&onStationDisconnected);
 
  AP_ReconnectTimer.attach(5, connectToWifi_AP);
  STA_ReconnectTimer.attach(5, connectToWifi_STA);
  graphTimer.attach(60, graphTick);

  timeClient.setUpdateInterval(UPDATE_INTERVAL);

  String str = readFromFlash("/connection_type.txt");
  if(str.length() != 0){
    CONNECTION_TYPE = (ConnectionType)str.toInt();
  }
  else {
    writeToFlash("/connection_type.txt", (String)CONNECTION_TYPE);
  }
  if(!wifiConfigFromJson(readFromFlash("/wifi_config.txt"))){
    writeToFlash("/wifi_config.txt", wifiConfigToJson());
  }
  if(!timeConfigFromJson(readFromFlash("/time_config.txt"))){
    writeToFlash("/time_config.txt", timeConfigToJson());
  }
  if(!wateringConfigFromJson(readFromFlash("/watering_config.txt"))){
    writeToFlash("/watering_config.txt", wateringConfigToJson());
  }
  if(!graphFromJson(readFromFlash("/graph.txt"))){
    writeToFlash("/graph.txt", graphToJson());
  }
  
  str = readFromFlash("/water_timer_seconds.txt");
  if(str.length() != 0){
    unsigned long temp = atol(str.c_str());

    if(!waterTimerFromJson(readFromFlash("/water_timer.txt"))){
      writeToFlash("/water_timer.txt", waterTimerToJson());
    }

    temp = waterTimerSeconds;
    waterTimerSecondsSave();
  }
  else {
    writeToFlash("/water_timer.txt", waterTimerToJson());
    waterTimerFromJson(readFromFlash("/water_timer.txt"));
  }

  
  button.setDebounce(100);
  delay(100);
  if(!digitalRead(BUTTON_PIN)){
    button.setTimeout(7000);
    resetButtonPressed = true;
  }
  else{
    button.setTimeout(300);
    resetButtonPressed = false;
  }
}


void loop(){
  button.tick();
  
  if(!resetButtonPressed){
    if(CONNECTION_TYPE == ACCESS_POINT){
      dnsServer.processNextRequest();
    }
    HTTP.handleClient();
    FTP.handleFTP();
  
    {
      EVERY_MS(60000){
        checkDepth("auto");
      }
    }
  
    {
      EVERY_MS(60000){
        checkSoilMoisture("auto");
      }
    }
  
    if(wateringManager.tick()){
      checkDepth("rechecking");
      checkSoilMoisture("rechecking");
    }
    
    
    if(button.hasClicks()){
      switch(button.getClicks()){
        case 1:
          checkDepth("manual");
          checkSoilMoisture("manual");
          break;
        case 2:
          wateringManager.autoWatering();
          logger.write(Log::LOG_INFO, "Manual watering [device]", "time: " + (String)wateringManager.getWateringDuration() + "s");
          break;
        case 3:
          disconnect();
          if(CONNECTION_TYPE == ACCESS_POINT){
            CONNECTION_TYPE = WIFI_CLIENT;
          }
          else if(CONNECTION_TYPE == WIFI_CLIENT){
            CONNECTION_TYPE = ACCESS_POINT;
          }
          writeToFlash("/connection_type.txt", (String)CONNECTION_TYPE);
          break;
      } 
    }
    wateringManager.manualWatering(button.isHold());
  
    if(!wateringManager.isWatering() && depthPercent > liquidTank.getMinDepthPercent() && soilMoisturePercent != -1 && soilMoisturePercent <= soilMoistureSensor.getMinSoilMoisturePercent()){
      wateringManager.autoWatering();
      logger.write(Log::LOG_INFO, "Auto watering [device]", "time: " + (String)wateringManager.getWateringDuration() + "s");
    }
  
    displayState();
    displayConnection();
    checkTimeState();
  
    if(disconnectingFlag){
      delay(100);
      disconnectingFlag = false;
      disconnect();
    }
  }
  else{
    if(button.state()){
      stateLed.blink(50, 50);
    }
    else{
      stateLed.off();
      button.setTimeout(300);
      resetButtonPressed = false;
    }
    
    if(button.isHolded()){
      stateLed.off();
      removeFromFlash("/connection_type.txt");
      removeFromFlash("/wifi_config.txt");
      removeFromFlash("/time_config.txt");
      removeFromFlash("/watering_config.txt");
      removeFromFlash("/graph.txt");
      removeFromFlash("/water_timer.txt");
      removeFromFlash("/water_timer_seconds.txt");
      logger.write(Log::LOG_WARN, "Data reset");

      delay(3000);
      ESP.restart();
    }
  }
}
