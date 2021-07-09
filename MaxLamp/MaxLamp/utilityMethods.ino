void fillAll(CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
  }
}

uint16_t getPixelNumber(int8_t x, int8_t y) {
  if (y % 2 == 0) return (y * WIDTH + x);
  else return (y * WIDTH + WIDTH - x - 1);
}

void drawPixelXY(int8_t x, int8_t y, CRGB color) {
  if (x < 0 || x > WIDTH - 1 || y < 0 || y > HEIGHT - 1) return;
  leds[getPixelNumber(x, y)] = color;
}

uint32_t getPixColor(int thisPixel) {
  if (thisPixel < 0 || thisPixel > NUM_LEDS - 1) return 0;
  return (((uint32_t)leds[thisPixel].r << 16) | ((long)leds[thisPixel].g << 8 ) | (long)leds[thisPixel].b);
}

uint32_t getPixColorXY(int8_t x, int8_t y) {
  return getPixColor(getPixelNumber(x, y));
}

void drawRow(byte y, CRGB color){
  for(byte i = 0; i < WIDTH; i++){
    drawPixelXY(i, y, color);
  }
}

bool isStrDigit(String str){
  for (int i = 0; i < str.length(); i++) 
    if(!isdigit(str[i])) return false;

  return true;
}

void attachInterruptFun(){
  attachInterrupt(digitalPinToInterrupt(KEY_PIN), encoderTick, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S1_PIN), encoderTick, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S2_PIN), encoderTick, CHANGE);
}

void detachInterruptFun(){
  detachInterrupt(digitalPinToInterrupt(KEY_PIN));
  detachInterrupt(digitalPinToInterrupt(S1_PIN));
  detachInterrupt(digitalPinToInterrupt(S2_PIN));
}

void on(){
  isOn = true;
  loadingFlag = true;
  writeToFlash("/state.txt", (String)((byte)isOn));
  if(mqttClient.connected()) mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());
  mp3Stop();
}

void off(){
  FastLED.clear();
  isOn = false;
  loadingFlag = true;
  writeToFlash("/state.txt", (String)((byte)isOn));
  if(mqttClient.connected()) mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());
  mp3Stop();
}

void checkEffectsMode(){
  if(currentEffect != previousEffect){
    previousEffect = currentEffect;
    FastLED.clear();
    loadingFlag = true;
    writeToFlash("/current_effect.txt", (String)currentEffect);
    writeEffectConfig();
    mp3Stop();
    delay(50);
  }
}

void setLevel(Level level){
  FastLED.clear();
  loadingFlag = true;
  mp3Stop();
  delay(50);
  
  switch(level){
    case MENU:
      menuLevel = currentEffect;
      previousMenuLevel = menuLevel;
      LEVEL = MENU; 
      FastLED.setBrightness(MAX_BRIGHTNESS);
      break;
    case EFFECTS: 
      LEVEL = EFFECTS; 
      FastLED.setBrightness(brightness);
      break;
  }
  //writeToFlash("/level_mode.txt", (String)LEVEL);
}

void updateMatrix(byte updatePeriod){
  if(abs(millis() - updateTimer) >= updatePeriod){
    updateTimer = millis();
    if((LEVEL == EFFECTS && isOn) || LEVEL == ALARM) effects[currentEffect].run();  
    FastLED.show();
  }
}

void mp3Play(int track){
  mp3_play(track);
  isPlaying = true;
}

void mp3Stop(){
  mp3_stop();
  isPlaying = false;
}

void setBrightness(byte value){
  brightness = value;
  FastLED.setBrightness(brightness);
  isBrightnessReady = true;
  brightnessCheckTimer = millis();
}

void setVolume(byte value){
  pixelVolume = value;
  mp3_set_volume(pixelVolume * 2);
  delay(10);
  isVolumeReady = true;
  volumeCheckTimer = millis();
}

void setSound(bool value){
  isSound = value;
  writeToFlash("/sound_state.txt", (String)((byte)isSound));
  if(mqttClient.connected()) mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());
  mp3Stop();
  delay(50);
}

void setScale(byte value){
  effects[currentEffect].setScale(value);
  effConfigReady = true;
  effConfigCheckTimer = millis();
}

void setSpeed(byte value){
  effects[currentEffect].setSpeed(value);
  effConfigReady = true;
  effConfigCheckTimer = millis();
}

void writeBrightness(){
  if(LEVEL == EFFECTS) writeToFlash("/brightness.txt", (String)brightness);
  if(mqttClient.connected()) mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());
}

void writeVolume(){
  writeToFlash("/sound_volume.txt", (String)pixelVolume);
  if(mqttClient.connected()) mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());
}

void writeEffectConfig(){
  if(LEVEL == EFFECTS) writeToFlash("/effect_" + (String)currentEffect + ".txt", effectsConfigToJson(currentEffect));
  if(mqttClient.connected()) mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());
}

void tickerRun(){
  if(DEBUG) Serial.println("[ticker run]");
  
  if(ticker.mode == 20){
     if(LEVEL == ALARM) alarm.stop();
     if(LEVEL != EFFECTS) setLevel(EFFECTS);   
     off();
  }
  else{
    if(LEVEL == ALARM) alarm.stop();
    if(LEVEL != EFFECTS) setLevel(EFFECTS);  
      currentEffect = ticker.mode;
      checkEffectsMode();
      on();
      if(isSound){
        mp3Play(currentEffect);
        delay(50);
    }         
  }
  ticker.off();
}
