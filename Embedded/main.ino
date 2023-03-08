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



#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Adafruit_BME280 bme;

int LIGHT_SENSOR = 12;
int LIGHTS = 40;
int freq = 2000;
int pwmResolution = 8;
int pwmChannel_0 = 0;
uint8_t brightness;

unsigned long previousMillis = 0;
const long interval = 10000;
String BASE_URL = "https://cloud.kovanen.io/sensordata/";

void setup() {
  Serial.begin(115200);

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

  // configure LED PWM functionalitites
  ledcSetup(pwmChannel_0, freq, pwmResolution);
  
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(LIGHTS, pwmChannel_0);

  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);

  connectWifi("aalto open", "");
  configTime(0, 0, "pool.ntp.org");
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
  if(lightIntensity>=300){
    brightness = map(lightIntensity, 300, 4095, 0, 255);
  } else {
    brightness = 0;
  }
  
  ledcWrite(pwmChannel_0, brightness);
}

void loop() {
  
  displayTempHumidity();
  adjustLights();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sendData(bme.readTemperature(), bme.readHumidity());
    
  }
  
}
