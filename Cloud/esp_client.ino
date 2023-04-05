#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>

unsigned long previousMillis = 0;
const long interval = 10000;
// String BASE_URL = "https://cloud.kovanen.io/sensordata/";
// String BASE_URL = "https://mapleplantapi.azurewebsites.net/";
String BASE_URL = "http://10.100.20.54:8001/";
String WIFI_SSID = "aalto open";
String WIFI_PASS = "";

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
            int randHumid = random() % 30;
            /*
            ****************************/
            
            sendLiveData((float)randTemp, (float)randHumid);
            
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

int connectWifi(String BSSID, String password) {
  WiFi.begin(BSSID.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  return 1;
}

// send data with a timestamp of the current time to the server,
// the data is sent to /sensordata/<timestamp> resource on the cloud server
void sendData(float temp, float airHumid) {
  // Create a JSON payload with the temperature and humidity values
    StaticJsonDocument<128> jsonPayload;
    jsonPayload["temp"] = temp;
    jsonPayload["humid"] = airHumid;

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
void sendLiveData(float temp, float airHumid) {
  // Create a JSON payload with the temperature and humidity values
    StaticJsonDocument<128> jsonPayload;
    jsonPayload["temp"] = temp;
    jsonPayload["humid"] = airHumid;

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

void setup() {
  Serial.begin(115200);
  connectWifi(WIFI_SSID, WIFI_PASS);
  configTime(0, 0, "pool.ntp.org");

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

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    // sendData(10, 20);
  }
}