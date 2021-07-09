void checkDepth(String text){
  int newDepthPercent = liquidTank.getDepthPercent();

  if(depthPercent != newDepthPercent){
    depthPercent = newDepthPercent;
    logger.write(Log::LOG_INFO, "Measurement of liquid level in the tank [" + text + "]", (String)depthPercent + "%");
    if(depthPercent <= liquidTank.getMinDepthPercent()){
      logger.write(Log::LOG_WARN, "Low liquid level in the tank");
    }
  }
}

void checkSoilMoisture(String text){
  int newSoilMoisturePercent = soilMoistureSensor.getSoilMoisturePercent();
  
  if(soilMoisturePercent != newSoilMoisturePercent){
    soilMoisturePercent = newSoilMoisturePercent;
    logger.write(Log::LOG_INFO, "Measurement of soil moisture level [" + text + "]", (String)soilMoisturePercent + "%");
    if(soilMoisturePercent == -1){
      logger.write(Log::LOG_WARN, "Soil moisture sensor not found");
    }
    else if(soilMoisturePercent <= soilMoistureSensor.getMinSoilMoisturePercent()){
      logger.write(Log::LOG_WARN, "Low soil moisture");
    }
  }
}

void checkTimeState(){
  if(CONNECTION_STATE == STA_CONNECTED){
    if(timeClient.update()){
      isNtpUpdated = true;
      isTimeUpdated = true;
      
      setTime(timeZone.toLocal(timeClient.getEpochTime()));
      logger.write(Log::LOG_INFO, "Time updated [ntp]");
      if(DEBUG){
        Serial.println("[time now UTC]: " + (String)now());
        Serial.println("[formatted time now UTC ]: " + TimeManager::getTimeNow());
      }
    }
  }
  else if(isTimeUpdated && abs(millis() - updateTimeTicker) >= UPDATE_INTERVAL){
    updateTimeTicker = millis();

    setTime(timeZone.toLocal(timeClient.getEpochTime()));
    logger.write(Log::LOG_INFO, "Time updated [ntp]");
    if(DEBUG){
      Serial.println("[time now UTC]: " + (String)now());
      Serial.println("[formatted time now UTC ]: " + TimeManager::getTimeNow());
    }
  }
}


void displayState(){
  if(depthPercent <= liquidTank.getMinDepthPercent() && !wateringManager.isWatering()){
    stateLed.blink(100, 100);
  }
  else if(wateringManager.isWatering()){
    stateLed.on();
  }
  else{
    stateLed.off();
  }
}


void graphTick(){
  if(isTimeUpdated){
    for(byte i = 0; i < 3; i++){
      if(hour(now()) == t[i] && !graphDataX[DRAPH_DATA_AMOUNT - 1].startsWith(TimeManager::getDateTimeNow().substring(0, 12))){
        for(byte j = 0; j < DRAPH_DATA_AMOUNT - 1; j++){
          graphDataX[j] = graphDataX[j + 1];
          graphDataY[j] = graphDataY[j + 1];
        }

        graphDataX[DRAPH_DATA_AMOUNT - 1] = TimeManager::getDateTimeNow();
        checkSoilMoisture("graph tick");
        graphDataY[DRAPH_DATA_AMOUNT - 1] = soilMoisturePercent;
      }
    }
  }
}

void timerAutoWateringTick(){
  if(waterTimerSeconds == 0){
    if(!wateringManager.isWatering() && depthPercent > liquidTank.getMinDepthPercent()){
      wateringManager.autoWatering();
      logger.write(Log::LOG_INFO, "Auto watering [timer]", "time: " + (String)wateringManager.getWateringDuration() + "s");
    }
    waterTimerSeconds = waterTimerPeriod * 3600;
    waterTimerSecondsSave();
  }
  else {
      waterTimerSeconds--;
  }
}

void waterTimerSecondsSave(){
  writeToFlash("/water_timer_seconds.txt", (String)waterTimerSeconds);
}

void calibrateDepth(){
  bool level = 0;
  
  while(level == 0){
    button.tick();
    stateLed.blink(300, 50);

    if(button.hasClicks()){
      switch(button.getClicks()){
        case 1:
          liquidTank.setEmptyDepthMm(liquidTank.getDepthMm());
          break;
        case 2:
          level = 1;
        case 3:
          level = 2;
          break;
      } 
    }
  }

  while(level == 1){
    button.tick();
    stateLed.blink(50, 50);

    if(button.hasClicks()){
      switch(button.getClicks()){
        case 1:
          liquidTank.setFullDepthMm(liquidTank.getDepthMm());
          break;
        case 2:
          level = 2;
        case 3:
          level = 2;
          break;
      }
    }
  }
}


bool isStrDigit(String str){
  for (int i = 0; i < str.length(); i++) 
    if(!isdigit(str[i])) return false;

  return true;
}
