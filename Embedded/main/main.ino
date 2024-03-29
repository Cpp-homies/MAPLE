#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Button2.h" //  https://github.com/LennartHennigs
#include "ESPRotary.h" //  https://github.com/LennartHennigs
#include <WiFi.h>
#include "mbedtls/md.h"
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
#include <AccelStepper.h>

//OLED
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
//Wifi config
#define ESP_DRD_USE_SPIFFS true
#define JSON_CONFIG_FILE "/config_maple_user.json"
//rotary
#define CLICKS_PER_STEP   4
#define MIN_POS 0
#define MAX_POS 4
#define INCREMENT 1
//pump motor
#define motorInterfaceType 1
//GPIO pins
#define ROTARY_PIN1	33
#define ROTARY_PIN2	25
#define BUTTON_PIN	26
#define LIGHT_SENSOR 34
#define SOIL_SENSOR 35
//#define DIR 27
//#define STEP 14
#define LIGHTS 32
#define FAN 13
#define PUMP_POWER 12
#define RX_PORT2 2
#define TX_PORT2 4
#define I2C_SDA 16
#define I2C_SCL 17

const int STEP = 14;
const int DIR = 27;

//PWM variables
int freq = 2000;
int pwmResolution = 8;
int pwmChannel_0 = 0;
int pwmChannel_2 = 2;

int lightCheckPreviousMillis = 0;
int lightCheckInterval = 10000; 
int pumpCheckPreviousMillis = 0;
int pumpCheckInterval = 10000;
int pumpInterval = 10000; 
uint16_t analogWet = 1420;
uint16_t analogDry = 2950;
uint16_t lightCutOff = 90;
uint16_t lightMax = 10;
uint16_t pumpTriggerPercent = 30;
int pumpStopPercent = 85;
bool watering = false;
String pumpingTime = "N/A";
uint8_t brightness;
int lightPercentage = 100;
String lightStartTime = "08:00";
String lightStopTime = "22:00";
int pumpSpeed = 800;
int fanSpeed = 240; //190-255
int fanThreshold = 50;
bool fanOn = false;
uint8_t screenMode = 0; //to define what to show on screen
bool executeMenu = false;
bool inWifiConfig = false;
bool offlineMode = true;
uint8_t menuPosition = 0;
bool reset = false;
const char *dataFile = "/mapleData.csv"; //data logging file path
hw_timer_t *timer = NULL; //rotary timer
uint32_t I2Cfreq = 100000;


TwoWire I2C = TwoWire(0);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C, -1);
Adafruit_BME280 bme;
HardwareSerial SerialPort(2); //UART2
ESPRotary r;
Button2 b;
AccelStepper stepper(motorInterfaceType, STEP, DIR);

String myTimezone ="EET-2EEST,M3.5.0/3,M10.5.0/4";

unsigned long previousMillis = 0;
const long interval = 30000;
String BASE_URL = "https://cloud.kovanen.io/";
// String BASE_URL = "https://mapleplantapi.azurewebsites.net/";
// String BASE_URL = "http://10.0.0.6:8001/";
 
// Flag for saving config data
bool shouldSaveConfig = false;
 
// Variables to hold data from custom textboxes
char usernameString[50] = "Username";
char passwordString[50] = "Password";

// The hashedString and password
String hashedUsername;
String hashedPassword;

// The session cookies for one time authentication
// Extract the session cookie from the response headers
String sessionCookie;
 
// Define WiFiManager Object
WiFiManager wm;

/////////////////////////////////////////////
// Device-Cloud communication methods
TaskHandle_t listenerHandle;

// The HTTPClient object used for communication
// HTTPClient http;

// Function that get incoming request from the cloud
// return the HTTP response code, and change the JSON document requestArgs to contain any input arguments for the request (if any)
int getNewRequest(DynamicJsonDocument *requestArgs) {
    // Create the address
    String address = BASE_URL + "request-to-esp/" + hashedUsername + "/" + hashedPassword;
    HTTPClient http;
    http.begin(address);
    Serial.println("Sending request to " + address);
    // Send HTTP GET request to the cloud to check for incoming request
    // http.setCookie("session", session["auth_user"]);
    http.addHeader("session", sessionCookie);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        // Parse the response JSON
        String response = http.getString();
        DeserializationError error = deserializeJson(*requestArgs, response);
        if (error) {
            Serial.println("Failed to parse JSON response");
            return -1;
        }
        http.end();
        return httpResponseCode;
    } else {
        http.end();
        return httpResponseCode;
    }
}

// function for updating the control parameter on the cloud server
void updateCloudControlParameters(int lightPercentage, int fanThreshold, int pumpTriggerPercent, String lightStartTime, String lightStopTime) {
  // Create a JSON payload with the control parameters
  StaticJsonDocument<512> jsonPayload;
  jsonPayload["lightIntensity"] = lightPercentage;
  jsonPayload["fanThreshold"] = fanThreshold;
  jsonPayload["pumpThreshold"] = pumpTriggerPercent;
  jsonPayload["lightStartTime"] = lightStartTime;
  jsonPayload["lightStopTime"] = lightStopTime;
  jsonPayload["user_id"] = hashedUsername;
  jsonPayload["password"] = hashedPassword;

  // Serialize the JSON payload to a string
  String payloadString;
  serializeJson(jsonPayload, payloadString);

  String address = BASE_URL + "/esp-control-pars-update";
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

// a mini server that check for new data requests from the cloud
void Listener( void * pvParameters ){
  // The setup code for the listener
  // login to the cloud
  int loginStatus = cloudLogin(hashedUsername, hashedPassword);

  if (loginStatus == 1) {
    Serial.println("Cloud authentication completed for core 0");
  } 
  else {
    Serial.println("Cloud authentication failed for core 0");
  }

  unsigned long listenerPreviousMillis = 0;
  const long controlParameters_updateInterval = 5000; // 5s
  unsigned long controlParUpdatePreviousMillis = 0;
  // const long listeningInterval = 5000;
  // an infinite loop equivalent to the void loop() function on the main core
  for(;;){
    unsigned long currentMillis = millis();

    // update (sync) the control parameters on the server side periodically
    if (currentMillis - controlParUpdatePreviousMillis >= controlParameters_updateInterval) {
      controlParUpdatePreviousMillis = currentMillis;
      updateCloudControlParameters(lightPercentage, fanThreshold, pumpTriggerPercent, lightStartTime, lightStopTime);
    }

    // if (currentMillis - listenerPreviousMillis >= listeningInterval) {
      // listenerPreviousMillis = currentMillis;
      // JSON document for keeping request arguments
      // int *requestArgs = (int*) malloc(10 * sizeof(int));
      const size_t capacity = JSON_OBJECT_SIZE(1) + 256;
      DynamicJsonDocument requestArgs(capacity);
    
      int httpResponseCode = getNewRequest(&requestArgs);

      if (httpResponseCode == -1) {
        Serial.print("Error sending request: ");
        Serial.println(httpResponseCode);
      }
      else {
        // get the request type
        int requestType = requestArgs["type"];
        // the value that go with the request (if applicable)
        int requestValue;
        char requestStringValue[5];

        

        switch (requestType) {
          // No request
          case 0:
            Serial.println("No new request");
            break;

          // Live data
          case 1: {
            Serial.println("Received live data request from cloud server");

            /*************************
            *
            * NOTE!! To be replace with real temp and humid, the random numbers are just for testing
            *
            */
            float randTemp = bme.readTemperature();
            float randAirHumid = bme.readHumidity();
            float randSoilHumid = convertToSoilHumidity(analogRead(SOIL_SENSOR));
            /*
            ****************************/
            
            sendLiveData(randTemp, randAirHumid, randSoilHumid);
            
            break;
          }
          // SET Pump threshold
          case 2: {
            // get the set value
            requestValue = requestArgs["value"];
            Serial.print("Received pump threshold SET request from server, with threshold ");
            Serial.println(requestValue);

            
            // Setting the threshold
            pumpTriggerPercent = requestValue;
            
            break;
          }
          // SET Fan threshold
          case 3: {
            // get the set value
            requestValue = requestArgs["value"];
            Serial.print("Received fan threshold SET request from server, with threshold ");
            Serial.println(requestValue);

            
            // Setting the threshold
            fanThreshold = requestValue;
            
            break;
          }
          // SET Light intensity
          case 4: {
            // get the set value
            requestValue = requestArgs["value"];
            Serial.print("Received light intensity SET request from server, with level ");
            Serial.println(requestValue);

            
            // Setting the threshold
            lightPercentage = requestValue;
            
            break;
          }
          // SET light start time
          case 5: {
            // get the set value
            strncpy(requestStringValue,requestArgs["value"],sizeof(requestStringValue)) ;
            Serial.print("Received light start time SET request from server, with time ");
            Serial.println(requestStringValue);

            
            // Setting the threshold
            lightStartTime = String(requestStringValue);
            
            break;
          }
          // SET light stop time
          case 6: {
            // get the set value
            strncpy(requestStringValue,requestArgs["value"],sizeof(requestStringValue)) ;
            Serial.print("Received light stop time SET request from server, with time ");
            Serial.println(requestStringValue);

            
            // Setting the threshold
            lightStopTime = String(requestStringValue);
            
            break;
          }
          default:
          //
            Serial.print("Received unknown request from cloud server with type: ");
            Serial.println(requestType);
            break;
        }
      }

      // free(requestArgs);
    // }
    // delay to not flood the whole Serial monitor and make it easier to debug
    delay(2000);
  } 
}


// send data with a timestamp of the current time to the server,
// the data is sent to esp-req/sensordata/<timestamp> resource on the cloud server
void sendData(float temp, float airHumid, float soilHumid) {
  // Create a JSON payload with the temperature and humidity values
    StaticJsonDocument<512> jsonPayload;
    jsonPayload["temp"] = temp;
    jsonPayload["air_humid"] = airHumid;
    jsonPayload["soil_humid"] = soilHumid;
    jsonPayload["user_id"] = hashedUsername;
    jsonPayload["password"] = hashedPassword;

    // Serialize the JSON payload to a string
    String payloadString;
    serializeJson(jsonPayload, payloadString);

    String address = BASE_URL + "esp-req/sensordata/";
    char timeStr[20];
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

// function for sending live data to /sensordata/live/ resource on cloud server
void sendLiveData(float temp, float airHumid, float soilHumid) {
  // Create a JSON payload with the temperature and humidity values
    StaticJsonDocument<512> jsonPayload;
    jsonPayload["temp"] = temp;
    jsonPayload["air_humid"] = airHumid;
    jsonPayload["soil_humid"] = soilHumid;
    jsonPayload["user_id"] = hashedUsername;
    jsonPayload["password"] = hashedPassword;

    char timeStr[20];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    strftime(timeStr, sizeof(timeStr), "%Y_%m_%d_%H_%M_%S", timeinfo);
    String timeStr_String = String(timeStr);
    jsonPayload["time"] = timeStr;

    // Serialize the JSON payload to a string
    String payloadString;
    serializeJson(jsonPayload, payloadString);

    // make the address
    String address = BASE_URL + "esp-req/sensordata/live";
    
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

// Function to hash a given string using SHA-256
 String hashString(const char *str) {
  byte hashed_bytes[32];
  
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  const size_t str_length = strlen(str);
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) str, str_length);
  mbedtls_md_finish(&ctx, hashed_bytes);
  mbedtls_md_free(&ctx);
  
  String hashed_str = "";

  for(int i= 0; i< sizeof(hashed_bytes); i++)
  {
    char temp[3];
    sprintf(temp, "%02x", (int)hashed_bytes[i]);
    
    // Serial.print(str);
    
    // Append the temporary string to the hashed_str result
    hashed_str += temp;
  }

  return hashed_str;
}

// Send a (hashed) pair of username and password to the cloud
// Return the status code: 0 == fail, 1 == succeed
int cloudLogin(String hashedUsername, String hashedPassword) {
  // hash the username and password
  // String hashedUsername = hashString(username);
  // String hashedPassword = hashString(password);

  // Create a JSON payload with the username and password
  // StaticJsonDocument<128> jsonPayload;
  // jsonPayload["user_id"] = hashedUsername;
  // jsonPayload["password"] = hashedPassword;

  // Serialize the JSON payload to a string
  // String payloadString;
  // serializeJson(jsonPayload, payloadString);
  
  // Create the address
  String address = BASE_URL + "auth/login?user_id=" + hashedUsername + "&password=" + hashedPassword;
  HTTPClient http;
  http.begin(address);
  Serial.println("Sending request to " + address);
  // Send HTTP GET request to the cloud to check for incoming request
  int httpResponseCode = http.GET();

  const size_t capacity = JSON_OBJECT_SIZE(1) + 20;
  DynamicJsonDocument requestArgs(capacity);
  if (httpResponseCode > 0) {
      // Extract the session cookie from the response headers
      sessionCookie = http.header("Set-Cookie");

      // Parse the response JSON
      String response = http.getString();
      DeserializationError error = deserializeJson(requestArgs, response);
      if (error) {
          Serial.println("Failed to parse JSON response");
          http.end();
          return -1;
      }
      http.end();
      int loginStatus = requestArgs["status"];
      return loginStatus;
  } else {
      http.end();
      return -1;
  }
}
////////////////////////////////////////////



void setup() {

  // http.setReuse(true);// reuse for all connection in order to authenticate one time only

  //Serial for PC connection
  Serial.begin(115200);
  while(!Serial){}
  //Serial for ESP CAM communitation
  SerialPort.begin(115200, SERIAL_8N1, RX_PORT2, TX_PORT2);
  while (!SerialPort){}

  //Changing I2C pins
  I2C.begin(I2C_SDA, I2C_SCL, I2Cfreq);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    //for(;;);
  }
  Serial.println("Display connected.");
  displayMaple();

  bool status = bme.begin(0x76,&I2C);  
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
  ledcAttachPin(LIGHTS, pwmChannel_0);
  ledcAttachPin(FAN, pwmChannel_2);

  adjustLights();

  stepper.setMaxSpeed(1000);
  //stepper.setAcceleration(50);
  stepper.setSpeed(400);
  stepper.moveTo(200);
  //stepper.setCurrentPosition(0);      

  // configure PWM functionalitites
  ledcSetup(pwmChannel_0, freq, pwmResolution);
  ledcSetup(pwmChannel_2, freq, pwmResolution);
  // attach the channel to the GPIO to be controlled
  
  
  display.setTextColor(WHITE);
  
  initRotary();

  

  configWifi(false);

  initTime(myTimezone);

  adjustLights();

  // Hash the username and password obtained from the user and store it to the global variables
  hashedUsername = hashString(usernameString);
  hashedPassword = hashString(passwordString);

  // // login to the cloud
  // int loginStatus = cloudLogin(hashedUsername, hashedPassword);

  // if (loginStatus == 1) {
  //   Serial.println("Cloud authentication completed for core 1");
  // } 
  // else {
  //   Serial.println("Cloud authentication failed for core 1");
  // }
  
  initSDcard();
  
  
  
  // execute the listener server on a different core (core 0)
  if(!offlineMode){
    xTaskCreatePinnedToCore(
                    Listener,   /* Task function. */
                    "Listener",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &listenerHandle,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */  
  }
  
  
}

/////////////////////////////////////////////////////////////////
//Rotary methods

//Rottary handler
void IRAM_ATTR handleLoop() {
  r.loop();
  b.loop();
}

void initRotary(){
  r.begin(ROTARY_PIN1, ROTARY_PIN2, CLICKS_PER_STEP, MIN_POS, MAX_POS, 0, INCREMENT);
  r.setRightRotationHandler(screenModeRight);
  r.setLeftRotationHandler(screenModeLeft); 
  b.begin(BUTTON_PIN);
  b.setTapHandler(click);
  b.setLongClickHandler(resetPosition);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &handleLoop, true);
  timerAlarmWrite(timer, 3000, true); // every 0.3 seconds
  timerAlarmEnable(timer);

}



//when turned left
void screenModeLeft(ESPRotary& r){
  if (screenMode == 2 || screenMode == 3 || screenMode == 4){
    menuPosition = r.getPosition();
    Serial.println(menuPosition);
  } else if(screenMode == 1){
    screenMode = 0;
  }
  
  Serial.println(r.directionToString(r.getDirection()));

}

//when turned right
void screenModeRight(ESPRotary& r){
  if (screenMode == 2 || screenMode == 3 || screenMode == 4){
    menuPosition = r.getPosition();
    Serial.println(menuPosition);
  } else if (screenMode == 0){
    screenMode = 1;
  }
  
  Serial.println(r.directionToString(r.getDirection()));
}

// single click
void click(Button2& btn) {

  //r.resetPosition();
  if(inWifiConfig){
    wm.stopConfigPortal();
    offlineMode = true;
  } else if(screenMode == 2){
    executeMenu = true;
  } else if( offlineMode && (screenMode == 1 || screenMode == 0)){
    screenMode = 2;
  } else if(screenMode == 3){
    screenMode = 31;
    pumpTriggerPercent = r.getPosition();
    r.resetPosition();
    r.setUpperBound(100);
  } else if(screenMode == 4){
    lightPercentage = r.getPosition();
    r.resetPosition();
    r.setUpperBound(4);
    screenMode = 0;
  } else if(screenMode == 31){
    pumpStopPercent = r.getPosition();
    r.resetPosition();
    r.setUpperBound(4);
    screenMode = 0;
  } else if(screenMode == 5){
    fanThreshold = r.getPosition();
    r.resetPosition();
    r.setUpperBound(4);
    screenMode = 0;
  }
  
}

// long click
void resetPosition(Button2& btn) {
  reset = true;
}

/////////////////////////////////////////////////////////////////
//Time and timezone methods

void setTimezone(String timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

// Connect to NTP server and adjust timezone
void initTime(String timezone){
  struct tm timeinfo;
  Serial.println("Setting up time");
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  if(!getLocalTime(&timeinfo)){
    Serial.println(" Failed to obtain time");
    return;
  }
  Serial.println("Got the time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
}

/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
//SDcard methods

void logData(float temp, float airHum, float soilHum){
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
  String toWrite = String(temp)+","+ String(airHum)+","+ String(soilHum);
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

void configWifi(bool config) {
  // Change to true when testing to force configuration every time we run
  inWifiConfig = true;
  
  bool forceConfig = config;
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
  //wm.setConfigPortalBlocking(false);
  if (forceConfig)
    // Run if we need a configuration
  {
    
    displayBoot();
    if (!wm.startConfigPortal("M.A.P.L.E.", "homeworkhomies"))
    {
      //Serial.println("failed to connect and hit timeout");
      //delay(3000);
      //reset and try again, or maybe put it to deep sleep
      //ESP.restart();
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(2);
      display.setCursor(0, 20);
      display.print("Offline");
      display.display();
      delay(2000);
      inWifiConfig = false;
      return;
    }
  }
  else
  {
    displayBoot();
    if (!wm.autoConnect("M.A.P.L.E.", "homeworkhomies"))
    {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(2);
      display.setCursor(0, 20);
      display.println("Offline");
      display.display();
      delay(2000);
      inWifiConfig = false;
      return;
      
    }
  }
 
  // If we get here, we are connected to the WiFi
  offlineMode = false;
  inWifiConfig = false;
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

  //configTime(0, 0, "pool.ntp.org");
}
/////////////////////////////////////////////////////////////////
//display methods

void displaySoilData(){
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Soil humidity:");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(convertToSoilHumidity(analogRead(SOIL_SENSOR)));
  display.print(" %");
  
  
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Last watering: ");
  display.setTextSize(1);
  //display.setCursor(0, 45);
  display.println("");
  display.println("");
  display.print(pumpingTime);
   
  
  display.display();  
}

void displayTempHumidity(){
  display.clearDisplay();
  // display temperature
  display.setTextColor(SSD1306_WHITE);
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



void displayMaple(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 15);
  display.println("M.A.P.L.E.");
  display.display();
}

void displayBoot(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("LOG IN ...");
  display.setCursor(0, 25);
  //display.setCursor(40, display.getCursorY());
  display.println("    OR    ");
  display.setCursor(0, 45);
  display.println(" CONTINUE?");
  display.display();
}

void displayMenu(){
  display.clearDisplay();
  const char *options[5] = { 
     
    "   WIFI   \n   LOGIN  ", 
    " WATERING \n THRESHOLD", 
    "  LIGHT   \n THRESHOLD",
    "   FAN    \n THRESHOLD",
    "   EXIT   " 
  };
  
  display.setCursor(40, 0);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.println("MENU");
  display.println("");
  
    display.setCursor(0, 25);
    display.setTextSize(2);
    display.setTextColor(SSD1306_BLACK,SSD1306_WHITE);
    display.println(options[menuPosition]);
  display.display();
}

void displayPumping(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(5, 25);
  display.print("Pumping...");
  display.display();
}

void displayWateringOption(){
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 4);
  display.println("Dry %");
  display.println("");
  display.print(r.getPosition());
  display.print("%");
  display.display();
}

void displayWateringOption2(){
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 4);
  display.println("Wet %");
  display.println("");
  display.print(r.getPosition());
  display.print("%");
  display.display();
}

void displayLightOption(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 4);
  display.println("Light %");
  display.println("");
  display.print(r.getPosition());
  display.print("%");
  display.display();
}

void displayFanOption(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 4);
  display.println("Air hum %");
  display.println("");
  display.print(r.getPosition());
  display.print("%");
  display.display();
}
/////////////////////////////////////////////////////////////////
//self-explanatory



void adjustLights(){
  if(!offlineMode){
    Serial.println("LIGHTS ONLINE MODE");
    int lightOn = lightStartTime.substring(0,2).toInt()*60+lightStartTime.substring(3).toInt();
    int lightOff = lightStopTime.substring(0,2).toInt()*60+lightStopTime.substring(3).toInt();
    char timeStr[20];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    int timeNow = (timeinfo->tm_hour)*60+(timeinfo->tm_min);
    Serial.println(lightOn);
    Serial.println(lightOff);
    Serial.println(timeNow);
    if(timeNow > lightOn && timeNow<lightOff){
      Serial.println("LIGHT ON TIME");
      int lightIntensity = analogRead(LIGHT_SENSOR);
    
      //Serial.println(lightIntensity);
      int brightnessPercent = map(lightIntensity, 0, 4095, 0, 100);
      if(brightnessPercent>=lightPercentage){
        brightness=0;
      } else {
        
        brightness = map(lightPercentage-brightnessPercent,0,100,0,255);
      }
    } else brightness = 0;
  } else {
    Serial.println("LIGHTS OFFLINE MODE");
    int lightIntensity = analogRead(LIGHT_SENSOR);
    int brightnessPercent = map(lightIntensity, 0, 4095, 0, 100);
      if(brightnessPercent>=lightPercentage){
        brightness=0;
      } else {
        brightness = map(lightPercentage-brightnessPercent,0,100,0,255);
      }
  }
  
  
  /*
  if(brightnessPercent<lightCutOff){
    if(brightnessPercent>lightMax){
      brightness = map(brightnessPercent,lightCutOff,lightMax,0,255);
    } else {
      brightness = 255;
    }
    //Serial.println(brightness);
  } else {
    brightness = 0;
  }
  */
  //ledcWrite(pwmChannel_0, 255);
  ledcWrite(pwmChannel_0, brightness);
}

bool needsWater(){
  if(watering){
    if(convertToSoilHumidity(analogRead(SOIL_SENSOR))<pumpStopPercent){
      return true;
    } else return false;
  }
  if(convertToSoilHumidity(analogRead(SOIL_SENSOR))<pumpTriggerPercent){
    return true;
  } else return false;
}

void pumpWater(){
  
  digitalWrite(PUMP_POWER,HIGH);
  displayPumping();
  for(int i = 0; i<1000; ++i){
  //for(;;){
    
    digitalWrite(STEP, HIGH);
    delayMicroseconds(pumpSpeed);
    digitalWrite(STEP, LOW);
    delayMicroseconds(pumpSpeed);
    
  }
  
  digitalWrite(PUMP_POWER,LOW);
}


void checkAirHumidity(){
  
  if(bme.readHumidity()>fanThreshold){
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
  //Serial.println(analogInput);
   int soilMoisturePercent = map(analogInput, analogDry, analogWet, 0, 100);
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
  
  return soilMoisturePercent;
}

void loop() {
  if(reset) {

    display.clearDisplay();
          display.setTextSize(2);
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(0, 40);
          display.print("Resetting...");
          display.display();
          WiFi.disconnect(false,true);
          delay(2000);
          wm.resetSettings();
          delay(2000);
          ESP.restart();
          
  }
  
  if (screenMode == 0){
    displayTempHumidity();
  } else if (screenMode == 1) {
    displaySoilData();
  } else if (screenMode == 2) {
    displayMenu();
    if(executeMenu){
      //Serial.println(menuPosition);
      switch(menuPosition){
        
        case 0:
          display.clearDisplay();
          display.setTextSize(2);
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(0, 40);
          display.print("Processing\n   ....   ");
          display.display();
          WiFi.disconnect(false,true);
          delay(2000);
          wm.resetSettings();
          delay(2000);
          ESP.restart();
          break;
        case 1:
          Serial.println("MODE 3");
          screenMode = 3;
          r.setUpperBound(100);
          
          break;
        case 2:
          Serial.println("MODE 4");
          screenMode = 4;
          r.setUpperBound(100);
          break;
        case 3:
          screenMode = 5;
          r.setUpperBound(100);
          break;
        case 4:
          screenMode = 0;
          break;

      }
      executeMenu = false;
      menuPosition = 0;
      r.resetPosition();
    }
    
  } else if (screenMode == 3){    
    displayWateringOption();
  } else if (screenMode == 4){    
    displayLightOption();
  } else if (screenMode == 31){
    displayWateringOption2();
  } else if (screenMode = 5){
    displayFanOption();
  }
  
  
  unsigned long currentMillis = millis();

  //adjustLights(); 

  if (currentMillis - lightCheckPreviousMillis >= lightCheckInterval) {
    lightCheckPreviousMillis = currentMillis;

    adjustLights(); 
    
  }
 
  if (currentMillis - pumpCheckPreviousMillis >= pumpCheckInterval) {
    pumpCheckPreviousMillis = currentMillis;

     if (needsWater()) pumpWater();
     if (needsWater()) {
       pumpCheckInterval = 60000;
       watering = true;
     } else  {
       if(offlineMode){

       } else{
         pumpCheckInterval = pumpInterval;
        watering = false;
        char timeStr[20];
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        strftime(timeStr, sizeof(timeStr), "%y/%m/%d %H:%M:%S", timeinfo);
        pumpingTime = String(timeStr);
      }
       
     }
  }
  
  //log and send data
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    float temp = 0;
    float airHum = 0;
    float soilHum = 0;
    int count = 10;
    for (int i = 0 ; i<count ; ++i){
      temp += bme.readTemperature();
      airHum += bme.readHumidity();
      soilHum += convertToSoilHumidity(analogRead(SOIL_SENSOR));
    }
    temp = temp/(float)count;
    airHum = airHum/(float)count;
    soilHum = soilHum/(float)count;
    logData(temp, airHum, soilHum); //to SDcard
    if(!offlineMode){
      sendData(temp, airHum, soilHum); //to cloud    
    }
    

  }
  checkAirHumidity();
  
}
