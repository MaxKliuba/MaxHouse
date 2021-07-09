String getJsonStatus(){
  size_t capacity = JSON_OBJECT_SIZE(7) + 2 * JSON_ARRAY_SIZE(DRAPH_DATA_AMOUNT) + DRAPH_DATA_AMOUNT * 40 + 33 + (DRAPH_DATA_AMOUNT + 6) * 3;
  String json; 
  
  DynamicJsonDocument doc(capacity);

  checkDepth("web");
  checkSoilMoisture("web");
  
  doc["liquid_tank_percent"] = depthPercent;
  doc["soil_moisture_percent"] = soilMoisturePercent;
  doc["liquid_tank_min_percent"] = liquidTank.getMinDepthPercent();
  doc["soil_moisture_min_percent"] = soilMoistureSensor.getMinSoilMoisturePercent();
  doc["is_watering"] = (byte)wateringManager.isWatering();
  doc["water_timer_state"] = (byte)waterTimerState;
  if(waterTimerState){
    doc["water_timer_alarm"] = TimeManager::getDateTimeNow(now() + (time_t)waterTimerSeconds);
    doc["water_timer_seconds"] = TimeManager::getTimeNow((time_t)waterTimerSeconds);
  }
  else {
    doc["water_timer_alarm"] = "----.--.-- --:--:--";
    doc["water_timer_seconds"] = "--:--:--";
  }

  for(int i = 0; i < DRAPH_DATA_AMOUNT; i++){
      doc["graph_x"][i] = graphDataX[i];
      doc["graph_y"][i] = graphDataY[i];
  }
  
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[json]: " + json);
           
  return json;
}

String graphCoordinateToJson(byte i){
  size_t capacity = JSON_OBJECT_SIZE(2) + 40 + 2 * 3;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["x"] = graphDataX[i];
  doc["y"] = graphDataY[i];
  
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[sent json]: " + json);
           
  return json;
}

bool graphCoordinateFromJson(String json, byte i){
  if(json.length() == 0) return false;
  
  size_t capacity = JSON_OBJECT_SIZE(2) + json.length();
  if(DEBUG){
    Serial.println("[received json]: " + json);
    Serial.println("[capacity]: " + (String)capacity);
  }
  
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);
  
  if(!error){
    graphDataX[i] = doc["x"].as<String>();
    graphDataY[i] = doc["y"];
    
    return true;   
  }
  if(DEBUG){
    Serial.print("[json error]: ");
    Serial.println(error.c_str());
  }
  
  return false;
}

String graphToJson(){
  size_t capacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(DRAPH_DATA_AMOUNT) + DRAPH_DATA_AMOUNT * 40 + DRAPH_DATA_AMOUNT * 3;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  
  for(int i = 0; i < DRAPH_DATA_AMOUNT; i++){
      doc["graph"][i] = graphCoordinateToJson(i);
  }
  
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[sent json]: " + json);
           
  return json;
}

bool graphFromJson(String json){
  if(json.length() == 0) return false;
  
  size_t capacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(DRAPH_DATA_AMOUNT) + json.length();
  if(DEBUG){
    Serial.println("[received json]: " + json);
    Serial.println("[capacity]: " + (String)capacity);
  }
  
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);
  
  if(!error){
    for(int i = 0; i < DRAPH_DATA_AMOUNT; i++){
      graphCoordinateFromJson(doc["graph"][i].as<String>(), i);
    }
    
    return true;   
  }
  if(DEBUG){
    Serial.print("[json error]: ");
    Serial.println(error.c_str());
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
      
    return writeToFlash("/time_config.txt", json); 
  }
  if(DEBUG){
    Serial.print("[json error]: ");
    Serial.println(error.c_str());
  }
   
  return false;
}

String wateringConfigToJson(){
  size_t capacity = JSON_OBJECT_SIZE(4) + 16 + 4 * 3;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["min_soil_moisture_p"] = soilMoistureSensor.getMinSoilMoisturePercent();
  doc["wataring_duration"] = wateringManager.getWateringDuration();
  doc["empty_depth_mm"] = liquidTank.getEmptyDepthMm();
  doc["full_depth_mm"] = liquidTank.getFullDepthMm();
    
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[sent json]: " + json);
           
  return json;
}

bool wateringConfigFromJson(String json){
  if(json.length() == 0) return false;
  
  size_t capacity = JSON_OBJECT_SIZE(4) + json.length();
  if(DEBUG){
    Serial.println("[received json]: " + json);
    Serial.println("[capacity]: " + (String)capacity);
  }
  
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);
  
  if(!error){
    soilMoistureSensor.setMinSoilMoisturePercent(doc["min_soil_moisture_p"]);
    wateringManager.setWateringDuration(doc["wataring_duration"]);
    liquidTank.setEmptyDepthMm(doc["empty_depth_mm"]);
    liquidTank.setFullDepthMm(doc["full_depth_mm"]);
            
    return writeToFlash("/watering_config.txt", json);   
  }
  if(DEBUG){
    Serial.print("[json error]: ");
    Serial.println(error.c_str());
  }
  
  return false;
}

String waterTimerToJson(){
  size_t capacity = JSON_OBJECT_SIZE(2) + 4 + 2 * 3;
  String json; 
  
  DynamicJsonDocument doc(capacity);
  doc["status"] = waterTimerState;
  doc["period"] = waterTimerPeriod;
  
  serializeJson(doc, json);
  if(DEBUG) Serial.println("[sent json]: " + json);
           
  return json;
}

bool waterTimerFromJson(String json){
  if(json.length() == 0) return false;
  
  size_t capacity = JSON_OBJECT_SIZE(2) + json.length();
  if(DEBUG){
    Serial.println("[received json]: " + json);
    Serial.println("[capacity]: " + (String)capacity);
  }
  
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);
  
  if(!error){
    waterTimerState = doc["status"];
    waterTimerPeriod = doc["period"];
    waterTimerSeconds = waterTimerPeriod * 3600;
    waterTimerSecondsSave();
    
    if(waterTimerState){
      waterTimer.attach(1, timerAutoWateringTick);
      waterTimerSecondsBackup.attach(3600, waterTimerSecondsSave);
    }
    else{
      waterTimer.detach();
      waterTimerSecondsBackup.detach();
    }
    
    return writeToFlash("/water_timer.txt", json);   
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
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
      fileSize += dir.fileSize();
      str += dir.fileName();
      str += " [";
      str += dir.fileSize();
      str += " Byte]\r\n";
  }
  str += "\r\n[Memory]: " + (String)fileSize + " Byte"; 

  return str;
}

bool handleFileRead(String path){ 
  if(path == "/") path = "/index.html";

  if(path == "/redirect" && CONNECTION_TYPE == ACCESS_POINT) path = "/index.html";

  if(path.endsWith("support_icon.png") || path.endsWith("style.css") || path.endsWith("not_found_img.png") || path.endsWith("phyto.png")){
    path = path.substring(path.lastIndexOf("/"));
  }
  
  String contentType = getContentType(path);
  //Serial.println("[http]: " + path);
  if(SPIFFS.exists(path)){
    File file = SPIFFS.open(path, "r");
    size_t sent = HTTP.streamFile(file, contentType);
    file.close();
    
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
  File file = SPIFFS.open(path, "w");
  if (file){
    if(DEBUG) Serial.println("[(w) " + path + "]: " + text);
    int bytesWritten = file.println(text);
    file.close();
    if(bytesWritten > 0) flag = true;
  }
  
  return flag;
}

String readFromFlash(String path){
  String str = "";
  if(SPIFFS.exists(path)){
    File file = SPIFFS.open(path, "r");
    while(file.available()){
     char ch = file.read();
     str += ch;
    }
    file.close();
  }
  str.trim();
  if(DEBUG) Serial.println("[(r) " + path + "]: " + str);
  
  return str;
}

bool removeFromFlash(String path){
  if(SPIFFS.exists(path)){
    return SPIFFS.remove(path);
  }
  else{
    return false;
  }
}
