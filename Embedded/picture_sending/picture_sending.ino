#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

const char* ssid     = "NETGEAR";
const char* password = "NosTae%#o1";

String serverName = "cloud.kovanen.io";
const int serverPort = 443;

WiFiClientSecure client;

// // Initialize the micro SD card
// void initMicroSDCard(){
//   // Start Micro SD card
//   Serial.println("Starting SD Card");
//   if(!SD_MMC.begin()){
//     Serial.println("SD Card Mount Failed");
//     return;
//   }
//   uint8_t cardType = SD_MMC.cardType();
//   if(cardType == CARD_NONE){
//     Serial.println("No SD Card attached");
//     return;
//   }
// }
void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println('\n');
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
}

void loop() {
  // sendImage("/potato.jpeg", "http://192.168.97.58:8000/upload-image");
  sendPhotoSDCard("/potato.jpeg");
  delay(5000);  // delay for 5 seconds before sending the next image
}

void sendImage(const char* filePath, const char* serverUrl) {
  File file = SD.open(filePath);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "multipart/form-data");

  int httpCode = http.sendRequest("POST", &file, file.size());
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println(payload);
  } else {
    Serial.println("Error on HTTP request");
  }

  http.end();
  file.close();

  // String start_request = "--boundary\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + String(filePath) + "\"\r\nContent-Type: image/jpeg\r\n\r\n";
  // String end_request = "\r\n--boundary--\r\n";
  
  // int totalSize = start_request.length() + file.size() + end_request.length();

  // WiFiClient client;
  // HTTPClient http;
  
  // http.begin(client, serverUrl);
  // http.addHeader("Content-Type", "multipart/form-data; boundary=boundary");

  // int httpCode = http.sendRequest("POST", (uint8_t *)start_request.c_str(), start_request.length(), &file, file.size(), (uint8_t *)end_request.c_str(), end_request.length());
  
  // if (httpCode > 0) {
  //   String payload = http.getString();
  //   Serial.println(payload);
  // } else {
  //   Serial.println("Error on HTTP request");
  // }

  // http.end();
  // file.close();
}

String sendPhotoSDCard(const char* filePath) {
  String getAll;
  String getBody;
  
  File file = SD.open(filePath);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "Failed to open file for reading";
  }
  

  Serial.println("Connecting to server: " + serverName);
  client.setInsecure(); //skip certificate validation
  if (client.connect(serverName.c_str(), serverPort)) {
    Serial.println("Connection successful!");
    
    String serverPath = "/upload-image";
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"file\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
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

