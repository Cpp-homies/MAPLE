

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Button2.h" //  https://github.com/LennartHennigs
#include "ESPRotary.h" //  https://github.com/LennartHennigs
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>
// File System Library
#include <FS.h>
// SPI Flash Syetem Library
#include <SPIFFS.h>
// WiFiManager Library
#include <WiFiManager.h>
#include "SD.h"
#include "SPI.h"
#include <HardwareSerial.h>

//OLED
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
//Wifi config
#define ESP_DRD_USE_SPIFFS true
#define JSON_CONFIG_FILE "/config_maple_user.json"
//rotary
#define CLICKS_PER_STEP   4
//GPIO pins
#define ROTARY_PIN1	0
#define ROTARY_PIN2	2
#define BUTTON_PIN	15
#define LIGHT_SENSOR 34
#define SOIL_SENSOR 35
#define DIR 27
#define STEP 14
#define LIGHTS 33
#define FAN 32
#define PUMP_POWER 12
#define RX_PORT2 16
#define TX_PORT2 17

//PWM variables
int freq = 2000;
int pwmResolution = 8;
int pwmChannel_0 = 0;
int pwmChannel_2 = 2;

uint16_t analogWet = 1500;
uint16_t analogDry = 3000;
uint8_t brightness;
int pumpSpeed = 1500;
int fanSpeed = 240; //190-255
bool fanOn = false;
uint8_t screenMode = 0; //to define what to show on screen
const char *dataFile = "/mapleData.csv"; //data logging file path
hw_timer_t *timer = NULL; //rotary timer
//data sending interval
unsigned long previousMillis = 0;
const long interval = 30000; 
String BASE_URL = "https://cloud.kovanen.io/sensordata/";
// Variables to hold data from custom textboxes
char usernameString[50] = "Username";
char passwordString[50] = "Password";
// Flag for saving WiFi config data
bool shouldSaveConfig = false;


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BME280 bme;
HardwareSerial SerialPort(2); //UART2
ESPRotary r;
Button2 b;
WiFiManager wm;


//Rottary handler
void IRAM_ATTR handleLoop() {
  r.loop();
  b.loop();
}

void setup() {
  //Serial for PC connection
  Serial.begin(115200);
  while(!Serial){}
  //Serial for ESP CAM communitation
  SerialPort.begin(115200, SERIAL_8N1, RX_PORT2, TX_PORT2);
  while (!SerialPort){}

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    //for(;;);
  }
  Serial.println("Display connected.");
  bool status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    //while (1);
  }
  Serial.println("BME280 connected.");
  analogReadResolution(12);

  pinMode(LIGHTS,OUTPUT);
  pinMode(FAN,OUTPUT);
  pinMode(PUMP_POWER,OUTPUT);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);

  digitalWrite(PUMP_POWER, LOW);

  // configure PWM functionalitites
  ledcSetup(pwmChannel_0, freq, pwmResolution);
  ledcSetup(pwmChannel_2, freq, pwmResolution);
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(LIGHTS, pwmChannel_0);
  ledcAttachPin(FAN, pwmChannel_2);
  
  display.setTextColor(WHITE);
  displayBoot();

  configWifi();
  
  initSDcard();
  
  initRotary();
  
}

/////////////////////////////////////////////////////////////////
//Rotary methods

void initRotary(){
  r.begin(ROTARY_PIN1, ROTARY_PIN2, CLICKS_PER_STEP);
  r.setRightRotationHandler(screenModeSoil);
  r.setLeftRotationHandler(screenModeAir); 
  b.begin(BUTTON_PIN);
  b.setTapHandler(click);
  b.setLongClickHandler(resetPosition);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &handleLoop, true);
  timerAlarmWrite(timer, 3000, true); // every 0.3 seconds
  timerAlarmEnable(timer);
}

//when turned left
void screenModeAir(ESPRotary& r){
  screenMode = 0;
  Serial.println(r.directionToString(r.getDirection()));

}

//when turned right
void screenModeSoil(ESPRotary& r){
  screenMode = 1;
  Serial.println(r.directionToString(r.getDirection()));
}

// single click
void click(Button2& btn) {
  Serial.println("Click!");
}

// long click
void resetPosition(Button2& btn) {
  r.resetPosition();
  Serial.println("Reset!");
}
/////////////////////////////////////////////////////////////////
//SDcard methods

void logData(){
  File file = SD.open(dataFile);
  if(!file){
    file = SD.open(dataFile, FILE_WRITE);
        if(!file){
          Serial.println("Failed to open file for data logging");
          return;
        } else {
          file.println("AirTemp,AirHumidity,SoilHumidity");
          
        }
  }
  file.close();
  file = SD.open(dataFile, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for data logging");
    }
  String toWrite = String(bme.readTemperature())+","+ String(bme.readHumidity())+","+ String(analogRead(SOIL_SENSOR));
  char buffer[toWrite.length()+1];
  toWrite.toCharArray(buffer, toWrite.length()+1);
  file.println(buffer);
  file.close();
}

void initSDcard(){
  if(!SD.begin()){
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    
}
/////////////////////////////////////////////////////////////////
//WiFi manager methods
// Save Config in JSON format
void saveConfigFile(){
  Serial.println(F("Saving configuration..."));
  
  // Create a JSON document
  StaticJsonDocument<512> json;
  json["usernameString"] = usernameString;
  json["passwordString"] = passwordString;
 
  // Open config file
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    // Error, file did not open
    Serial.println("failed to open config file for writing");
  }
 
  // Serialize JSON data to write to file
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    // Error writing file
    Serial.println(F("Failed to write to file"));
  }
  // Close file
  configFile.close();
}
// Load existing configuration file
bool loadConfigFile(){
  // Uncomment if we need to format filesystem
  // SPIFFS.format();
 
  // Read configuration from FS json
  //Serial.println("Mounting File System...");
 
  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    //Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      // The file exists, reading and loading
      //Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        //Serial.println("Opened configuration file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        //serializeJsonPretty(json, Serial);
        if (!error)
        {
          //Serial.println("Parsing JSON");
 
          strcpy(usernameString, json["usernameString"]);
          strcpy(passwordString, json["passwordString"]);
          //Serial.println("Finished parsing JSON");          
          return true;
        }
        else
        {
          // Error loading JSON data
          Serial.println("Failed to load json config");
        }
      }
    }
  }
  else
  {
    // Error mounting file system
    Serial.println("Failed to mount FS");
  }
 
  return false;
}
// Callback notifying us of the need to save configuration
void saveConfigCallback(){
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
// Called when config mode launched 
void configModeCallback(WiFiManager *myWiFiManager){
  Serial.println("Entered Configuration Mode");
 
  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
 
  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void configWifi() {
  // Change to true when testing to force configuration every time we run
  bool forceConfig = false;
  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup)
  {
    Serial.println("Forcing config mode as there is no saved config");
    forceConfig = true;
  }
  // Explicitly set WiFi mode
  WiFi.mode(WIFI_STA);
 
  //Reset settings (only for development)
  //wm.resetSettings();
 
  // Set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);
 
  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);
 
  // Custom elements
 
  // Text box (String) - 50 characters maximum
  WiFiManagerParameter custom_text_box_username("key_username", "Enter your M.A.P.L.E. username", usernameString, 50);
    
  // Text box (String) - 50 characters maximum
  WiFiManagerParameter custom_text_box_password("key_password", "Enter your M.A.P.L.E. password here", passwordString, 50); 
 
  // Add all defined parameters
  wm.addParameter(&custom_text_box_username);
  wm.addParameter(&custom_text_box_password);
 
  if (forceConfig)
    // Run if we need a configuration
  {
    if (!wm.startConfigPortal("M.A.P.L.E.", "homeworkhomies"))
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }
  }
  else
  {
    if (!wm.autoConnect("M.A.P.L.E.", "homeworkhomies"))
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }
 
  // If we get here, we are connected to the WiFi
 
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
 
  // Lets deal with the user config values
 
  // Copy the string value
  strncpy(usernameString, custom_text_box_username.getValue(), sizeof(usernameString));
  Serial.print("usernameString: ");
  Serial.println(usernameString);
 
  //Convert the number value
  strncpy(passwordString, custom_text_box_password.getValue(), sizeof(passwordString));
  Serial.print("passwordString: ");
  Serial.println(passwordString);
  
  delay(1000);
  SerialPort.println(0);
  delay(1000);
  SerialPort.println(wm.getWiFiSSID());
  delay(1000);
  String pass = wm.getWiFiPass();
  Serial.print("Wifi password:");
  Serial.println(pass);
  if(pass==""){
    SerialPort.println(" ");
  }else{
    SerialPort.println(pass);    
  }
  
 
  // Save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveConfigFile();
  }

  configTime(0, 0, "pool.ntp.org");
}
/////////////////////////////////////////////////////////////////
//display methods

void displaySoilData(){
  display.clearDisplay();
  // display temperature
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Soil humidity:");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(convertToSoilHumidity(analogRead(SOIL_SENSOR)));
  display.print(" %");
  
  // display humidity
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Last watering: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print("Some time");
   
  
  display.display();  
}

void displayTempHumidity(){
  display.clearDisplay();
  // display temperature
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(String(bme.readTemperature()));
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(2);
  display.print("C");
  
  // display humidity
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Humidity: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(String(bme.readHumidity()));
  display.print(" %"); 
  
  display.display();
}

void displayLightIntensity(){
  display.clearDisplay();
  // display temperature
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Light intensity: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  Serial.println(analogRead(LIGHT_SENSOR));
  display.print(String(analogRead(LIGHT_SENSOR)));
  display.display();
}

void displayBoot(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(5, 25);
  display.print("M.A.P.L.E.");
  display.display();
}
/////////////////////////////////////////////////////////////////
//self-explanatory

void sendData(float temp, float airHumid) {
  // Create a JSON payload with the temperature and humidity values
    StaticJsonDocument<128> jsonPayload;
    jsonPayload["temp"] = temp;
    jsonPayload["humid"] = airHumid;

    // Serialize the JSON payload to a string
    String payloadString;
    serializeJson(jsonPayload, payloadString);

    String address = BASE_URL;
    char timeStr[30];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    strftime(timeStr, sizeof(timeStr), "%Y_%m_%d_%H_%M_%S", timeinfo);
    
    String timeStr_String = String(timeStr);
    address = address + timeStr_String;
    HTTPClient http;
    http.begin(address);

    // Set the content type header to JSON
    http.addHeader("Content-Type", "application/json");

    // Send the PUT request with the JSON payload
    int httpResponseCode = http.PUT(payloadString);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Sent request:");
      Serial.println(response);
    } else {
      String response = http.getString();
      Serial.print("Error sending request: ");
      Serial.println(httpResponseCode);
    }
    http.end();
}

void adjustLights(){
  /*
  if (analogRead(LIGHT_SENSOR)>3000){
    digitalWrite(LIGHTS,LOW);
  } 
  else {
    digitalWrite(LIGHTS, HIGH);
  }*/
  int lightIntensity = analogRead(LIGHT_SENSOR);
  //Serial.println(lightIntensity);
  if(lightIntensity<=3600){
    brightness = map(lightIntensity, 3600, 0, 0, 255);
  } else {
    brightness = 0;
  }
  
  ledcWrite(pwmChannel_0, brightness);
}

bool needsWater(){
  int dry = 3000;
  int wet = 1600;
  if(analogRead(SOIL_SENSOR)>2500){
    return true;
  } else return false;
}

void pumpWater(){
  digitalWrite(PUMP_POWER,HIGH);
  digitalWrite(DIR, HIGH);
  while(needsWater()){
    //displayTempHumidity();
    //adjustLights();
    digitalWrite(STEP, HIGH);
    delayMicroseconds(pumpSpeed);
    digitalWrite(STEP, LOW);
    delayMicroseconds(pumpSpeed);
  }
  digitalWrite(PUMP_POWER,LOW);
}


void checkAirHumidity(){
  
  if(bme.readHumidity()>50.0){
    if(!fanOn){
      ledcWrite(pwmChannel_2, fanSpeed);
      Serial.println("FAN ON");
      fanOn = true;      
    } 
  } else {
    if(fanOn) {
      ledcWrite(pwmChannel_2, 0);
      Serial.println("FAN OFF");
      fanOn = false;
    }
  }
}

float convertToSoilHumidity(uint16_t analogInput){ 
  return (3050.0-float(analogInput))/15.0;
}

void loop() {
  
  
  if (screenMode == 0){
    displayTempHumidity();
  } else {
    displaySoilData();
  }
  
  adjustLights();
  unsigned long currentMillis = millis();
  if (needsWater()){
    //pumpWater();
  }
  if (currentMillis - previousMillis >= 5000) {
    previousMillis = currentMillis;
    logData();   
  }
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    //sendData(bme.readTemperature(), bme.readHumidity());    
  }
  checkAirHumidity();
  
}
