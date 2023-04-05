

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Button2.h" //  https://github.com/LennartHennigs
#include "ESPRotary.h"
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



#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define CLICKS_PER_STEP   4
#define ROTARY_PIN1	0
#define ROTARY_PIN2	2
#define BUTTON_PIN	15
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Adafruit_BME280 bme;



#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <WiFi.h>
#include <HTTPClient.h>

int LIGHT_SENSOR = 34;
int SOIL_SENSOR = 35;
const int DIR = 27;
const int STEP = 14;
int LIGHTS = 33;
int FAN = 32;
int PUMP_POWER = 12;
int freq = 2000;
int pwmResolution = 8;
int pwmChannel_0 = 0;
int pwmChannel_2 = 2;
uint8_t brightness;
int pumpSpeed = 1500;
int fanSpeed = 240; //190-255
bool fanOn = false;
uint8_t screenMode = 0;

ESPRotary r;
Button2 b;
hw_timer_t *timer = NULL;

void IRAM_ATTR handleLoop() {
  r.loop();
  b.loop();
}

unsigned long previousMillis = 0;
const long interval = 30000;
// String BASE_URL = "https://cloud.kovanen.io/";
// String BASE_URL = "https://mapleplantapi.azurewebsites.net/";
String BASE_URL = "http://10.100.37.238:8001/";


#define ESP_DRD_USE_SPIFFS true
// JSON configuration file
#define JSON_CONFIG_FILE "/config_maple_user.json"
 
// Flag for saving config data
bool shouldSaveConfig = false;
 
// Variables to hold data from custom textboxes
char usernameString[50] = "Username";
char passwordString[50] = "Password";
 
// Define WiFiManager Object
WiFiManager wm;

/////////////////////////////////////////////
// Device-Cloud communication methods
TaskHandle_t listenerHandle;

// Function that get incoming request from the cloud
// return the HTTP response code, and change the JSON document requestArgs to contain any input arguments for the request (if any)
int getNewRequest(DynamicJsonDocument *requestArgs) {
    // Create the address
    String address = BASE_URL + "request-to-esp";
    HTTPClient http;
    http.begin(address);
    Serial.println("Sending request to " + address);
    // Send HTTP GET request to the cloud to check for incoming request
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

// a mini server that check for new data requests from the cloud
void Listener( void * pvParameters ){
  // unsigned long listenerPreviousMillis = 0;
  // const long listeningInterval = 5000;
  // an infinite loop equivalent to the void loop() function on the main core
  for(;;){
    // unsigned long currentMillis = millis();
    // if (currentMillis - listenerPreviousMillis >= listeningInterval) {
      // listenerPreviousMillis = currentMillis;
      // JSON document for keeping request arguments
      // int *requestArgs = (int*) malloc(10 * sizeof(int));
      const size_t capacity = JSON_OBJECT_SIZE(1) + 20;
      DynamicJsonDocument requestArgs(capacity);

      int httpResponseCode = getNewRequest(&requestArgs);

      if (httpResponseCode == -1) {
        Serial.print("Error sending request: ");
        Serial.println(httpResponseCode);
      }
      else {
        // get the request type
        int requestType = requestArgs["type"];

        

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
            int randTemp = random() % 40;
            int randAirHumid = random() % 30;
            int randSoilHumid = random() % 100;
            /*
            ****************************/
            
            sendLiveData((float)randTemp, (float)randAirHumid, (float)randSoilHumid);
            
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
// the data is sent to /sensordata/<timestamp> resource on the cloud server
void sendData(float temp, float airHumid, float soilHumid) {
  // Create a JSON payload with the temperature and humidity values
    StaticJsonDocument<128> jsonPayload;
    jsonPayload["temp"] = temp;
    jsonPayload["air_humid"] = airHumid;
    jsonPayload["soil_humid"] = soilHumid;

    // Serialize the JSON payload to a string
    String payloadString;
    serializeJson(jsonPayload, payloadString);

    String address = BASE_URL + "sensordata/";
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
    StaticJsonDocument<128> jsonPayload;
    jsonPayload["temp"] = temp;
    jsonPayload["air_humid"] = airHumid;
    jsonPayload["soil_humid"] = soilHumid;

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
    String address = BASE_URL + "sensordata/live";
    
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
////////////////////////////////////////////



void setup() {
  Serial.begin(115200);
  while(!Serial){}
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

  // configure LED PWM functionalitites
  ledcSetup(pwmChannel_0, freq, pwmResolution);
  ledcSetup(pwmChannel_2, freq, pwmResolution);

  
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(LIGHTS, pwmChannel_0);
  ledcAttachPin(FAN, pwmChannel_2);
  
  
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);

  configWifi();
  
  initSDcard();
  
  r.begin(ROTARY_PIN1, ROTARY_PIN2, CLICKS_PER_STEP);
  //r.setChangedHandler(rotate);
  //r.setLeftRotationHandler(showDirection);
  //r.setRightRotationHandler(showDirection);
  r.setRightRotationHandler(screenModeSoil);
  r.setLeftRotationHandler(screenModeAir); 
  //r.setIncrement(1);
  b.begin(BUTTON_PIN);
  b.setTapHandler(click);
  b.setLongClickHandler(resetPosition);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &handleLoop, true);
  timerAlarmWrite(timer, 3000, true); // every 0.1 seconds
  timerAlarmEnable(timer);
  //r.enableSpeedup(true);
  //r.setSpeedupIncrement(15);
  //r.setSpeedupInterval(2500);

  // execute the listener server on a different core (core 0)
  xTaskCreatePinnedToCore(
                    Listener,   /* Task function. */
                    "Listener",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &listenerHandle,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */       
  delay(500);
}

/////////////////////////////////////////////////////////////////
//Rotary methods




void screenModeAir(ESPRotary& r){
  screenMode = 0;
  Serial.println(r.directionToString(r.getDirection()));

}

void screenModeSoil(ESPRotary& r){
  screenMode = 1;
  Serial.println(r.directionToString(r.getDirection()));
}

// on change
void rotate(ESPRotary& r) {
   Serial.println(r.getPosition());
}

// on left or right rotation
void showDirection(ESPRotary& r) {
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

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
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
    /*
    listDir(SD, "/", 0);
    createDir(SD, "/mydir");
    listDir(SD, "/", 0);
    removeDir(SD, "/mydir");
    listDir(SD, "/", 2);
    writeFile(SD, "/hello.txt", "Hello ");
    appendFile(SD, "/hello.txt", "World!\n");
    readFile(SD, "/hello.txt");
    deleteFile(SD, "/foo.txt");
    renameFile(SD, "/hello.txt", "/foo.txt");
    readFile(SD, "/foo.txt");
    testFileIO(SD, "/test.txt");
    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
    */
}
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
  /*
  delay(5000);
  Serial.println(0);
  delay(5000);
  Serial.println(wm.getWiFiSSID());
  delay(5000);
  Serial.println(wm.getWiFiPass());
 */
  // Save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveConfigFile();
  }

  configTime(0, 0, "pool.ntp.org");
}

void displaySoilData(){
  display.clearDisplay();
  // display temperature
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Soil humidity:");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print("Some %");
  
  
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

void loop() {
  
  
  if (screenMode == 0){
    displayTempHumidity();
  } else {
    displaySoilData();
  }
  
  adjustLights();
  unsigned long currentMillis = millis();
  if (needsWater()){
    pumpWater();
  }
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    //sendData(bme.readTemperature(), bme.readHumidity());    
  }
  checkAirHumidity();
  
}