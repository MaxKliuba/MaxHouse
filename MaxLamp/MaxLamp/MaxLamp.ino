/*
 * ESP8266 Boards 2.5.2
 * LOLIN(WEMOS) D1 R2 & mini
 * 1M SPIFFS
 * 
 * by Max Kliuba
 */

#include <GyverEncoder.h>
#include <GyverButton.h>
#include <FastLED.h>
#include <SoftwareSerial.h>
#include <DFPlayer_Mini_Mp3.h>
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

#define S1_PIN D2
#define S2_PIN D1
#define KEY_PIN D7

#define LED_PIN D4
#define WIFI_LED_PIN D8

#define MP3_RX D6
#define MP3_TX D5
#define MP3_BUSY_PIN D0

#define ADC_PIN A0

#define WIDTH 16
#define HEIGHT 16
#define NUM_LEDS WIDTH * HEIGHT
#define BRIGHTNESS_STEP 1
#define MIN_BRIGHTNESS 1
#define MAX_BRIGHTNESS 60
#define CURRENT_LIMIT 2000
#define DEFAULT_VOLUME 6

#define TRACK_ACCESS_POINT 90
#define TRACK_WIFI_CLIENT 91
#define TRACK_VOLUME_CLICK 99

#define UPDATE_INTERVAL 60 * 60 * 1000

#define OTAUSER "admin"
#define OTAPASSWORD "admin"
#define OTAPATH "/firmware"

const String TYPE = "MaxLamp";
const String UUID = (String)WiFi.macAddress(); // BC:DD:C2:7A:D0:80
const char* ssidAP = "MaxLamp";                     
const char* passwordAP = "password";
const String prefix = TYPE + "/" + UUID;

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
String topicSetAlarm = userTopicPrefix + prefix + "/set_alarm";
String topicSetTicker = userTopicPrefix + prefix + "/set_ticker";

Encoder encoder(S1_PIN, S2_PIN, KEY_PIN, TYPE2);
GButton button(KEY_PIN, HIGH_PULL, NORM_OPEN);
CRGB leds[NUM_LEDS];
SoftwareSerial mp3_serial(MP3_RX, MP3_TX);

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

bool isOn = false;
byte brightness = 50;
byte currentEffect = 0;
byte previousEffect = currentEffect;
byte menuLevel = currentEffect, previousMenuLevel = menuLevel;
bool loadingFlag = true;
byte pixelVolume = DEFAULT_VOLUME;
bool isSound = true, isPlaying = false;
unsigned long updateTimer = 0;
unsigned long checkMp3StatusTimer = 0;
bool isBrightnessReady = false, isVolumeReady = false, effConfigReady = false;
unsigned long brightnessCheckTimer = 0, volumeCheckTimer = 0, effConfigCheckTimer = 0;
bool disconnectingFlag = false;
bool isTimeUpdated = false, isNtpUpdated = false;
unsigned long updateTimeTicker = 0;
int timeZoneOffset = 2 * 60;
byte timeZoneID = 40;
byte isDST = 0;

TimeChangeRule DST_Rule = {"DST", Last, Sun, Mar, 3, 0};    // Daylight time = UTC + timeZoneOffset + 1 hour
TimeChangeRule STD_Rule = {"STD", Last, Sun, Oct, 4, 0};     // Standard time = UTC + timeZoneOffset
Timezone timeZone(DST_Rule, STD_Rule);

enum Level {EFFECTS, MENU, ALARM};
enum ConnectionType {ACCESS_POINT, WIFI_CLIENT};
enum ConnectionState {DISCONNECTED, AP_CONNECTED, STA_CONNECTED};
enum Theme {DARK, LIGHT};

Level LEVEL = EFFECTS;
ConnectionType CONNECTION_TYPE = ACCESS_POINT;
ConnectionState CONNECTION_STATE = DISCONNECTED;
Theme THEME = DARK;

#include "Classes.h"

#define NUMBER_OF_EFFECTS 18
Effect effects[NUMBER_OF_EFFECTS];

Led wifiLed(WIFI_LED_PIN);

Alarm alarm(0);
TimeTicker ticker(0);

void setup() { 
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

  HTTP.on("/get_level", [](){
      return HTTP.send(200, "text/plain", (String)LEVEL);
  });

  HTTP.on("/set_level", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");

      if(isStrDigit(HTTP.arg("value"))){
        Level value = (Level)((HTTP.arg("value")).toInt());
        if(value == EFFECTS || value == MENU){
          if(LEVEL == ALARM) alarm.stop();
          setLevel(value);
          return HTTP.send(200, "text/plain", "[level]: " + (String)LEVEL);
        }
      }
      return HTTP.send(501, "text/plain", "[error]: Invalid value");
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
  
  HTTP.on("/set_state", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");

      if(isStrDigit(HTTP.arg("value"))){
        if(LEVEL == ALARM) alarm.stop();
        bool value = (bool)((HTTP.arg("value")).toInt());
        if(LEVEL != EFFECTS) setLevel(EFFECTS);   
        if(value) on();
        else off();     
        return HTTP.send(200, "text/plain", (String)isOn);
      }
      return HTTP.send(501, "text/plain", "[error]: Invalid value");
  });

  HTTP.on("/set_effect", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");

      if(isStrDigit(HTTP.arg("value"))){
        byte value = (byte)((HTTP.arg("value")).toInt());
        if(value >= 0 && value < NUMBER_OF_EFFECTS){
          if(LEVEL == ALARM) alarm.stop();
          if(LEVEL != EFFECTS) setLevel(EFFECTS);  
          currentEffect = value;
          checkEffectsMode();
          on();
          if(isSound){
            mp3Play(currentEffect);
            delay(50);
          }         
          return HTTP.send(200, "text/plain", "[effect]: " + (String)currentEffect);
        }
      }
      return HTTP.send(501, "text/plain", "[error]: Invalid value");
  });

  HTTP.on("/set_default_effect", [](){
      if(LEVEL == ALARM) alarm.stop();
      if(LEVEL != EFFECTS) setLevel(EFFECTS);  
      currentEffect = 0;
      effects[currentEffect].setDefaults();
      checkEffectsMode();
      on(); 
      if(isSound){
        mp3Play(currentEffect);
        delay(50);
      }       
      return HTTP.send(200, "text/plain", "[effect]: " + (String)currentEffect);
  });

  HTTP.on("/set_brightness", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");

      if(isStrDigit(HTTP.arg("value"))){
        int value = (HTTP.arg("value")).toInt();
        if(value >= MIN_BRIGHTNESS && value <= MAX_BRIGHTNESS){ 
          if(LEVEL == EFFECTS) setBrightness(value);       
          return HTTP.send(200, "text/plain", "[brightness]: " + (String)brightness);
        }
      }
      return HTTP.send(501, "text/plain", "[error]: Invalid value");
  });

  HTTP.on("/set_sound_state", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");

      if(isStrDigit(HTTP.arg("value"))){
        setSound((bool)((HTTP.arg("value")).toInt()));     
        return HTTP.send(200, "text/plain", "[sound state]: " + (String)isSound);
      }     
      return HTTP.send(501, "text/plain", "[error]: Invalid value");
  });
  
  HTTP.on("/set_volume", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");

      if(isStrDigit(HTTP.arg("value"))){
        int value = (HTTP.arg("value")).toInt();
        if(value >= 1 && value <= 15){
          setVolume(value);
          
          return HTTP.send(200, "text/plain", "[volume]: " + (String)pixelVolume);
        }
      }     
      return HTTP.send(501, "text/plain", "[error]: Invalid value");
  });

  HTTP.on("/set_default_volume", [](){
      setVolume(DEFAULT_VOLUME);        
      return HTTP.send(200, "text/plain", "[volume]: " + (String)pixelVolume);
  });

  HTTP.on("/set_scale", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");

      if(isStrDigit(HTTP.arg("value"))){
        int value = (HTTP.arg("value")).toInt();
        if(value >= effects[currentEffect].getMinScale() && value <= effects[currentEffect].getMaxScale()){
          value = (byte)((HTTP.arg("value")).toInt());
          if(LEVEL == EFFECTS || LEVEL == ALARM) setScale(value);       
          return HTTP.send(200, "text/plain", "[scale]: " + (String)effects[currentEffect].getScale());
        }
      }     
      return HTTP.send(501, "text/plain", "[error]: Invalid value");
  });

  HTTP.on("/set_default_scale", [](){
      if(LEVEL == EFFECTS || LEVEL == ALARM) setScale(effects[currentEffect].getDefaultScale());        
      return HTTP.send(200, "text/plain", "[scale]: " + (String)effects[currentEffect].getScale());
  });

  HTTP.on("/set_speed", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");

      if(isStrDigit(HTTP.arg("value"))){
        int value = (HTTP.arg("value")).toInt();
        if(value >= effects[currentEffect].getMinSpeed() && value <= effects[currentEffect].getMaxSpeed()){
          value = (byte)((HTTP.arg("value")).toInt());
          if(LEVEL == EFFECTS || LEVEL == ALARM) setSpeed(value);        
          return HTTP.send(200, "text/plain", "[speed]: " + (String)effects[currentEffect].getSpeed());
        }
      }     
      return HTTP.send(501, "text/plain", "[error]: Invalid value");
  });

  HTTP.on("/set_default_speed", [](){
      if(LEVEL == EFFECTS || LEVEL == ALARM) setSpeed(effects[currentEffect].getDefaultSpeed());          
      return HTTP.send(200, "text/plain", "[speed]: " + (String)effects[currentEffect].getSpeed());
  });

  HTTP.on("/set_alarm", [](){
      if (!HTTP.hasArg("json")) return HTTP.send(428, "text/plain", "[error]: No json");
      // http://192.168.0.111/set_alarm?json={"state":1,"replay":0,"effect_id":3,"time":"00:00","outset":1,"duration":1}
      // http://192.168.4.1/set_alarm?json={"state":1,"replay":0,"effect_id":3,"time":"00:00","outset":1,"duration":1}

      if(LEVEL == ALARM) alarm.stop();     
      String json = HTTP.arg("json");
      if(alarm.fromJson(json)){
        writeToFlash("/alarm.txt", json);      
        return HTTP.send(200, "text/plain", "success");
      }     
      return HTTP.send(501, "text/plain", "error");
  });

  HTTP.on("/set_alarm_state", [](){
      if (!HTTP.hasArg("value")) return HTTP.send(428, "text/plain", "[error]: No value");

      if(LEVEL == ALARM) alarm.stop();    
      byte _state = (byte)(HTTP.arg("value")).toInt();

      if(_state == alarm.OFF){
        alarm.off();
        return HTTP.send(200, "text/plain", (String)alarm.state);
      }
      else if(_state == alarm.ON){
        alarm.on();
        return HTTP.send(200, "text/plain", (String)alarm.state);
      }
      else if(_state == alarm.RESET){
        alarm.reset();
        return HTTP.send(200, "text/plain", (String)alarm.state);
      }
      else if(_state == alarm.STOP){
        alarm.stop();
        return HTTP.send(200, "text/plain", (String)alarm.state);
      }
      else return HTTP.send(501, "text/plain", "error");
  });

  HTTP.on("/set_ticker", [](){
      if (!HTTP.hasArg("json")) return HTTP.send(428, "text/plain", "[error]: No json");
      // http://192.168.0.111/set_ticker?json={"state":1,"type":1,"effect_id":3,"time":"00:00;00"}
      // http://192.168.4.1/set_ticker?json={"state":1,"type":1,"effect_id":3,"time":"00:00:00"}
    
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
  
  /*Effect(void (*callback)(void) = NULL, byte DEFAULT_SCALE_ = 20, byte DEFAULT_SPEED_ = 20, 
           byte scaleStep_ = 2, byte speedStep_ = 2, byte MAX_SCALE_ = 255, byte MAX_SPEED_ = 60, byte MIN_SCALE_ = 0, byte MIN_SPEED_ = 2)*/
  effects[0] = Effect(coloredLightEffect, 128, 20, 2, 0, 255, 20, 0, 20);
  effects[1] = Effect(fireEffect, 0, 4, 2, 2, 255, 30, 0, 1);
  effects[2] = Effect(lava3DEffect, 60, 2, 5, 1, 255, 30, 10, 1);
  effects[3] = Effect(ocean3DEffect, 60, 2, 5, 1, 255, 30, 10, 1);
  effects[4] = Effect(sky3DEffect, 60, 2, 5, 1, 255, 10, 10, 1);
  effects[5] = Effect(forest3DEffect, 60, 4, 5, 1, 255, 30, 10, 1);
  effects[6] = Effect(firefliesEffect, 15, 40, 1, 2, 40, 120, 1, 14);
  effects[7] = Effect(sparklesEffect, 5, 54, 1, 2, 100, 120, 1, 2);
  effects[8] = Effect(rainbowVerticalEffect, 12, 20);
  effects[9] = Effect(rainbowHorizontalEffect, 16, 20);
  effects[10] = Effect(rainbow3DEffect, 60, 4, 5, 1, 255, 30, 10, 1);
  effects[11] = Effect(rainbowStripe3DEffect, 60, 2, 5, 1, 255, 30, 10, 1);
  effects[12] = Effect(plasma3DEffect, 60, 4, 5, 1, 255, 30, 10, 1);
  effects[13] = Effect(madness3DEffect, 60, 20, 5, 1, 255, 30, 10, 1);
  effects[14] = Effect(rainEffect, 50, 40, 5, 5, 80, 100, 5, 2);
  effects[15] = Effect(snowEffect, 50, 80, 5, 5, 80, 100, 5, 2);
  effects[16] = Effect(matrixEffect, 50, 30, 5, 5, 80, 100, 5, 2);  
  effects[17] = Effect(audioAnalyzerEffect, 150, 10, 5, 0, 255, 10, 0, 10);  
  
  encoder.setDirection(REVERSE);
  button.setClickTimeout(300);
  button.setTimeout(ENC_HOLD_TIMEOUT);
  //button.setDebounce(50);

  timeClient.setUpdateInterval(UPDATE_INTERVAL);

  alarm.init(&writeToFlash, &readFromFlash);
  ticker.init(&writeToFlash, &readFromFlash, &tickerRun);

  attachInterruptFun();

  mp3_serial.begin(9600);
  delay(500);
  mp3_set_serial(mp3_serial);
  delay(50);
  mp3_set_volume(pixelVolume * 2);
  delay(50);
  mp3Stop();
  pinMode(MP3_BUSY_PIN, INPUT);

  String str = readFromFlash("/state.txt");
  if(str.length() != 0) isOn = (bool)str.toInt();
  
  str = readFromFlash("/current_effect.txt");
  if(str.length() != 0){
    currentEffect = str.toInt();
    previousEffect = currentEffect;
  }
  
  str = readFromFlash("/brightness.txt");
  if(str.length() != 0) brightness = str.toInt();
  
  for(byte id = 0; id < NUMBER_OF_EFFECTS; id++){
    effectsConfigFromJson(id, readFromFlash("/effect_" + (String)id + ".txt"));
  }
  
  str = readFromFlash("/sound_state.txt");
  if(str.length() != 0) isSound = (bool)str.toInt();
  
  str = readFromFlash("/sound_volume.txt");
  if(str.length() != 0){
    pixelVolume = str.toInt();
    mp3_set_volume(pixelVolume * 2);
  }

  str = readFromFlash("/connection_type.txt");
  if(str.length() != 0) CONNECTION_TYPE = (ConnectionType)str.toInt();

  str = readFromFlash("/theme.txt");
  if(str.length() != 0) THEME = (Theme)str.toInt();
  
  wifiConfigFromJson(readFromFlash("/wifi_config.txt"));
  mqttConfigFromJson(readFromFlash("/mqtt_config.txt"));
  timeConfigFromJson(readFromFlash("/time_config.txt"));
  
  alarm.fromJson(readFromFlash("/alarm.txt"));
  ticker.fromJson(readFromFlash("/ticker.txt"));
  
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS)/*.setCorrection( TypicalLEDStrip )*/;
  FastLED.setBrightness(brightness);
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  FastLED.clear();
}

void loop() {
  button.tick();
  encoder.tick();
  alarm.tick();
  ticker.tick();

  if(CONNECTION_TYPE == ACCESS_POINT) dnsServer.processNextRequest();
  HTTP.handleClient();
  FTP.handleFTP();
  
  if(encoder.isHolded()){
    if(LEVEL == ALARM){
      if(DEBUG) Serial.println("[stop in encoder.isHolded()]");
      alarm.stop();
    }
    
    if(LEVEL == EFFECTS) setLevel(MENU);
    else if(LEVEL == MENU) setLevel(EFFECTS);
  }

  switch(LEVEL){
    case EFFECTS: effectsMode(); break;
    case MENU: menuMode(); break;
    case ALARM: alarmMode(); break;
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

  if(abs(millis() - checkMp3StatusTimer) >= 150){
    checkMp3StatusTimer = millis();
    isPlaying = !digitalRead(MP3_BUSY_PIN);
  }

  if(isBrightnessReady && abs(millis() - brightnessCheckTimer) >= 150){
    isBrightnessReady = false;
    writeBrightness();
  }

  if(isVolumeReady && abs(millis() - volumeCheckTimer) >= 150){
    isVolumeReady = false;
    writeVolume();
  }

  if(effConfigReady && abs(millis() - effConfigCheckTimer) >= 150){
    effConfigReady = false;
    writeEffectConfig();
  }
}

void effectsMode(){
  checkEffectsMode();
  updateMatrix(effects[currentEffect].getSpeed());
  
  if(button.hasClicks() && encoder.isClick()){
    switch(button.getClicks()){
      case 1: 
        if(isOn) off();
        else on();
        break;
      case 2: 
        if(currentEffect + 1 < NUMBER_OF_EFFECTS) currentEffect++;
        else currentEffect = 0;
        checkEffectsMode();
        if(isSound){
          mp3Play(currentEffect);
          delay(50);
        }
        break;
      case 3:
        if(currentEffect - 1 >= 0) currentEffect--;
        else currentEffect = NUMBER_OF_EFFECTS - 1;
        checkEffectsMode();
        if(isSound){
          mp3Play(currentEffect);
          delay(50);
        }
        break;
      case 4:
        setSound(!isSound);
        break;
      case 5: 
        effects[currentEffect].setDefaults();
        writeEffectConfig();        
        break;
      case 6:
        currentEffect = 0;
        effects[currentEffect].setDefaults();
        checkEffectsMode();
        if(isSound){
          mp3Play(currentEffect);
          delay(50);
        } 
        break;
    }
  }
  
  if(encoder.isRight()){
    if(brightness + BRIGHTNESS_STEP <= MAX_BRIGHTNESS){
      setBrightness(brightness + BRIGHTNESS_STEP);
      if(DEBUG) Serial.println("[right]: " + (String)brightness);
    }
  }
  if(encoder.isLeft()){
    if(brightness - BRIGHTNESS_STEP >= MIN_BRIGHTNESS){
      setBrightness(brightness - BRIGHTNESS_STEP);
      if(DEBUG) Serial.println("[left]: " + (String)brightness);
    }
  }

  if(encoder.isRightH()){
    effects[currentEffect].scaleIncrement();
    setScale(effects[currentEffect].getScale());
    if(DEBUG) Serial.println(effects[currentEffect].getScale());
    /*effects[currentEffect].speedIncrement();
      Serial.println(effects[currentEffect].getSpeed());*/
  }
  if(encoder.isLeftH()){
    effects[currentEffect].scaleDecrement();
    setScale(effects[currentEffect].getScale());
    if(DEBUG) Serial.println(effects[currentEffect].getScale());
    /*effects[currentEffect].speedDecrement();
      Serial.println(effects[currentEffect].getSpeed());*/
  }
}

void menuMode(){
  for(byte x = 0; x < WIDTH; x++){
    if(x <= pixelVolume){
      if(pixelVolume == 1) drawPixelXY(14, HEIGHT - 1, CRGB::Black);
      else if(isSound) drawPixelXY(WIDTH - 1 - x, HEIGHT - 1, CHSV(100, 255, 255));
      else drawPixelXY(WIDTH - 1 - x, HEIGHT - 1, CHSV(0, 255, 255));
    }
    else drawPixelXY(WIDTH - 1 - x, HEIGHT - 1, CRGB::Black);
  }

  byte startPosition = 0;
  byte finalPosition = HEIGHT - 4;
  
  if(menuLevel > HEIGHT - 4){
      startPosition = menuLevel - (HEIGHT - 4);
      finalPosition = menuLevel;
  }
  
  for(byte y = startPosition, ledPos = HEIGHT - 4; y <= finalPosition; y++, ledPos--){
    for(byte x = 0; x < WIDTH; x++){
      if(y == menuLevel) drawPixelXY(x, ledPos, CHSV(147, 255, 255));
      else{
        if(y % 2 == 0) drawPixelXY(x, ledPos, CHSV(0, 0, 60));
        else drawPixelXY(x, ledPos, CRGB::Black);
      }
    }
  }

  if(button.hasClicks() && encoder.isClick()){
    switch(button.getClicks()){
      case 1:
        setLevel(EFFECTS);
        currentEffect = menuLevel;
        on();
        break;
      case 2: 
        setSound(!isSound);
        break;
      case 3:
        setVolume(DEFAULT_VOLUME);
        break;
      case 4:
        disconnect();
        if(CONNECTION_TYPE == ACCESS_POINT){
          CONNECTION_TYPE = WIFI_CLIENT;
          mp3Play(TRACK_WIFI_CLIENT);
          delay(50);
        }
        else if(CONNECTION_TYPE == WIFI_CLIENT){
          CONNECTION_TYPE = ACCESS_POINT;
          if(isSound) mp3Play(TRACK_ACCESS_POINT);
          delay(50);
        }
        writeToFlash("/connection_type.txt", (String)CONNECTION_TYPE);
        break;
      case 10:
        off();
        setSound(false);
        setVolume(DEFAULT_VOLUME);
        alarm.reset();
        for(byte id = 0; id < NUMBER_OF_EFFECTS; id++){
          effects[id].setDefaults();
          writeToFlash("/effect_" + (String)id + ".txt", effectsConfigToJson(id));
        }
        CONNECTION_TYPE = ACCESS_POINT;
        writeToFlash("/connection_type.txt", (String)CONNECTION_TYPE);
        setLevel(EFFECTS);
        currentEffect = 0;
        setBrightness(MAX_BRIGHTNESS / 2);  
        break;
    }
  }

  if(encoder.isRight()){
    previousMenuLevel = menuLevel;
    if(menuLevel - 1 >= 0) menuLevel--;
    else menuLevel = NUMBER_OF_EFFECTS - 1;
    if(isSound){
      mp3Play(menuLevel);
      delay(20);
    }
  }
  if(encoder.isLeft()){
    previousMenuLevel = menuLevel;
    if(menuLevel + 1 <= NUMBER_OF_EFFECTS - 1) menuLevel++;
    else menuLevel = 0;
    if(isSound){
      mp3Play(menuLevel);
      delay(20);
    }
  }
  
  if(encoder.isRightH()){
    if(pixelVolume + 1 <= 15){
      pixelVolume++;
      setVolume(pixelVolume);
      if(isSound){
        mp3Play(TRACK_VOLUME_CLICK);
        delay(10);
      }
    }
  }
  if(encoder.isLeftH()){
    if(pixelVolume - 1 >= 1){
      pixelVolume--;
      setVolume(pixelVolume);
      if(isSound){
        mp3Play(TRACK_VOLUME_CLICK);
        delay(10);
      }
    }
  }

  updateMatrix(20);
}

void alarmMode(){
  if(alarm.run()) updateMatrix(effects[currentEffect].getSpeed());
  if((button.hasClicks() && encoder.isClick()) || encoder.isRight() || encoder.isLeft() || encoder.isRightH() || encoder.isLeftH()){
    if(DEBUG) Serial.println("[stop in alarmMode()]");
    alarm.stop();
  }
}

ICACHE_RAM_ATTR void encoderTick(){
  encoder.tick();
  button.tick();
}
