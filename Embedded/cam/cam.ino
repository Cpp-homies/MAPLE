/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-cam-photo-microsd-card-timestamp
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "esp_camera.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "time.h"


// REPLACE WITH YOUR TIMEZONE STRING: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
String myTimezone ="EET-2EEST,M3.5.0/3,M10.5.0/4";

// Constants for photo sending
String hashedUsername = "e3b89e9d33f88e523083d8b4436adcc3726c89e97fd3179a2e102d765d1b16ed";
String serverPath = "/upload-image/" + hashedUsername;
String serverName = "cloud.kovanen.io";
const int serverPort = 443;

WiFiClientSecure client;



// Pin definition for CAMERA_MODEL_AI_THINKER
// Change pin definition if you're using another ESP32 camera module
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Stores the camera configuration parameters
camera_config_t config;

long currentMillis = 0;
long previousMillis = 0;
long interval = 3600000;


// Initializes the camera
void configInitCamera(){
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; //YUV422,GRAYSCALE,RGB565,JPEG
  config.grab_mode = CAMERA_GRAB_LATEST;

  // Select lower framesize if the camera doesn't support PSRAM
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10; //0-63 lower number means higher quality
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Initialize the Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

// Connect to wifi
void  initWiFi(){
  bool connected = false;
  String ssid = "";
  String pass = "";
  bool psswd = false;
  while(!connected){
    if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');
    input.trim();
    Serial.println("Input:");
    Serial.println(input);
    if (input == "0") {
      
      while(ssid==""){
        if (Serial.available()){
          
          ssid = Serial.readStringUntil('\n');
          ssid.trim();
          Serial.println("Got something:");
          Serial.println(ssid);
        }
      }
      while(!psswd){
        if (Serial.available()){
          pass = Serial.readStringUntil('\n');
          pass.trim();
          Serial.println("Got something:");
          Serial.println(pass);
          psswd = true;          
        }
      }
      WiFi.begin(ssid.c_str(), pass.c_str());
      Serial.println("Connecting Wifi");
      while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
        
      }
      connected = true;
    }
    
  }
  }
  
  
}

// Function to set timezone
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

// Get the picture filename based on the current ime
String getPictureFilename(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "";
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d-%H-%M-%S", &timeinfo);
  Serial.println(timeString);
  String filename = "/picture_" + String(timeString) +".jpg";
  return filename; 
}

// Initialize the micro SD card
void initMicroSDCard(){
  // Start Micro SD card
  Serial.println("Starting SD Card");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }
}

// Take photo and save to microSD card
void takeSavePhoto(){
  // Take Picture with Camera
  camera_fb_t * fb = esp_camera_fb_get();
 
  //Uncomment the following lines if you're getting old pictures
  //esp_camera_fb_return(fb); // dispose the buffered image
  //fb = NULL; // reset to capture errors
  //fb = esp_camera_fb_get();
  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  // Path where new picture will be saved in SD Card
  String path = getPictureFilename();
  Serial.printf("Picture file name: %s\n", path.c_str());
  
  // Save picture to microSD card
  fs::FS &fs = SD_MMC; 
  File file = fs.open(path.c_str(),FILE_WRITE);
  if(!file){
    Serial.printf("Failed to open file in writing mode");
  } 
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf("Saved: %s\n", path.c_str());

    // send the photo to the server
    sendPhotoFromSDCard(path.c_str());
  }
  file.close();
  esp_camera_fb_return(fb); 
}

// Helper function that returns the file name from a given file path
String filenameFromPath(String filePath) {
  int lastSlashIndex = filePath.lastIndexOf('/');
  String fileName = "";

  if(lastSlashIndex != -1) {
    fileName = filePath.substring(lastSlashIndex + 1);
  }

  return fileName;
}

String sendPhotoFromSDCard(const char* filePath) {
  String getAll;
  String getBody;
  
  File file = SD_MMC.open(filePath);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "Failed to open file for reading";
  }
  
  String filename = filenameFromPath(String(filePath));

  Serial.println("Connecting to server: " + serverName);
  client.setInsecure(); //skip certificate validation
  if (client.connect(serverName.c_str(), serverPort)) {
    Serial.println("Connection successful!");
    
    String serverPath = "/upload-image";
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint32_t imageLen = file.size();
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;
  
    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    client.print(head);
  
    uint8_t *fbBuf = new uint8_t[1024];
    for (size_t n=0; n<imageLen; n=n+1024) {
      file.read(fbBuf, min(1024, (int)(imageLen-n)));
      client.write(fbBuf, min(1024, (int)(imageLen-n)));
    }
    client.print(tail);
    
    file.close();
    free(fbBuf);
    
    int timoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + timoutTimer) > millis()) {
      Serial.print(".");
      delay(100);      
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (getAll.length()==0) { state=true; }
          getAll = "";
        }
        else if (c != '\r') { getAll += String(c); }
        if (state==true) { getBody += String(c); }
        startTimer = millis();
      }
      if (getBody.length()>0) { break; }
    }
    Serial.println();
    client.stop();
    Serial.println(getBody);
  }
  else {
    getBody = "Connection to " + serverName +  " failed.";
    Serial.println(getBody);
  }
  return getBody;
}

bool connectWifi(String BSSID, String password) {
  WiFi.begin(BSSID.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  return true;
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

  Serial.begin(115200);
  while(!Serial){}

  // Initialize Wi-Fi
  initWiFi();
  // Initialize time with timezone
  initTime(myTimezone);    
  // Initialize the camera  
  Serial.print("Initializing the camera module...");
  configInitCamera();
  Serial.println("Ok!");
  // Initialize MicroSD
  Serial.print("Initializing the MicroSD card module... ");
  initMicroSDCard();

  

}

void loop() {    
  // Take and Save Photo
  long currentMillis = millis();
  if (previousMillis == 0 || (currentMillis - previousMillis >= interval)) {
    previousMillis = currentMillis;
    takeSavePhoto();    
  }
  
  
}
