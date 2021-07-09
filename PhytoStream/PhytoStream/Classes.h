class TimeManager{
  public:
    static String getTimeNow(time_t t = now()){
      if(isTimeUpdated){
        return str(hour(t)) + ":" + str(minute(t)) + ":" + str(second(t));
      }
      else {
        return "--:--:--";
      }
    }

    static String getDateNow(time_t t = now()){
      if(isTimeUpdated){
        return (String)year(t) + "." + str(month(t)) + "." + str(day(t));
      }
      else {
        return "----.--.--";
      }
    }

    static String getDateTimeNow(time_t t = now()){
      return getDateNow(t) + " " + getTimeNow(t);
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

    static String str(int num){
      String str = (String)num;
      
      if(str.length() == 1) return "0" + str;
      else return str;
    }
};


class Log {
  public:
    const static byte LOG_DEBUG = 0;
    const static byte LOG_INFO = 1;
    const static byte LOG_WARN = 2;
    const static byte LOG_ERROR = 3;

  private:
    String path;
    unsigned long maxSize;

  public:
    Log(String _path, unsigned long _maxSize = 500000){
      path = _path;
      maxSize = _maxSize;
    }

    bool write(byte type, String text){
      checkSize();
      return this->writeToFlash(type, text);
    }

    bool write(byte type, String text, String value){
      return this->write(type, text + "\t (" + value + ")");
    }

  private:
    bool writeToFlash(byte type, String text){
      bool flag = false;
      
      if(LOG){      
        File logfile = SPIFFS.open(path, "a"); 
        if (logfile){        
          String finalText = TimeManager::getDateTimeNow() + "\t";
          switch(type){
            case LOG_DEBUG: finalText += "DEBUG\t"; break;
            case LOG_INFO: finalText += "INFO\t"; break;
            case LOG_WARN: finalText += "WARN\t"; break;
            case LOG_ERROR: finalText += "ERROR\t"; break;
          }
          int bytesWritten = logfile.println(finalText + text);
          logfile.close();
          if(bytesWritten > 0) flag = true;

          if(DEBUG){
            Serial.println(finalText + text);
          }
        }
      }
      
      return flag;
    }
    
    void checkSize() {
      File logfile = SPIFFS.open(path, "a");
      int fileSize = logfile.size();
      
      if (logfile){
        if(fileSize == 0 || fileSize > maxSize){
          logfile.close();
          if(fileSize > maxSize){
            SPIFFS.remove(path);
          }
          writeToFlash(LOG_INFO, "Log file created");
        }
      }
    }
};

Log logger("/logfile.log");


class WateringManager {
  public:
    static byte const WATERING_OFF = 0;
    static byte const WATERING_AUTO = 1;
    static byte const WATERING_MANUAL = 2;

  private:
    byte state;
    bool stateOff;
    byte pumpPin;
    unsigned int wateringDuration;
    unsigned long startWateringTime;
    bool flag;

  public:
    WateringManager(byte _pumpPin, unsigned int _wateringDuration = 5, bool _stateOff = LOW){
      pumpPin = _pumpPin;
      this->setWateringDuration(_wateringDuration);
      stateOff =_stateOff;
      pinMode(pumpPin, OUTPUT);         
      this->off();
      flag = true;
    }

    void on(){
      digitalWrite(pumpPin, !stateOff);
      logger.write(Log::LOG_INFO, "Pump on");
    }
  
    void off(){
      digitalWrite(pumpPin, stateOff);
      state = WATERING_OFF;
      logger.write(Log::LOG_INFO, "Pump off");
    }

    void autoWatering(){
      if(state == WATERING_OFF){
        startWateringTime = millis();
        on();
        state = WATERING_AUTO;
      }
    }

    void manualWatering(bool _state){
      if(_state && state == WATERING_OFF){
        on();
        state = WATERING_MANUAL;
        logger.write(Log::LOG_INFO, "Manual watering [device]", "time: custom");
      }
      else if(!_state && state == WATERING_MANUAL){
        flag = true;
        this->off();
      }
    }

    bool tick(){
      if(state == WATERING_AUTO && abs(millis() - startWateringTime) >= wateringDuration * 1000){
        flag = true;
        this->off();
      }

      if(flag){
        flag = false;
        return true;
      }
      
      return flag;
    }

    byte getState(){
      return state;
    }

    bool isWatering(){
      return state != WATERING_OFF;
    }

    void setWateringDuration(unsigned int _wateringDuration){
      wateringDuration = _wateringDuration;
    }

    unsigned int getWateringDuration(){
      return wateringDuration;
    }
};


class Filterer {
  private:
    double medianK;
    double filVal = 228;
    double buf[3];
    byte count = 0;
    
  public:
    Filterer(double _medianK) {
      this->setK(_medianK);
    }
  
    Filterer() {
      medianK = 0.1;
    }
  
    double getK() {
      return medianK;
    }
  
    void setK(double _medianK) {
      medianK = _medianK;
    }
  
    double expRunningAverage(double newVal) {
      filVal += (newVal - filVal) * getK();
      return filVal;
    }
  
    double median(double newVal) {
      buf[count] = newVal;
      if (++count >= 3) count = 0;
      double a = buf[0];
      double b = buf[1];
      double c = buf[2];
      double middle;
      if ((a <= b) && (a <= c)) {
        middle = (b <= c) ? b : c;
      }
      else {
        if ((b <= a) && (b <= c)) {
          middle = (a <= c) ? a : c;
        }
        else {
          middle = (a <= b) ? a : b;
        }
      }
      return middle;
    }
};


class LiquidTank{
  public:

  private:
    byte triggerPin;
    byte echoPin;
    unsigned int emptyDepthMm;
    unsigned int fullDepthMm;
    byte minDepthPercent;
    const unsigned int MAX_DISTANCE = 4000;
    Filterer sonicFilt = Filterer();

  public:
    LiquidTank(byte _triggerPin, byte _echoPin, unsigned int _emptyDepthMm = 200, unsigned int _fullDepthMm = 50, byte _minDepthPercent = 10){
      this->setSensorPins(_triggerPin, _echoPin);
      this->setDepth(_emptyDepthMm, _fullDepthMm);
      this->setMinDepthPercent(_minDepthPercent);
    }

    int getDepthPercent(){
      int depthMm = getDepthMm();

      if(depthMm >= 0 && depthMm <= MAX_DISTANCE){
        return constrain(map(depthMm, emptyDepthMm, fullDepthMm, 0, 100), 0, 100);
      }
      else {
        return -1;
      }
    }

    double getDepthMm(){
      double result = 0;

      for(byte i = 0; i < 3; i++) {
        digitalWrite(triggerPin, LOW);
        delayMicroseconds(2);
        digitalWrite(triggerPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(triggerPin, LOW);
        
        unsigned long durationMicroSec = pulseIn(echoPin, HIGH);
        double distanceMm = (durationMicroSec / 2.0 * 0.0343) * 10.0;
        
        if (distanceMm <= 0 || distanceMm > MAX_DISTANCE) {
          result = -1.0;
        }
        else {
          result = distanceMm;
        }
    
        result = sonicFilt.median(result);
        result = sonicFilt.expRunningAverage(result);
      }
      
      return result;
    }

    void setSensorPins(byte _triggerPin, byte _echoPin){
      triggerPin = _triggerPin;
      echoPin = _echoPin;

      pinMode(triggerPin, OUTPUT);
      pinMode(echoPin, INPUT);
    }

    void setEmptyDepthMm(unsigned int _emptyDepthMm){
      emptyDepthMm = _emptyDepthMm;
    }

    void setFullDepthMm(unsigned int _fullDepthMm){
      fullDepthMm = _fullDepthMm;
    }

    void setDepth(unsigned int _emptyDepthMm, unsigned int _fullDepthMm){
      emptyDepthMm = _emptyDepthMm;
      fullDepthMm = _fullDepthMm;
    }

    unsigned int getEmptyDepthMm(){
      return emptyDepthMm;
    }

    unsigned int getFullDepthMm(){
      return fullDepthMm;
    }

    void setMinDepthPercent(byte _minDepthPercent) {
      minDepthPercent = _minDepthPercent;
    }

    byte getMinDepthPercent() {
      return minDepthPercent;
    }
};


class SoilMoistureSensor{
  public:

  private:
    const int MIN_SOIL_MOISTURE = 16;
    const int MAX_SOIL_MOISTURE = 835;
    byte minSoilMoisturePercent;
    byte sensorPin;
    int sensorVccPin;
    int sensorDetectionPin;
    Filterer moistFilt = Filterer();
    
  public:
    SoilMoistureSensor(byte _sensorPin, int _sensorVccPin = -1, int _sensorDetectionPin = -1, byte _minSoilMoisturePercent = 25){
      sensorPin = _sensorPin;
      sensorVccPin = _sensorVccPin;

      if(sensorVccPin != -1){
        pinMode(sensorVccPin, OUTPUT);
        setVccPinState(LOW);
      }

      sensorDetectionPin = _sensorDetectionPin;

      this->setMinSoilMoisturePercent(_minSoilMoisturePercent);
    }

    int getSoilMoisturePercent(){
      if(!isSensorDetected()) {
        return -1;
      }

      int soilMoisture = getSoilMoisture();

      if(soilMoisture >= MIN_SOIL_MOISTURE && soilMoisture <= MAX_SOIL_MOISTURE){
        return map(soilMoisture, MIN_SOIL_MOISTURE, MAX_SOIL_MOISTURE, 0, 100);
      }
      else {
        return -1;
      } 
    }

    int getSoilMoisture(){
      if(!isSensorDetected()) {
        return -1;
      }

      int result = 0;
      
      setVccPinState(HIGH);

      for(byte i = 0; i < 3; i++) {
        result = analogRead(sensorPin);
    
        result = moistFilt.median(result);
        result = moistFilt.expRunningAverage(result);
      }
    
      setVccPinState(LOW);
      
      return result;
    }

    void setMinSoilMoisturePercent(byte _minSoilMoisturePercent) {
      minSoilMoisturePercent = _minSoilMoisturePercent;
    }

    byte getMinSoilMoisturePercent() {
      return minSoilMoisturePercent;
    }
    
  private:
    void setVccPinState(bool _state) {
      if(sensorVccPin != -1){
        digitalWrite(sensorVccPin, _state);
      }
    }

    bool isSensorDetected() {
      if(sensorDetectionPin != -1) {
        return !digitalRead(sensorDetectionPin);
      }
      else {
        return true;
      }
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
