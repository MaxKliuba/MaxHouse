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



class TimeTicker : public TimeManager{  
  public:
    const byte NONE = 0;
    const byte ALARM = 1;
    const byte TIMER = 2;

    const byte MODE_OFF = 0;
    const byte MODE_ON = 1;
    
    byte id;
    byte state;
    byte output;
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

    void on(byte output_, byte type_, byte mode_, String time_){
      if(DEBUG) Serial.println("[ticker on]");
      output = output_;
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
      output = 0;
      type = this->NONE;
      mode = MODE_OFF;
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
      size_t capacity = JSON_OBJECT_SIZE(5) + 5 * 3;
      String json;
      
      DynamicJsonDocument doc(capacity);
      doc["output"] = (byte)output;
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
      
      size_t capacity = JSON_OBJECT_SIZE(5) + json.length();
      DynamicJsonDocument doc(capacity);
      DeserializationError error = deserializeJson(doc, json);
      
      if(!error){
        output = doc["output"];
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


class Output
{
  private:
    uint8_t pin;
    uint8_t led;
    bool stateOffP;
    bool stateOffL;
    bool state;
  
  public:
    Output(uint8_t pin_, uint8_t led_, bool stateOffP_ = LOW, bool stateOffL_ = LOW)
    {
      pin = pin_;
      led = led_;

      stateOffP = stateOffP_;
      stateOffL = stateOffL_;

      pinMode(pin, OUTPUT);
      pinMode(led, OUTPUT);
  
      this->off();
    }

    void setStatus(byte st)
    {
      if(st == 1) on();
      else off();
    }
    
    byte Switch()
    {
      if(this->isOn()) this->off();
      else this->on();

      return this->getStatus();
    }
    
    byte on()
    {
      digitalWrite(led, !stateOffL);
      digitalWrite(pin, !stateOffP);
      state = true;

      return this->getStatus();
    }
  
    byte off()
    {
      digitalWrite(led, stateOffL);
      digitalWrite(pin, stateOffP);
      state = false;

      return this->getStatus();
    }

    byte getStatus()
    {
      if(state) return 1;
      else return 0;
    }
  
    bool isOn()
    {
      return state;
    }
  
    bool isOff()
    {
      return !state;
    }

    uint8_t getPin()
    {
      return pin;
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
