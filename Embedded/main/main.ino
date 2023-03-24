/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-i2c-communication-arduino-ide/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

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



#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Adafruit_BME280 bme;

int LIGHT_SENSOR = 34;
int SOIL_SENSOR = 35;
const int DIR = 12;
const int STEP = 14;
int LIGHTS = 23;
int PUMP_POWER = 27;
int freq = 2000;
int pwmResolution = 8;
int pwmChannel_0 = 0;
uint8_t brightness;
int pumpSpeed = 1500;

unsigned long previousMillis = 0;
const long interval = 10000;
String BASE_URL = "https://cloud.kovanen.io/sensordata/";


#define ESP_DRD_USE_SPIFFS true
// JSON configuration file
#define JSON_CONFIG_FILE "/config.json"
 
// Flag for saving config data
bool shouldSaveConfig = false;
 
// Variables to hold data from custom textboxes
char usernameString[50] = "Username";
char passwordString[50] = "Password";
 
// Define WiFiManager Object
WiFiManager wm;


void setup() {
  Serial.begin(115200);
  delay(10);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  bool status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  
  analogReadResolution(12);

  pinMode(LIGHTS,OUTPUT);
  pinMode(PUMP_POWER,OUTPUT);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);

  digitalWrite(PUMP_POWER, LOW);

  // configure LED PWM functionalitites
  ledcSetup(pwmChannel_0, freq, pwmResolution);
  
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(LIGHTS, pwmChannel_0);

  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);

  //connectWifi("Stanley's toilet", "81053851414");

  // Change to true when testing to force configuration every time we run
  bool forceConfig = false;
  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup)
  {
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }
  // Explicitly set WiFi mode
  WiFi.mode(WIFI_STA);
 
  // Reset settings (only for development)
  wm.resetSettings();
 
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
    if (!wm.startConfigPortal("M.A.P.L.E.", "homies"))
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
    if (!wm.autoConnect("M.A.P.L.E.", "homies"))
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
 
 
  // Save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveConfigFile();
  }

  configTime(0, 0, "pool.ntp.org");
}

// Save Config in JSON format
void saveConfigFile(){
  Serial.println(F("Saving configuration..."));
  
  // Create a JSON document
  StaticJsonDocument<512> json;
  json["usernameString"] = usernameString;
  json["passwordNumber"] = passwordString;
 
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
  Serial.println("Mounting File System...");
 
  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      // The file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        Serial.println("Opened configuration file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error)
        {
          Serial.println("Parsing JSON");
 
          strcpy(usernameString, json["usernameString"]);
          strcpy(passwordString, json["passwordString"]);
 
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

int connectWifi(String BSSID, String password) {
  WiFi.begin(BSSID.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  return 1;
}

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
    displayTempHumidity();
    adjustLights();
    digitalWrite(STEP, HIGH);
    delayMicroseconds(pumpSpeed);
    digitalWrite(STEP, LOW);
    delayMicroseconds(pumpSpeed);
  }
  digitalWrite(PUMP_POWER,LOW);
}

void loop() {
  
  displayTempHumidity();
  adjustLights();
  unsigned long currentMillis = millis();
  if (needsWater()){
    pumpWater();
  }
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sendData(bme.readTemperature(), bme.readHumidity());
    
  }
  
}
