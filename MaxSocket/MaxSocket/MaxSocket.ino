/*
 * ESP8266 Boards 2.5.2
 * Generic ESP8266 Module
 * 1M (160K SPIFFS)
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
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <FS.h> 
#include <ESP8266FtpServer.h>

#define DEBUG false

#define GPIO0 0
#define GPIO2 2
#define GPIO4 4
#define GPIO5 5
#define GPIO12 12
#define GPIO13 13
#define GPIO14 14
#define GPIO15 15
#define GPIO16 16

#define BUTTON_PIN GPIO13
#define BUTTON_BOOT_PIN GPIO0
#define WIFI_LED_PIN GPIO2
#define OUTPUT_PIN GPIO12
#define OUTPUT_LED_PIN GPIO4
#define TIMER_LED_PIN GPIO5

#define UPDATE_INTERVAL 60 * 60 * 1000

#define OTAUSER "admin"
#define OTAPASSWORD "admin"
#define OTAPATH "/firmware"

const String TYPE = "MaxSocket";
const byte NAMBER_OF_OUTPUTS = 1;
const String UUID = (String)WiFi.macAddress(); // 84:F3:EB:C9:33:96
const char* ssidAP = "MaxSocket";                     
const char* passwordAP = "password";
const String prefix = TYPE + "/" + (String)NAMBER_OF_OUTPUTS + "/" + UUID;

const byte DNS_PORT = 53;

String ssidSTA = "TP-LINK TL-WR841N";
String passwordSTA = "07101973";
String serverMQTT = "mqtt.by";
int portMQTT = 1883;
String loginMQTT = "maks_kliuba";
String passwordMQTT = "43gele29";

String userTopicPrefix = "/user/maks_kliuba/";
String topicDeviceState = userTopicPrefix + prefix + "/device_state";
String topicControllerState = userTopicPrefix + prefix + "/controller_state";
String topicSetTicker = userTopicPrefix + prefix + "/set_ticker";

unsigned long updateTimer = 0;
bool disconnectingFlag = false;
bool isTimeUpdated = false, isNtpUpdated = false;
unsigned long updateTimeTicker = 0;
int timeZoneOffset = 2 * 60;
byte timeZoneID = 40;
byte isDST = 0;

TimeChangeRule DST_Rule = {"DST", Last, Sun, Mar, 3, 0};    // Daylight time = UTC + timeZoneOffset + 1 hour
TimeChangeRule STD_Rule = {"STD", Last, Sun, Oct, 4, 0};     // Standard time = UTC + timeZoneOffset
Timezone timeZone(DST_Rule, STD_Rule);

Ticker AP_ReconnectTimer;
Ticker STA_ReconnectTimer;
Ticker MQTT_ReconnectTimer;

WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;

IPAddress ipAddress;
WiFiClient wifiClient;
DNSServer dnsServer;
ESP8266WebServer HTTP(80);
ESP8266HTTPUpdateServer httpUpdater;
AsyncMqttClient mqttClient;
FtpServer FTP;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

GButton button(BUTTON_PIN, HIGH_PULL, NORM_OPEN);
GButton buttonBoot(BUTTON_BOOT_PIN, HIGH_PULL, NORM_OPEN);

enum ConnectionType {ACCESS_POINT, WIFI_CLIENT};
enum ConnectionState {DISCONNECTED, AP_CONNECTED, STA_CONNECTED};
enum Theme {DARK, LIGHT};

ConnectionType CONNECTION_TYPE = ACCESS_POINT;
ConnectionState CONNECTION_STATE = DISCONNECTED;
Theme THEME = DARK;

#include "Classes.h"

Led timerLed (TIMER_LED_PIN);
Led wifiLed(WIFI_LED_PIN);

Output output0(OUTPUT_PIN, OUTPUT_LED_PIN);
Output outputs[NAMBER_OF_OUTPUTS] = {output0};

TimeTicker ticker(0);

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

  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
   
  httpUpdater.setup(&HTTP, OTAPATH, OTAUSER, OTAPASSWORD);
  
  HTTP.on("/last_sta_ip", [](){
      return HTTP.send(200, "text/plain", readFromFlash("/sta_ip.txt"));
  });

  HTTP.on("/change_theme", [](){
      // http://192.168.4.1/change_theme
      
      if(THEME == LIGHT){
        THEME = DARK;
        writeToFlash("/theme.txt", (String)THEME);       
        return HTTP.send(200, "text/plain", "dark theme"); 
      }
      else if(THEME == DARK){
        THEME = LIGHT;
        writeToFlash("/theme.txt", (String)THEME);      
        return HTTP.send(200, "text/plain", "light theme"); 
      }
      return HTTP.send(501, "text/plain", "fail");
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

  HTTP.on("/get_mqtt_config", [](){
      return HTTP.send(200, "text/plain", mqttConfigToJson());
  });

  HTTP.on("/set_mqtt_config", [](){
      if (!HTTP.hasArg("json")) return HTTP.send(428, "text/plain", "[error]: No json");
      // http://192.168.4.1/set_mqtt_config?json={"server":"mqtt.by","port":"1883","loginMQTT":"maks_kliuba","passwordMQTT":"43gele29","userTopicPrefix":"/user/maks_kliuba/"}
      // http://192.168.4.1/set_mqtt_config?json={"server":"192.168.0.107","port":"1883","loginMQTT":"max_house","passwordMQTT":"aspiree1531g","userTopicPrefix":""}
          
      String json = HTTP.arg("json");   
      if(mqttConfigFromJson(json)){
        if(mqttClient.connected()) mqttClient.disconnect();             
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

  HTTP.on("/get_status", [](){
      if (!HTTP.hasArg("time")) return HTTP.send(428, "text/plain", "[error]: No time");
      
      if(!isNtpUpdated && (abs(millis() - updateTimeTicker) >= UPDATE_INTERVAL || !isTimeUpdated)){
        updateTimeTicker = millis();    
        char *end;
        setTime(timeZone.toLocal(strtoul((HTTP.arg("time")).c_str(), &end, 10)));
        if(DEBUG){
          Serial.println("[time now UTC]: " + (String)now());
          Serial.println("[formatted time now UTC ]: " + TimeManager::getTimeNow());
        }
        if(!isTimeUpdated) ticker.updateTime();
        isTimeUpdated = true;
      }         
      return HTTP.send(200, "text/plain", getJsonStatus());
  });
  
  HTTP.on("/on", [](){
      if (!HTTP.hasArg("port")) return HTTP.send(428, "text/plain", "[error]: No port");

      if(isStrDigit(HTTP.arg("port"))){
        int id = (HTTP.arg("port")).toInt();

        if(id >= 0 && id < NAMBER_OF_OUTPUTS) return HTTP.send(200, "text/plain", (String)outputOn(id));
        else return HTTP.send(501, "text/plain", "[error]: Invalid port");
      }
      return HTTP.send(501, "text/plain", "[error]: Invalid port");
  });

  HTTP.on("/off", [](){
      if (!HTTP.hasArg("port")) return HTTP.send(428, "text/plain", "[error]: No port");

      if(isStrDigit(HTTP.arg("port"))){
        int id = (HTTP.arg("port")).toInt();

        if(id >= 0 && id < NAMBER_OF_OUTPUTS) return HTTP.send(200, "text/plain", (String)outputOff(id));
        else return HTTP.send(501, "text/plain", "[error]: Invalid port");
      }
      return HTTP.send(501, "text/plain", "[error]: Invalid port");
  });

  HTTP.on("/switch", [](){
      if (!HTTP.hasArg("port")) return HTTP.send(428, "text/plain", "[error]: No port");

      if(isStrDigit(HTTP.arg("port"))){
        int id = (HTTP.arg("port")).toInt();

        if(id >= 0 && id < NAMBER_OF_OUTPUTS) return HTTP.send(200, "text/plain", (String)outputSwitch(id));
        else return HTTP.send(501, "text/plain", "[error]: Invalid port");
      }
      return HTTP.send(501, "text/plain", "[error]: Invalid port");
  });

  HTTP.on("/set_ticker", [](){
      if (!HTTP.hasArg("json")) return HTTP.send(428, "text/plain", "[error]: No json");
      // http://192.168.0.111/set_ticker?json={"state":1,"output":0,"type":1,"effect_id":3,"time":"00:00;00"}
      // http://192.168.4.1/set_ticker?json={"state":1,"output":0,"type":1,"effect_id":3,"time":"00:00:00"}
    
      String json = HTTP.arg("json");
      if(ticker.fromJson(json)){
        writeToFlash("/ticker.txt", json);      
        return HTTP.send(200, "text/plain", "success");
      }     
      return HTTP.send(501, "text/plain", "error");
  });

  HTTP.on("/set_ticker_state", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");
    
      byte _state = (byte)(HTTP.arg("value")).toInt();

      if(_state == ticker.OFF){
        ticker.off();
        return HTTP.send(200, "text/plain", (String)ticker.state);
      }
      else if(_state == ticker.ON){
        ticker.on();
        return HTTP.send(200, "text/plain", (String)ticker.state);
      }
      else if(_state == ticker.RESET){
        ticker.reset();
        return HTTP.send(200, "text/plain", (String)ticker.state);
      }
      else return HTTP.send(501, "text/plain", "error");
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

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  stationConnectedHandler = WiFi.onStationModeGotIP(&onStationConnected);
  stationDisconnectedHandler = WiFi.onStationModeDisconnected(&onStationDisconnected);
 
  AP_ReconnectTimer.attach(5, connectToWifi_AP);
  STA_ReconnectTimer.attach(5, connectToWifi_STA);
  MQTT_ReconnectTimer.attach(7, connectToMqtt);

  timeClient.setUpdateInterval(UPDATE_INTERVAL);

  ticker.init(&writeToFlash, &readFromFlash, &tickerRun);

  outputsStatesFromJson(readFromFlash("/outputs_states.txt"));

  String str = readFromFlash("/connection_type.txt");
  if(str.length() != 0) CONNECTION_TYPE = (ConnectionType)str.toInt();
  
  str = readFromFlash("/theme.txt");
  if(str.length() != 0) THEME = (Theme)str.toInt();

  wifiConfigFromJson(readFromFlash("/wifi_config.txt"));
  mqttConfigFromJson(readFromFlash("/mqtt_config.txt"));
  timeConfigFromJson(readFromFlash("/time_config.txt"));

  if(!ticker.fromJson(readFromFlash("/ticker.txt"))) ticker.reset();
}

void loop(){ 
  button.tick();
  buttonBoot.tick();
  ticker.tick();
  // mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());
  
  if(CONNECTION_TYPE == ACCESS_POINT) dnsServer.processNextRequest();
  HTTP.handleClient();
  FTP.handleFTP();

  if(button.isPress()) outputSwitch(0);

  if(buttonBoot.isHolded()){
    disconnect();
    if(CONNECTION_TYPE == ACCESS_POINT){
      CONNECTION_TYPE = WIFI_CLIENT;
    }
    else if(CONNECTION_TYPE == WIFI_CLIENT){
      CONNECTION_TYPE = ACCESS_POINT;
    }
    writeToFlash("/connection_type.txt", (String)CONNECTION_TYPE);
  }

  displayConnection();

  if(disconnectingFlag){
    delay(50);
    disconnectingFlag = false;
    disconnect();
  }

  if(CONNECTION_STATE == STA_CONNECTED){
    if(timeClient.update()){
      isNtpUpdated = true;
      isTimeUpdated = true;
      
      setTime(timeZone.toLocal(timeClient.getEpochTime()));
      if(DEBUG){
        Serial.println("[time now UTC]: " + (String)now());
        Serial.println("[formatted time now UTC ]: " + TimeManager::getTimeNow());
      }
  
      ticker.updateTime();
    }
  }
  else if(isTimeUpdated && abs(millis() - updateTimeTicker) >= UPDATE_INTERVAL){
    updateTimeTicker = millis();

    setTime(timeZone.toLocal(timeClient.getEpochTime()));
    if(DEBUG){
      Serial.println("[time now UTC]: " + (String)now());
      Serial.println("[formatted time now UTC ]: " + TimeManager::getTimeNow());
    }

    ticker.updateTime();
  }

  if(ticker.state == ticker.ON) timerLed.blink(100, 900);
  else timerLed.off();
}
