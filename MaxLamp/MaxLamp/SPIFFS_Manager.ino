String getJsonStatus(){
  size_t capacity = JSON_OBJECT_SIZE(23) + 3 * 23;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["state"] = (byte)isOn;
  doc["current_effect"] = currentEffect;
  doc["brightness"] = brightness;
  doc["sound_state"] = (byte)isSound;
  doc["volume"] = pixelVolume;
  doc["scale"] = effects[currentEffect].getScale();
  doc["min_scale"] = effects[currentEffect].getMinScale();
  doc["max_scale"] = effects[currentEffect].getMaxScale();
  doc["speed"] = effects[currentEffect].getSpeed();
  doc["min_speed"] = effects[currentEffect].getMinSpeed();
  doc["max_speed"] = effects[currentEffect].getMaxSpeed();

  doc["alarm_state"] = (byte)alarm.state;
  doc["alarm_replay"] = (byte)alarm.isReplay;
  doc["alarm_time"] = alarm.time;
  if(alarm.state != alarm.RESET){
    doc["alarm_effect_id"] = alarm.effectID;
    if(alarm.state != alarm.OFF) doc["alarm_ticker"] = TimeManager::formatSeconds(alarm.secondsTicker);
    else doc["alarm_ticker"] = doc["alarm_ticker"] = "--:--:--";
    doc["alarm_outset"] = alarm.outset;
    doc["alarm_duration"] = alarm.duration;
  }
  else{
    doc["alarm_effect_id"] = "-----";
    doc["alarm_ticker"] = "--:--:--";
    doc["alarm_outset"] = "--";
    doc["alarm_duration"] = "--";
  }

  doc["ticker_state"] = (byte)ticker.state;
  if(ticker.state != ticker.RESET){
    doc["ticker_type"] = ticker.type;
    doc["ticker_mode"] = ticker.mode;
    if(ticker.state != ticker.OFF){
      doc["ticker_time"] = ticker.time;
      doc["ticker_ticker"] = TimeManager::formatSeconds(ticker.secondsTicker);
    }
    else{
      if(ticker.type == ticker.ALARM){
        doc["ticker_time"] = ticker.time;
        doc["ticker_ticker"] =  "--:--:--";
      }
      else if(ticker.type == ticker.TIMER){
        doc["ticker_time"] = "--:--:--";
        doc["ticker_ticker"] = TimeManager::formatSeconds(ticker.secondsTicker);
      }
    }
  }
  else{
    doc["ticker_time"] = "--:--:--";
    doc["ticker_ticker"] = "--:--:--";
    doc["ticker_type"] = "-----";
    doc["ticker_mode"] = "-----";
  }
  
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[json]: " + json);
           
  return json;
}

bool setJsonStatus(String json){
  if(json.length() == 0) return false;
  if(DEBUG) Serial.println("[json]: " + json);
  
  size_t capacity = JSON_OBJECT_SIZE(7) + json.length();
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);
  
  if(!error){ 
    if(isOn != doc["state"]){
      if(LEVEL == ALARM) alarm.stop();
      if(LEVEL != EFFECTS) setLevel(EFFECTS);  
      doc["state"] == 1 ? on() : off();
    }
    if(currentEffect != doc["current_effect"]){
      if(LEVEL == ALARM) alarm.stop();
      if(LEVEL != EFFECTS) setLevel(EFFECTS);  
      currentEffect = doc["current_effect"];
      checkEffectsMode();
      on();
      if(isSound){
        mp3Play(currentEffect);
        delay(50);
      }    
    }
    if(brightness != doc["brightness"]){
      if(LEVEL == EFFECTS) setBrightness(doc["brightness"]);
    }
    if(isSound != doc["sound_state"]) setSound(doc["sound_state"]);
    if(pixelVolume != doc["volume"]) setVolume(doc["sound_state"]);
    if(effects[currentEffect].getScale() != doc["scale"]){
      if(LEVEL == EFFECTS || LEVEL == ALARM) setScale(doc["scale"]);
    }
    if(effects[currentEffect].getSpeed() != doc["speed"]){
      if(LEVEL == EFFECTS || LEVEL == ALARM) setSpeed(doc["speed"]);
    }
    
    return true;   
  }

  if(mqttClient.connected()) mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());
   
  return false;
}

String getJsonScale(){
  size_t capacity = JSON_OBJECT_SIZE(3) + 9;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["scale"] = effects[currentEffect].getScale();
  doc["min_scale"] = effects[currentEffect].getMinScale();
  doc["max_scale"] = effects[currentEffect].getMaxScale();
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[json]: " + json);
        
  return json;
}

String getJsonSpeed(){
  size_t capacity = JSON_OBJECT_SIZE(3) + 9;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["speed"] = effects[currentEffect].getSpeed();
  doc["min_speed"] = effects[currentEffect].getMinSpeed();
  doc["max_speed"] = effects[currentEffect].getMaxSpeed();
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[json]: " + json);
        
  return json;
}

String effectsConfigToJson(byte id){
  size_t capacity = JSON_OBJECT_SIZE(2) + 6;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["scale"] = effects[id].getScale();
  doc["speed"] = effects[id].getSpeed();
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[json]: " + json);
        
  return json;
}

bool effectsConfigFromJson(byte id, String json){
  if(json.length() == 0) return false;
  if(DEBUG) Serial.println("[json]: " + json);
  
  size_t capacity = JSON_OBJECT_SIZE(2) + json.length();
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);
  
  if(!error){
    effects[id].setScale(doc["scale"]);
    effects[id].setSpeed(doc["speed"]);   
    
    return true;   
  }
   
  return false;
}

String wifiConfigToJson(){
  size_t capacity = JSON_OBJECT_SIZE(4) + ssidSTA.length() + passwordSTA.length() + 30 + 4 * 3;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["ssid"] = ssidSTA;
  doc["password"] = passwordSTA;
  if(CONNECTION_TYPE == ACCESS_POINT) doc["current_ip"] = WiFi.softAPIP().toString();
  else doc["current_ip"] = WiFi.localIP().toString();
  doc["last_sta_ip"] = readFromFlash("/sta_ip.txt");
  
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[sent json]: " + json);
           
  return json;
}

bool wifiConfigFromJson(String json){
  if(json.length() == 0) return false;
  
  size_t capacity = JSON_OBJECT_SIZE(4) + json.length();
  if(DEBUG){
    Serial.println("[received json]: " + json);
    Serial.println("[capacity]: " + (String)capacity);
  }
  
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);
  
  if(!error){
    ssidSTA = doc["ssid"].as<String>();
    passwordSTA = doc["password"].as<String>();
    
    return writeToFlash("/wifi_config.txt", json);   
  }
  if(DEBUG){
    Serial.print("[json error]: ");
    Serial.println(error.c_str());
  }
  
  return false;
}

String mqttConfigToJson(){
  size_t capacity = JSON_OBJECT_SIZE(9)
                    + serverMQTT.length() + ((String)portMQTT).length() + loginMQTT.length() + passwordMQTT.length() 
                    + userTopicPrefix.length() + topicDeviceState.length() + topicControllerState.length() + topicSetAlarm.length() + topicSetTicker.length() + 9 * 3;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["server"] = serverMQTT;
  doc["port"] = (String)portMQTT;
  doc["login_mqtt"] = loginMQTT;
  doc["password_mqtt"] = passwordMQTT;
  doc["user_topic_prefix"] = userTopicPrefix;
  doc["device_state_topic"] = topicDeviceState;
  doc["controller_state_topic"] = topicControllerState;
  doc["set_alarm_topic"] = topicSetAlarm;
  doc["set_ticker_topic"] = topicSetTicker;
  
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[sent json]: " + json);
           
  return json;
}

bool mqttConfigFromJson(String json){
  if(json.length() == 0) return false;
  
  size_t capacity = JSON_OBJECT_SIZE(9) + json.length();
  if(DEBUG){
    Serial.println("[received json]: " + json);
    Serial.println("[capacity]: " + (String)capacity);
  }
  
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);
  
  if(!error)
  {
    serverMQTT = doc["server"].as<String>();
    portMQTT = (doc["port"].as<String>()).toInt();
    loginMQTT = doc["login_mqtt"].as<String>();
    passwordMQTT = doc["password_mqtt"].as<String>();
    userTopicPrefix = doc["user_topic_prefix"].as<String>();

    topicDeviceState = userTopicPrefix + prefix + "/device_state";
    topicControllerState = userTopicPrefix + prefix + "/controller_state";
    topicSetAlarm = userTopicPrefix + prefix + "/set_alarm";
    topicSetTicker = userTopicPrefix + prefix + "/set_ticker";
    
    return writeToFlash("/mqtt_config.txt", json);   
  }
  if(DEBUG){
    Serial.print("[json error]: ");
    Serial.println(error.c_str());
  }
  
  return false;
}

String timeConfigToJson(){
  size_t capacity = JSON_OBJECT_SIZE(11) + 20 + 11 * 3;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["time_zone_offset"] = timeZoneOffset;
  doc["time_zone_id"] = timeZoneID;
  doc["is_dst"] = isDST;
  
  doc["dst_week"] = DST_Rule.week;
  doc["dst_dow"] = DST_Rule.dow;
  doc["dst_month"] = DST_Rule.month;
  doc["dst_hour"] = DST_Rule.hour;

  doc["std_week"] = STD_Rule.week;
  doc["std_dow"] = STD_Rule.dow;
  doc["std_month"] = STD_Rule.month;
  doc["std_hour"] = STD_Rule.hour;
  
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[sent json]: " + json);
           
  return json;
}

bool timeConfigFromJson(String json){
  if(json.length() == 0) return false;
  
  size_t capacity = JSON_OBJECT_SIZE(11) + json.length();
  if(DEBUG){
    Serial.println("[received json]: " + json);
    Serial.println("[capacity]: " + (String)capacity);
  }
  
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);
  
  if(!error){
    timeZoneOffset = doc["time_zone_offset"];
    timeZoneID = doc["time_zone_id"];
    isDST = doc["is_dst"];

    DST_Rule.week = doc["dst_week"];
    DST_Rule.dow = doc["dst_dow"];
    DST_Rule.month = doc["dst_month"];
    DST_Rule.hour = doc["dst_hour"];
    DST_Rule.offset = timeZoneOffset + 60;

    STD_Rule.week = doc["std_week"];
    STD_Rule.dow = doc["std_dow"];
    STD_Rule.month = doc["std_month"];
    STD_Rule.hour = doc["std_hour"];
    STD_Rule.offset = timeZoneOffset;

    if(!isDST) timeZone = Timezone(STD_Rule);
    else timeZone = Timezone(DST_Rule, STD_Rule);

    
    isTimeUpdated = false;
    if(CONNECTION_STATE == STA_CONNECTED){
      timeClient.forceUpdate();
      isNtpUpdated = true;
      isTimeUpdated = true;
    }

    setTime(timeZone.toLocal(timeClient.getEpochTime()));
    if(DEBUG){
      Serial.println("[time now UTC]: " + (String)now());
      Serial.println("[formatted time now UTC ]: " + TimeManager::getTimeNow());
    }

    if(isTimeUpdated) ticker.updateTime();
      
    return writeToFlash("/time_config.txt", json); 
  }
  if(DEBUG){
    Serial.print("[json error]: ");
    Serial.println(error.c_str());
  }
   
  return false;
}

String getFileSystemList(){
  String str = "";
  int fileSize = 0;
  detachInterruptFun();
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
      fileSize += dir.fileSize();
      str += dir.fileName();
      str += " [";
      str += dir.fileSize();
      str += " Byte]\r\n";
  }
  attachInterruptFun();
  str += "\r\n[Memory]: " + (String)fileSize + " Byte"; 

  return str;
}

bool handleFileRead(String path){ 
  if(path == "/") path = "/index.html";

  if(path == "/redirect" && CONNECTION_TYPE == ACCESS_POINT) path = "/settings.html";

  if(path.endsWith("support_icon.png") || path.endsWith("button_back.png") || path.endsWith("button_support.png") 
    || path.endsWith("style.css") || path.endsWith("sad_eyes.png") || path.endsWith("esp.ico")){
    path = path.substring(path.lastIndexOf("/"));
  }
    
  if(path.endsWith("/theme.css")){
    if(THEME == LIGHT) path = "/light_theme.css";
    else if(THEME == DARK) path = "/dark_theme.css";
  }  
  
  String contentType = getContentType(path);
  //Serial.println("[http]: " + path);
  if(SPIFFS.exists(path)){
    detachInterruptFun();
    File file = SPIFFS.open(path, "r");
    size_t sent = HTTP.streamFile(file, contentType);
    file.close();
    attachInterruptFun();
    
    return true;
  }
  
  return false;
}

String getContentType(String filename){
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

bool writeToFlash(String path, String text){
  bool flag = false;
  detachInterruptFun();
  File file = SPIFFS.open(path, "w");
  if (file){
    if(DEBUG) Serial.println("[(w) " + path + "]: " + text);
    int bytesWritten = file.println(text);
    file.close();
    if(bytesWritten > 0) flag = true;
  }
  attachInterruptFun();
  
  return flag;
}

String readFromFlash(String path){
  String str = "";
  detachInterruptFun();
  if(SPIFFS.exists(path)){
    File file = SPIFFS.open(path, "r");
    while(file.available()){
     char ch = file.read();
     str += ch;
    }
    file.close();
  }
  attachInterruptFun();
  str.trim();
  if(DEBUG) Serial.println("[(r) " + path + "]: " + str);
  
  return str;
}
