class Effect{
  public:
    byte scaleStep;
    byte speedStep;
  private:
    byte scale;
    byte speed;
    byte MIN_SCALE;
    byte DEFAULT_SCALE;
    byte MAX_SCALE;
    byte MIN_SPEED;
    byte DEFAULT_SPEED;
    byte MAX_SPEED;
    
  public:    
    Effect(void (*callback)(void) = NULL, byte DEFAULT_SCALE_ = 20, byte DEFAULT_SPEED_ = 20, 
           byte scaleStep_ = 2, byte speedStep_ = 2, byte MAX_SCALE_ = 255, byte MAX_SPEED_ = 60, byte MIN_SCALE_ = 0, byte MIN_SPEED_ = 2){
            
      DEFAULT_SCALE = DEFAULT_SCALE_;
      DEFAULT_SPEED = DEFAULT_SPEED_;
      scaleStep = scaleStep_;
      speedStep = speedStep_;
      MAX_SPEED = MAX_SPEED_;
      MAX_SCALE = MAX_SCALE_;
      MIN_SPEED = MIN_SPEED_;
      MIN_SCALE = MIN_SCALE_;

      _onRun = callback;
      
      this->setDefaults();
    }
    
    void (*_onRun)(void);

    void setFunction(void (*callback)(void) = NULL){
      _onRun = callback;
    }
    
    void scaleIncrement(){
      int value = scale + scaleStep;
      if(value < MIN_SCALE) value = MAX_SCALE;
      else if(value > MAX_SCALE) value = MIN_SCALE;
      setScale(value);
    }
    
    void scaleDecrement(){
      int value = scale - scaleStep;
      if(value < MIN_SCALE) value = MAX_SCALE;
      else if(value > MAX_SCALE) value = MIN_SCALE;
      setScale(value);
    }

    void setScale(int value){
      if(value >= MIN_SCALE && value <= MAX_SCALE) scale = value;
      else if(value < MIN_SCALE) scale = MIN_SCALE;
      else if(value > MAX_SCALE) scale = MAX_SCALE;
    }

    byte getScale(){
      return scale;
    }

    byte getMinScale(){
      return MIN_SCALE;
    }
    
    byte getMaxScale(){
      return MAX_SCALE;
    }

    void speedIncrement(){
      int value = speed - speedStep;
      if(value < MIN_SPEED) value = MAX_SPEED;
      else if(value > MAX_SPEED) value = MIN_SPEED;
      setSpeed(value);
    }
    
    void speedDecrement(){
      int value = speed - speedStep;
      if(value < MIN_SPEED) value = MAX_SPEED;
      else if(value > MAX_SPEED) value = MIN_SPEED;
      setSpeed(value);
    }
    
    void setSpeed(int value){
      if(value >= MIN_SPEED && value <= MAX_SPEED) speed = value;
      else if(value < MIN_SPEED) speed = MIN_SPEED;
      else if(value > MAX_SPEED) speed = MAX_SPEED;
    }

    byte getSpeed(){
      return speed;
    }

    byte getMinSpeed(){
      return MIN_SPEED;
    }
    
    byte getMaxSpeed(){
      return MAX_SPEED;
    }
        
    void setDefaults(){
      setScale(DEFAULT_SCALE);
      setSpeed(DEFAULT_SPEED);
    }

    byte getDefaultScale(){
      return DEFAULT_SCALE;
    }

    byte getDefaultSpeed(){
      return DEFAULT_SPEED;
    }
    
    void run(){
      if(_onRun != NULL)
        _onRun();
    }
};

class TimeManager{
  public:
    const byte OFF = 0;
    const byte ON = 1;
    const byte RUN = 2;
    const byte RESET = 3;
    const byte STOP = 4;
  
  protected:
    bool (*writeToFlash)(String path, String text) = NULL;
    String (*readFromFlash)(String path) = NULL;
    
  public:
    
    void init(bool (*_writeToFlash)(String path, String text), String (*_readFromFlash)(String path)){
      writeToFlash = _writeToFlash;
      readFromFlash = _readFromFlash;
    }
    
    virtual void tick() = 0;

    virtual String toJson() = 0;
    
    virtual bool fromJson(String json) = 0;

    static String getTimeNow(){
      return str(hour(now())) + ":" + str(minute(now())) + ":" + str(second(now()));
    }

    static String formatSeconds(unsigned int _seconds){
      int hours = _seconds / 3600;
      String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);
    
      int minutes = (_seconds % 3600) / 60;
      String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);
    
      int seconds = _seconds % 60;
      String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);
    
      //Serial.println("[countTimeFromSeconds()]: " + hoursStr + ":" + minuteStr + ":" + secondStr);
      return hoursStr + ":" + minuteStr + ":" + secondStr;
    }
    
  protected:
    static int getCalculatedTimeSeconds(String time1, String time2){
      int arrayTime1[3];
      parseTime(time1, arrayTime1);
      int arrayTime2[3];
      parseTime(time2, arrayTime2);

      int _secondsBetween = (arrayTime2[0] + arrayTime1[0]) * 3600 + (arrayTime2[1] + arrayTime1[1]) * 60 + arrayTime2[2] + arrayTime1[2];
      if(_secondsBetween >= 86400) _secondsBetween -= 86400;

      return _secondsBetween;
    }
    
    static int getSecondsBetween(String time1, String time2){
      int arrayTime1[3];
      parseTime(time1, arrayTime1);
      int arrayTime2[3];
      parseTime(time2, arrayTime2);
    
      int secondsBetween = (arrayTime2[0] - arrayTime1[0]) * 3600 + (arrayTime2[1] - arrayTime1[1]) * 60 + arrayTime2[2] - arrayTime1[2];
      if(secondsBetween < 0) secondsBetween = 86400 + secondsBetween;
    
      //Serial.println("[getSecondsBetween()]: " + (String)secondsBetween);
      return secondsBetween;
    }

    static void parseTime(String timeStr, int *timeArray){
      timeArray[0] = (timeStr.substring(0, timeStr.indexOf(':'))).toInt();
      timeArray[1] = (timeStr.substring(timeStr.indexOf(':') + 1, timeStr.lastIndexOf(':'))).toInt();
      timeArray[2] = (timeStr.substring(timeStr.lastIndexOf(':') + 1)).toInt();
      //Serial.println("[parseTime()]: " + (String)timeArray[0] + ":" + (String)timeArray[1]);
    }

    static String str(byte num){
      String str = (String)num;
      
      if(str.length() == 1) return "0" + str;
      else return str;
    }
};

class Alarm : public TimeManager{
  public:
    byte id;
    byte state;
    bool isReplay;
    byte effectID;
    byte outset;
    byte duration;
    String time;
    unsigned int secondsTicker;
  private:
    byte volume;
    unsigned long alarmTicker = 0;
    unsigned long brightnessTicker = 0;
    unsigned long volumeTicker = 0;
    unsigned int BRIGHTNESS_PERIOD = 0;
    unsigned int VOLUME_PERIOD = 0;
    
  public:
    Alarm(byte id_, void (*callback)(void) = NULL){
      id = id_;
    }
    
    void tick() override{
      if(state != OFF && state != RESET && isTimeUpdated && abs(millis() - alarmTicker) >= 1000){
        alarmTicker = millis();

        if(state == ON){
          secondsTicker = getSecondsBetween(getTimeNow(), time + ":00");
          if(getTimeNow().startsWith(time) && secondsTicker < 86330) run();
        }
        else if(state == RUN){
          secondsTicker++;
          if(secondsTicker >= (outset + duration) * 60){
            if(DEBUG) Serial.println("[stop in tick (run)]");
            stop();
          }
        }

        if(DEBUG) Serial.println("[alarm tick]: " + (String)secondsTicker);
      }
    }

    bool run(){
      if(state == RUN){
        if(secondsTicker <= outset * 60){       
          if(abs(millis() - brightnessTicker) >= BRIGHTNESS_PERIOD){
            brightnessTicker = millis();
            if(brightness + BRIGHTNESS_STEP <= MAX_BRIGHTNESS){
              brightness += BRIGHTNESS_STEP;
              FastLED.setBrightness(brightness);       
            }
          }

          if(abs(millis() - volumeTicker) >= VOLUME_PERIOD){
            volumeTicker = millis();
            if(volume + 1 <= 30){
              volume++;
              pixelVolume = volume / 2;
              mp3_set_volume(volume);       
            }
          }
        }
        
        return true;
      }
      else if(state == ON){
        if(DEBUG) Serial.println("[alarm run]");
        state = RUN;
        LEVEL = ALARM;
        secondsTicker = 0;
        currentEffect = effectID;
        FastLED.clear();
        loadingFlag = true;
        mp3_stop();
        isPlaying = false;
        delay(50);
        isSound = true;
        volume = 1;
        pixelVolume = 1;
        mp3_set_volume(pixelVolume);
        brightness = MIN_BRIGHTNESS;
        FastLED.setBrightness(brightness);

        BRIGHTNESS_PERIOD = ((outset * 60) / MAX_BRIGHTNESS) * 1000;
        VOLUME_PERIOD = ((outset * 60) / 30) * 1000;

        return false;
      }
      else return false;
    }

    void stop(){
      if(DEBUG) Serial.println("[alarm stop]");
      if(isReplay) on();
      else reset();
      
      LEVEL = EFFECTS;
      FastLED.clear();
      loadingFlag = true;
      mp3_stop();
      isPlaying = false;
      delay(50);
      if(readFromFlash("/current_effect.txt").length() != 0) currentEffect = readFromFlash("/current_effect.txt").toInt();
      if(readFromFlash("/brightness.txt").length() != 0) brightness = readFromFlash("/brightness.txt").toInt();
      FastLED.setBrightness(brightness);
      if(readFromFlash("/sound_state.txt").length() != 0) isSound = (bool)readFromFlash("/sound_state.txt").toInt();
      if(readFromFlash("/sound_volume.txt").length() != 0){
        pixelVolume = readFromFlash("/sound_volume.txt").toInt();
        mp3_set_volume(pixelVolume * 2);
        delay(10);
      }    
    }

    void on(bool isReplay_, byte effectID_, byte outset_, byte duration_, String time_){
      if(DEBUG) Serial.println("[alarm on]");
      state = ON;
      isReplay = isReplay_;
      effectID = effectID_;
      outset = outset_;
      duration = duration_;
      time = time_;
      secondsTicker = 0;
      writeToFlash("/alarm.txt", toJson());
    }

    void on(bool writeFlag = true){
      if(DEBUG) Serial.println("[alarm on]");
      state = ON;
      secondsTicker = 0;
      if(writeFlag) writeToFlash("/alarm.txt", toJson());
    }

    void off(bool writeFlag = true){
      if(DEBUG) Serial.println("[alarm off]");
      state = OFF;
      secondsTicker = 0;
      if(writeFlag) writeToFlash("/alarm.txt", toJson());
    }
    
    void reset(bool writeFlag = true){
      if(DEBUG) Serial.println("[alarm reset]");
      state = RESET;
      isReplay = false;
      effectID = 0;
      outset = 0;
      duration = 0;
      time = "--:--";
      secondsTicker = 0;
      if(writeFlag) writeToFlash("/alarm.txt", toJson());
    }

    String toJson() override{
      size_t capacity = JSON_OBJECT_SIZE(6) + 6 * 3;
      String json; 
      
      DynamicJsonDocument doc(capacity);
      doc["state"] = (byte)state;
      doc["replay"] = (byte)isReplay;
      doc["effect_id"] = effectID;
      doc["time"] = time;
      doc["outset"] = outset;
      doc["duration"] = duration;
      serializeJson(doc, json);
      if(DEBUG) Serial.println("[json]: " + json);
      
      return json;
    }
    
    bool fromJson(String json) override{
      if(json.length() == 0) return false;
      if(DEBUG) Serial.println("[json]: " + json);
      
      size_t capacity = JSON_OBJECT_SIZE(6) + json.length();
      DynamicJsonDocument doc(capacity);
      DeserializationError error = deserializeJson(doc, json);
      
      if(!error){
        state = doc["state"];

        if(state == RESET){
          reset(false);
        }
        else{
          isReplay = (bool)doc["replay"];
          effectID = doc["effect_id"];
          time = doc["time"].as<String>();
          outset = doc["outset"];
          duration = doc["duration"];
          secondsTicker = 0;
        }
        
        return true;   
      }
      
      return false;
    }
};

class TimeTicker : public TimeManager{  
  public:
    const byte NONE = 0;
    const byte ALARM = 1;
    const byte TIMER = 2;
    
    byte id;
    byte state;
    byte mode;
    byte type;
    String time;
    unsigned int secondsTicker;
    unsigned int prevSecondsTicker;

    void (*run)(void)  = NULL;
    
  private:
    unsigned long timeTicker = 0;
    
  public:
    TimeTicker(byte id_){
      id = id_;
    }

    void init(bool (*_writeToFlash)(String path, String text), String (*_readFromFlash)(String path), void (*_run)(void)){
      writeToFlash = _writeToFlash;
      readFromFlash = _readFromFlash;
      run = _run;
    }
    
    void tick() override{
      if(state == ON && isTimeUpdated && abs(millis() - timeTicker) >= 1000){
        timeTicker = millis();

        if(time.equals("--:--:--")) fromJson(readFromFlash("/ticker.txt"));

        prevSecondsTicker = secondsTicker;
        secondsTicker = getSecondsBetween(getTimeNow(), time);
        
        if(secondsTicker == 0 || (prevSecondsTicker < secondsTicker && secondsTicker > 86340)) run();

        if(DEBUG) Serial.println("[ticker tick]: " + (String)secondsTicker);
      }
    }

    void on(byte type_, byte mode_, String time_){
      if(DEBUG) Serial.println("[ticker on]");
      state = ON;
      type = type_;
      mode = mode_;

      if(type == this->TIMER){
        secondsTicker = getSecondsBetween("00:00:00", time_);

        if(!isTimeUpdated) time = "--:--:--";
        else time = formatSeconds(getCalculatedTimeSeconds(getTimeNow(), time_));
      }
      else{
        time = time_;
        secondsTicker = 0;
      }
      prevSecondsTicker = secondsTicker;
      
      writeToFlash("/ticker.txt", toJson());
    }

    void on(bool writeFlag = true){
      if(DEBUG) Serial.println("[ticker on]");
      if(type == this->TIMER)fromJson(readFromFlash("/ticker.txt"));
      state = ON;
      if(writeFlag) writeToFlash("/ticker.txt", toJson());
    }

    void off(bool writeFlag = true){
      if(DEBUG) Serial.println("[ticker off]");
      if(type == this->TIMER)fromJson(readFromFlash("/ticker.txt"));
      state = OFF;
      if(writeFlag) writeToFlash("/ticker.txt", toJson());
    }
    
    void reset(bool writeFlag = true){
      if(DEBUG) Serial.println("[ticker reset]");
      state = RESET;
      type = this->NONE;
      mode = 20;
      time = "--:--:--";
      secondsTicker = 0;
      prevSecondsTicker = secondsTicker;
      if(writeFlag) writeToFlash("/ticker.txt", toJson());
    }

    void updateTime(){
      if(state == ON && type == this->TIMER)
        time = formatSeconds(getCalculatedTimeSeconds(getTimeNow(), formatSeconds(secondsTicker)));
    }

    String toJson() override{
      size_t capacity = JSON_OBJECT_SIZE(4) + 4 * 3;
      String json;
      
      DynamicJsonDocument doc(capacity);
      doc["state"] = (byte)state;
      doc["type"] = (byte)type;
      doc["mode"] = mode;
      if(type == this->ALARM) doc["time"] = time;
      else if(type == this->TIMER) doc["time"] = formatSeconds(secondsTicker);
      else doc["time"] = "--:--:--";
      
      serializeJson(doc, json);
      if(DEBUG) Serial.println("[json]: " + json);
      
      return json;
    }
    
    bool fromJson(String json) override{
      if(json.length() == 0) return false;
      if(DEBUG) Serial.println("[json]: " + json);
      
      size_t capacity = JSON_OBJECT_SIZE(4) + json.length();
      DynamicJsonDocument doc(capacity);
      DeserializationError error = deserializeJson(doc, json);
      
      if(!error){
        state = doc["state"];

        if(state == RESET){
          reset(false);
        }
        else{
          type = doc["type"];
          mode = doc["mode"];
        
          if(type == this->TIMER){
            secondsTicker = getSecondsBetween("00:00:00", doc["time"]);
            
            if(!isTimeUpdated) time = "--:--:--";
            else time = formatSeconds(getCalculatedTimeSeconds(getTimeNow(), doc["time"]));
          }
          else{
            time = doc["time"].as<String>();
            secondsTicker = 0;
          }
          prevSecondsTicker = secondsTicker;
        }
        
        return true;   
      }
      
      return false;
    }
};

class Led{
  public:
    bool state;
  private:
    byte pinLed;
    bool stateOff;
    unsigned long timer;

  public:
    Led(byte pinLed_, bool stateOff_ = LOW){
      pinLed = pinLed_;
      stateOff = stateOff_;

      pinMode(pinLed, OUTPUT);

      this->off();
    }

    void blink(unsigned long ON_TIME, unsigned long OFF_TIME){
      if(isOff() && millis() - timer >= OFF_TIME){
        timer = millis();
        this->on();
      }
      else if(isOn() && millis() - timer >= ON_TIME){
        timer = millis();
        this->off();
      }
    }
    
    void on(){
      if(isOff()){
        digitalWrite(pinLed, !stateOff);
        state = true;
      }
    }
  
    void off(){
      if(isOn()){
        digitalWrite(pinLed, stateOff);
        state = false;
      }
    }

    bool isOn(){
      return state;
    }
  
    bool isOff(){
      return !state;
    }
};
