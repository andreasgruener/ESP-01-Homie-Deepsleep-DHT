#include <Homie.h>
#include <DHT.h>


#define DHTPIN 2 // D5
#define DHTTYPE DHT22   // Es handelt sich um den DHT22 Sensor
#define FW_NAME "esp-01-deepsleep-dht22"
#define FW_VERSION "1.7"


/* Magic sequence for Autodetectable Binary Upload */
const char *__FLAGGED_FW_NAME = "\xbf\x84\xe4\x13\x54" FW_NAME "\x93\x44\x6b\xa7\x75";
const char *__FLAGGED_FW_VERSION = "\x6a\x3f\x3e\x0e\xe1" FW_VERSION "\xb0\x30\x48\xd4\x1a";
/* End of magic sequence for Autodetectable Binary Upload */

// config file to store config for reset / reboot
const char* UH27_CONFIG_FILENAME = "uh27/config.json";
const unsigned int JSON_CONFIG_MAX_BUFFER_SIZE = 1024;
const char* CONFIG_VERSION = "1.0";

// 5ccf7fa47b9e HomeTemperature3 # GRETA
// mosquitto_pub -h 10.0.2.2 -t "homie/5ccf7fa47b9e/light/on/set" -m "true"
// mosquitto_sub -h 10.0.2.2 -t "homie/5ccf7fa47b9e/humidity/percent"
// mosquitto_pub -h 10.0.2.2 -t "homie/5ccf7fa47b9e/temperature/delay/set" -m "300"
// mosquitto_pub -h 10.0.2.2 -t "homie/5ccf7fa47b9e/deepsleep/sleeptime/set" -m "5"
// mosquitto_sub -h 10.0.2.2 -t "homie/5ccf7fa47b9e/temperature/degrees"

// comment
const int PIN_RELAY = LED_BUILTIN;
const int TEMPERATURE_INTERVAL = 5;

int temperature_interval = TEMPERATURE_INTERVAL;
unsigned long lastTemperatureSent = 0;
unsigned long lastRetry = 0;
int maxRetries = 5;
int retries = 0;

// battery
double battery_voltage;

//deepsleep (in seconds)
//int deepsleeptime = 12;
/* DEEPSLEEP */
const int DEFAULT_DEEP_SLEEP_TIME = 5;
int deepsleeptime = DEFAULT_DEEP_SLEEP_TIME;
boolean deepSleepEnabled = true;


HomieNode lightNode("light", "switch");
HomieNode temperatureNode("temperature", "temperature");
HomieNode humidityNode("humidity", "humidity");
HomieNode batteryNode("battery", "battery");
HomieNode deepsleepNode("deepsleep", "deepsleep");
// Temperature
DHT dht(DHTPIN, DHTTYPE); //Der Sen 
// if you nee getVCC() turn this on
ADC_MODE(ADC_VCC);

void setupHandler() {

  lightNode.advertise("on").settable(lightOnHandler);
  lightNode.setProperty("on").send("false");
  
  temperatureNode.advertise("delay").settable(delayHandler);
  temperatureNode.setProperty("delay").send(String(temperature_interval));
 
  humidityNode.advertise("delay").settable(delayHandler);
  humidityNode.setProperty("delay").send(String(temperature_interval));

  deepsleepNode.advertise("sleeptime").settable(deepsleepHandler);
  deepsleepNode.setProperty("sleeptime").send(String(deepsleeptime));

  deepsleepNode.advertise("enabled").settable(deepsleepEnabledHandler);
  deepsleepNode.setProperty("enabled").send(String(deepSleepEnabled));

  temperatureNode.setProperty("unit").send("c");
  humidityNode.setProperty("unit").send("%");
  batteryNode.setProperty("unit").send("V");
  deepsleepNode.setProperty("unit").send("s");
}



bool delayHandler(const HomieRange& range, const String& value) { 
  
  Homie.getLogger() << "Got Delay " << value << endl;;
  int delay = value.toInt();
  temperature_interval = delay;
  temperatureNode.setProperty("delay").send(value);
  humidityNode.setProperty("delay").send(value);
  Homie.getLogger() << "Delayis " << delay << endl;

  return true;
}


bool lightOnHandler(const HomieRange& range, const String& value) {
  if (value != "true" && value != "false") return false;

  bool on = (value == "true");
  digitalWrite(PIN_RELAY, on ? LOW : HIGH);
  lightNode.setProperty("on").send(value);
  Homie.getLogger() << "Light is " << (on ? "on" : "off") << endl;

  return true;
}

/*
*
*/
bool deepsleepEnabledHandler(const HomieRange& range, const String& value) { 
  
  Homie.getLogger() << "MQTT Input :: Got Deepsleep Enabled " << value << endl;;
  if (value != "true" && value != "false") return false;

  bool enabled = (value == "true");
  deepSleepEnabled  = enabled;
  deepsleepNode.setProperty("enabled").send(value);

  Homie.getLogger() << "   Deepsleep Enabled value  " << deepSleepEnabled << endl;
  saveConfig();

  return true;
}

/*
NOt yet working - keeps forgetting...
*/
bool deepsleepHandler(const HomieRange& range, const String& value) { 
  
  Homie.getLogger() << "Got Deepsleep value (seconds) " << value << endl;;
  int delay = value.toInt();
  deepsleeptime  = delay;
  deepsleepNode.setProperty("sleeptime").send(value);

  Homie.getLogger() << "Deepsleep Delay Value  " << delay << endl;
  saveConfig();
  
  return true;
}

void onHomieEvent(const HomieEvent& event) {
  switch(event.type) {
    // case HomieEventType::MQTT_READY:
    //   Homie.getLogger() << "MQTT connected, preparing for deep sleep..." << endl;
    //   Homie.prepareToSleep();
    //   break;
    case HomieEventType::READY_TO_SLEEP:
      Homie.getLogger() << "Ready to sleep. Initiate Deepsleep for " << deepsleeptime << " seconds" << endl;
      ESP.deepSleep(deepsleeptime * 1000 * 1000); // (microseconds)
      break;
  }
}


void setup() {
  Serial.begin(115200);
  Serial.setTimeout(2000);
  // Wait for serial to initialize.
  while(!Serial) { }

  Serial << endl << endl;

  initFS();
  readConfig();

  //pinMode(PIN_RELAY, OUTPUT);
  //digitalWrite(PIN_RELAY, LOW);
 
  // uncomment for ESP-01
  Homie.disableLedFeedback(); // needed for deepsleep
//  Homie.setLedPin(10, HIGH);
  Homie.disableResetTrigger();
  Homie_setFirmware(FW_NAME, FW_VERSION);
  Homie_setBrand("mercatis-iot");
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
  
  Homie.onEvent(onHomieEvent);
  /*
  Homie.reset();
  Homie.setIdle(true);
  */
  Homie.setup();
  dht.begin(); //DHT11 Sensor starten
  // sensors.begin(); //OneWire/Dallas
  // configure for VC measurement

}

float getTemperature() {  
  float temperature = dht.readTemperature();
  return temperature;
}

float getHumidity() {
  float humidity = dht.readHumidity(); 
 return humidity;
}

float getBatteryVoltage() {
  return ESP.getVcc() / 1024.00f; 
}

/** Voltage Divider via analog Reas **/
float getBatteryVoltageAnalog() {
  float maxADCVoltage = 3.3;
  float reading = analogRead(A0);
  float percentage = reading / 1024;
  float vout = percentage * maxADCVoltage;
  float r1 = 330;
  float r2 = 100;

  float vin = vout / ( r2 / ( r1 + r2 ));

  Serial.print(reading);
  Serial.print("   vout = ");
  Serial.print(vout);
  Serial.print("  percentage = ");
  Serial.print(percentage);
  Serial.print("   vin = ");
  Serial.print(vin);
 // Serial.println(ESP.getVcc());

  Serial.println();
  // float voltage = ESP.getVcc() / 1000;
 return vin;
}


void checkDHT11() {
  
  if (  ( retries < maxRetries && millis() - lastRetry >= 3000UL ) ||
        millis() - lastTemperatureSent >= temperature_interval * 1000UL ) {

    float temperature = 0;
    float humidity = 0;
    float pressure = 0;
    lastTemperatureSent =  millis();

    // successful the lasttime, new game
    if ( retries > maxRetries ) {
      retries = 0;
    } else if ( retries == maxRetries ) {
      Homie.getLogger() << "MAx retries reached - Sensor not OK - giving up .." << endl;
      temperatureNode.setProperty("sensor_ok").send("0");
      retries++;
      return;
    }

    // try getting temperature -- sometimes there is no answer from the sensor
    temperature = getTemperature();
    //Homie.getLogger() << "Temperature reading " << temperature << endl;
    if ( isnan(temperature) || temperature == 0 ) {
      retries++;
      Homie.getLogger() << "*** Temperature not available retrying " << retries << ". time " << " max Retries = " << lastRetry << "... " << millis() << endl;
      lastRetry = millis();
      temperatureNode.setProperty("sensor_ok").send("0");
    }
    else {

      Homie.getLogger() << "Temperature: " << temperature << " °C" << endl;
      temperatureNode.setProperty("degrees").send(String(temperature));
      temperatureNode.setProperty("sensor_ok").send("1");
      lastTemperatureSent = millis();
      retries = maxRetries + 1;
      
      // assume if temperature works sensor is good :-)
      humidity = getHumidity();
      Homie.getLogger() << "Humidity: " << humidity << " %" << endl;
      humidityNode.setProperty("percent").send(String(humidity));
    }
    checkBattery();
  }
}


void checkDHT11DeepSleep() {
  
  if (  millis() - lastRetry >= 3000UL && millis() - lastTemperatureSent >= temperature_interval * 1000UL) {

    float temperature = 0;
    float humidity = 0;
    float pressure = 0;
    // lastTemperatureSent =  millis();

    // successful the lasttime, new game
    if ( retries == maxRetries ) {
      Homie.getLogger() << "Max retries reached  giving up .." << endl;
      //temperatureNode.setProperty("sensor_ok").send("0");
      retries++;
      deepsleepNode.setProperty("seconds").send(String(deepsleeptime));
      // uncomment ESP-01
      if ( deepSleepEnabled ) {
        Homie.getLogger() << "Deepsleep is enabled" << endl;
        Homie.prepareToSleep();
      }
      else {
        Homie.getLogger() << "Deepsleep is NOT enabled" << endl;
        retries = 0;
        lastRetry = millis() + temperature_interval * 1000UL;  // wait for delay
      }
      return;
    } else if ( retries > maxRetries ) {
      Homie.getLogger() << "Wait 15 seconds for next retry" << endl;
      lastRetry = millis() + 15000UL;  // wait 15 seconds
      return;
    }

    // try getting temperature -- 
    
    // sometimes there is no answer from the sensor
    temperature = getTemperature();
    //Homie.getLogger() << "Temperature reading " << temperature << endl;
    if ( isnan(temperature) || temperature == 0 ) {
      retries++;
      Homie.getLogger() << "*** Temperature not available retrying " << retries << ". time " << " max Retries = " << lastRetry << "... " << millis() << endl;
      lastRetry = millis();
      temperatureNode.setProperty("sensor_ok").send("0");
    }
    else {

      Homie.getLogger() << "Temperature: " << temperature << " °C" << endl;
      temperatureNode.setProperty("degrees").send(String(temperature));
      temperatureNode.setProperty("sensor_ok").send("1");
      lastTemperatureSent = millis();
      retries = maxRetries + 1;
      
      // assume if temperature works sensor is good :-)
      humidity = getHumidity();
      Homie.getLogger() << "Humidity: " << humidity << " %" << endl;
      humidityNode.setProperty("percent").send(String(humidity));
      deepsleepNode.setProperty("seconds").send(String(deepsleeptime));
      batteryNode.setProperty("voltage").send(String(getBatteryVoltage()));
      Homie.getLogger() << "Prepare Deepsleep for " << deepsleeptime << " seconds " << endl;
      // uncomment ESP-01
      if ( deepSleepEnabled ) {
        Homie.getLogger() << "Deepsleep is enabled" << endl;
        Homie.prepareToSleep();
      }
      else {
        Homie.getLogger() << "Deepsleep is NOT enabled" << endl;
        retries = 0;
        lastRetry = millis() + temperature_interval * 1000UL;  // wait for delay
      }
    }
  }
}

void checkBattery() {
  float batteryVoltage = getBatteryVoltage();
  Homie.getLogger() << "Battery: " << batteryVoltage << "V";
  batteryNode.setProperty("voltage").send(String(batteryVoltage));
}


//float getTemperature() {
 // sensors.requestTemperatures(); // Send the command to get temperature readings 
// return sensors.getTempCByIndex(0);
//}

void loopHandler() {
  checkDHT11DeepSleep();
}

void loop() {

  Homie.loop();
}





/**************************************************************************/
/* FILESYSTEM */
/**************************************************************************/


/**
 * 
 */
void initFS() {
  bool result = SPIFFS.begin();
  if ( !result ) {
      //Serial.println("COULD NO Mount Filesystem");
  }
  else {
      //Serial.println("Mounted successfully.");
  }
}


/**
* 
*/
void saveConfig() {
  // open the file in write mode
  File f = SPIFFS.open(UH27_CONFIG_FILENAME, "w");
  if (!f) {
    //Serial.println("file creation failed");
  }
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["version"] = CONFIG_VERSION;
  root["sleeptime"] = deepsleeptime;
  root["deepSleepEnabled"] = deepSleepEnabled;
  String output;
  root.printTo(output);
  // now write two lines in key/value style with  end-of-line characters
  // char json[] =  "{\"sleeptime\":\"33\",\"version\":\"1.0\"}";
  f.println(output);
  f.close();
  Serial.println("Updated UH27 Config.");
}

/**
* 
*/
bool readConfig() {
  File configFile = SPIFFS.open(UH27_CONFIG_FILENAME, "r");

  if (!configFile) {
    //Serial.println("✖ Cannot open config file");
    return false;
  }

  size_t configSize = configFile.size();
  
  std::unique_ptr<char[]> buf(new char[configSize]);
  configFile.readBytes(buf.get(), configSize);
  
  StaticJsonBuffer<JSON_CONFIG_MAX_BUFFER_SIZE> jsonBuffer;
  JsonObject& parsed_json = jsonBuffer.parseObject(buf.get());
  if (!parsed_json.success()) {
    //Serial.println("✖ Invalid or too big config file");
    return false;
  }

  int delay = parsed_json["sleeptime"]; 
  deepsleeptime = delay;
  // Serial.print("sleeptime : "); 
  // Serial.println(delay);


  int deepSleepOn = parsed_json["deepSleepEnabled"]; 
  deepSleepEnabled = deepSleepOn;
  // Serial.print("Deepsleep On : "); 
  // Serial.println(deepSleepEnabled);

  const char* version = parsed_json["version"]; 
  // Serial.print("Version : ");
  Serial.println("Read UH27 Config");
}

