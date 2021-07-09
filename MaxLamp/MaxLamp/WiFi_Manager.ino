void displayConnection(){
  switch(CONNECTION_STATE){
    case DISCONNECTED: 
      wifiLed.blink(50, 1450);
      break;
    case AP_CONNECTED:
      wifiLed.blink(1500, 1500);
      break;
    case STA_CONNECTED:
      if(mqttClient.connected()) wifiLed.on();
      else wifiLed.blink(1450, 50);
      break;
  }
}

void connectToWifi_AP(){ 
  if(CONNECTION_TYPE == ACCESS_POINT && CONNECTION_STATE != AP_CONNECTED){      
    if(DEBUG) Serial.println("[creating AP ... ]");
    WiFi.mode(WIFI_AP);
    if(WiFi.softAP(ssidAP, passwordAP)){
      CONNECTION_STATE = AP_CONNECTED;
      if(DEBUG){
        Serial.println("AP created (SSID: " + (String)ssidAP + ")");
        Serial.println("Password: " + (String)passwordAP);
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
      }
    }
    else{
      if(DEBUG) Serial.println("Unsuccessful attempt :(");
    }
  }
}

void connectToWifi_STA(){
  if(CONNECTION_TYPE == WIFI_CLIENT && CONNECTION_STATE != STA_CONNECTED){
    if(DEBUG) Serial.println("[connecting to " + ssidSTA + " ... ]");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidSTA, passwordSTA);
  }
}

void onStationConnected(const WiFiEventStationModeGotIP& evt){
  CONNECTION_STATE = STA_CONNECTED;
  if(DEBUG){
    Serial.println("Connected to " + ssidSTA);
    Serial.print("Local IP address: ");
    Serial.println(WiFi.localIP());
  }
  ipAddress = WiFi.localIP();
  writeToFlash("/sta_ip.txt", WiFi.localIP().toString());
}

void onStationDisconnected(const WiFiEventStationModeDisconnected& evt){
  CONNECTION_STATE = DISCONNECTED;
  if(DEBUG) Serial.println("Disonnected from " + ssidSTA);
}

void connectToMqtt(){
  if(serverMQTT.length() > 0 && CONNECTION_TYPE == WIFI_CLIENT && CONNECTION_STATE == STA_CONNECTED && !mqttClient.connected()){
    if(DEBUG) Serial.println("[connecting to MQTT ... ]");
      
    mqttClient.setServer(serverMQTT.c_str(), portMQTT);
    mqttClient.setCredentials(loginMQTT.c_str(), passwordMQTT.c_str());
    mqttClient.connect();
  }
}

void onMqttConnect(bool sessionPresent){
  if(DEBUG){
    Serial.println("Connected to MQTT");
    Serial.print("[session present]: ");
    Serial.println(sessionPresent);

    Serial.println("[publish]: " + topicDeviceState);

    Serial.println("[subscribe]: " + topicControllerState);
    Serial.println("[subscribe]: " + topicSetAlarm);
    Serial.println("[subscribe]: " + topicSetTicker);
  }

  mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());

  mqttClient.subscribe(topicControllerState.c_str(), 0);
  mqttClient.subscribe(topicSetAlarm.c_str(), 0);
  mqttClient.subscribe(topicSetTicker.c_str(), 0);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total){
  String receivedTopic = (String)topic;
  String message = "";
  
  for (int i = 0; i < len; i++) 
    message += (char)payload[i];

  if(DEBUG){
    Serial.println("[topic]: " + receivedTopic);
    Serial.println("[payload]: " + message);
  }

  if(receivedTopic.equals(topicControllerState)){
    setJsonStatus(message);
  }
  else if(receivedTopic.equals(topicSetAlarm)){
    if(LEVEL == ALARM) alarm.off();     
    if(alarm.fromJson(message)){
      writeToFlash("/alarm.txt", message);      
    }     
  }
  else if(receivedTopic.equals(topicSetTicker)){    
    if(ticker.fromJson(message)){
      writeToFlash("/ticker.txt", message);      
    }     
  }

  /*
  else if(receivedTopic.equals(topicSetBrightness)){
    byte _brightness = message.toInt();
    if(_brightness >= MIN_BRIGHTNESS && _brightness <= MAX_BRIGHTNESS){
      if(LEVEL == EFFECTS) setBrightness(_brightness);
      mqttClient.publish(topicGetBrightness.c_str(), 0, true, ((String)brightness).c_str());      
    }
  }
  else if(receivedTopic.equals(topicSetVolume)){
    byte _volume = message.toInt();
    if(_volume >= 1 && _volume <= 15) setVolume(_volume);
    mqttClient.publish(topicGetVolume.c_str(), 0, true, ((String)pixelVolume).c_str());
  }
  else if(receivedTopic.equals(topicSetScale)){  
    if(LEVEL == EFFECTS) setScale(message.toInt());
    mqttClient.publish(topicGetScale.c_str(), 0, true, (getJsonScale()).c_str());
  }
  else if(receivedTopic.equals(topicSetSpeed)){
    if(LEVEL == EFFECTS) setSpeed(message.toInt());
    mqttClient.publish(topicGetSpeed.c_str(), 0, true, (getJsonSpeed()).c_str());
  }
  */
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason){
  if(DEBUG) Serial.println("Disconnected from MQTT");
}

void disconnect(){
  if(DEBUG) Serial.println("[disconnecting ... ]"); 
 
  if(mqttClient.connected())
    mqttClient.disconnect();
    
  if(CONNECTION_STATE == STA_CONNECTED)
    WiFi.disconnect();
  else if(CONNECTION_STATE == AP_CONNECTED){
    WiFi.softAPdisconnect(true);
    if(DEBUG) Serial.println("AP is disabled");
  }

  CONNECTION_STATE = DISCONNECTED;
}
