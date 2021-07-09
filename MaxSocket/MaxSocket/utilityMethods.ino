byte outputSwitch(int id){
  if(outputs[id].isOn()) outputOff(id);
  else outputOn(id);
}

byte outputOn(int id) {
  Serial.print("[output " + (String)id + "]: ");
  outputs[id].on();
  writeToFlash("/outputs_states.txt", outputsStatesToJson());
  if(mqttClient.connected()) mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());

  return outputs[id].getStatus();
}

byte outputOff(int id) {
  Serial.print("[output " + (String)id + "]: ");
  outputs[id].off();
  writeToFlash("/outputs_states.txt", outputsStatesToJson());
  if(mqttClient.connected()) mqttClient.publish(topicDeviceState.c_str(), 0, true, getJsonStatus().c_str());
  
  return outputs[id].getStatus();
}

bool isStrDigit(String str){
  for (int i = 0; i < str.length(); i++) 
    if(!isdigit(str[i])) return false;

  return true;
}

void tickerRun(){
  if(DEBUG) Serial.println("[ticker run]");
  
  if(ticker.mode == ticker.MODE_ON){
     outputOn(ticker.output);
  }
  else {
    outputOff(ticker.output);      
  }
  ticker.off();
}
