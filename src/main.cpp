#include <Arduino.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // need to install this library ArduinoJson by Benoit Blanchon

// Define Structs

typedef struct dataStruct {
  String meterId;
  int accumulatedValue;
} dataStruct;


// Define Variables
xQueueHandle dataQueue;
SemaphoreHandle_t sdCardMutex;
int accumulation = 0;

const char* ssid = "your-ssid";
const char* password = "your-password";
const char* apiUrl = "https://example.com/api/data";


// Define the pins
const int impulsePin1 = 0; // Impulse pin
const int impulsePin2 = 1; // Impulse pin
const int impulsePin3 = 3; // Impulse pin
const int impulsePin4 = 33; // Impulse pin


// Define the meter ids
const String meterId1 = "111";
const String meterId2 = "222";
const String meterId3 = "333";
const String meterId4 = "444";


// define functions
void impulseDetected1();
void impulseDetected2();
void impulseDetected3();
void impulseDetected4();

void sendImpulseDataToQueue(String meterId);

void queueDataHandling(void *pvParameters);
void sendToApi(void *pvParameters);

void apiCall(dataStruct data);

// setup starts here
void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // initialize sd card
  if(!SD_MMC.begin()){
    Serial.println("Card Mount Failed");
    while(1);
  }

  // check if sd card file exists
  // if it does not exist create it
  if(!SD_MMC.exists("data.json"))
  {
    File file = SD_MMC.open("/data.json", FILE_WRITE);
    file.close();
  }


  // create semaphore to lock sd-card access
  sdCardMutex = xSemaphoreCreateMutex();

  if(sdCardMutex == NULL){
    Serial.println("Mutex creation failed");
    while(1);
  }


  pinMode(impulsePin1, INPUT); // sets pin to input
  attachInterrupt(impulsePin1, impulseDetected1, RISING); // sets interrupt when pin goes from low to high

  pinMode(impulsePin2, INPUT);
  attachInterrupt(impulsePin2, impulseDetected2, RISING);

  pinMode(impulsePin3, INPUT);
  attachInterrupt(impulsePin3, impulseDetected3, RISING); 

  pinMode(impulsePin4, INPUT); 
  attachInterrupt(impulsePin4, impulseDetected4, RISING);


  dataQueue = xQueueCreate(100, sizeof(dataStruct));

  xTaskCreate(                  // create a new rtos  task
    queueDataHandling,          // the name of what function will run
    "queueDataHandling",        // the name of the task
    1024,                       // the stack size of the task
    NULL,                       // the parameter passed to the task
    1,                          // the priority of the task
    NULL                        // the task handle
  );

  xTaskCreate(
    sendToApi,
    "SendToAPI",
    1024,
    NULL,
    2,
    NULL
  );

  vTaskStartScheduler(); // starts rtos

}

// Interrupt functions

void impulseDetected1() {

  Serial.println("Impulse detected on impulsePin1!");
  sendImpulseDataToQueue(meterId1);

}


void impulseDetected2() {

  Serial.println("Impulse detected on impulsePin2!");
  sendImpulseDataToQueue(meterId2);

}


void impulseDetected3() {

  Serial.println("Impulse detected on impulsePin3!");
  sendImpulseDataToQueue(meterId3);

}


void impulseDetected4() {

  Serial.println("Impulse detected on impulsePin4!");
  sendImpulseDataToQueue(meterId4);

}


// function for interrupts to use

void sendImpulseDataToQueue(String meterId){

    xQueueSendFromISR(dataQueue, &meterId, 0);
}


// RTOS Functions
// need to change if the sd_card library isn't working
void queueDataHandling(void *pvParameters){
  
    dataStruct data;
    String meterId;
    while(1){

      if(xQueueReceive(dataQueue, &meterId, 0))
      {
        data.accumulatedValue = ++accumulation;
        data.meterId = meterId;
        // take mutex
        if(xSemaphoreTake(sdCardMutex, portMAX_DELAY) != pdTRUE){
          Serial.println("Mutex failed to be taken within max delay");
          continue;
        }

        // open sd card
        File file = SD_MMC.open("/data.json", FILE_APPEND);

        // check if file opened
        if(!file){

          Serial.println("Failed to open file for appending");
          return;

        }

        // format data as json file
        String dataString = "{\n\t\"meterId\": \"" + data.meterId + "\",\n\t\"accumulatedValue\": " + String(data.accumulatedValue) + "\n}\n";

        // write data to sd card
        file.print(dataString);

        // close sd card
        file.close();

        // give mutex
        xSemaphoreGive(sdCardMutex);

        // data is remove from queue on recieve
      }

      // add vtaskdelay() ???


    }

}


void sendToApi(void *pvParameters){
  while(1){
    // take mutex
                      // mutex     // max delay
    if(xSemaphoreTake(sdCardMutex, portMAX_DELAY) != pdTRUE){
      Serial.println("Mutex failed to be taken within max delay");
      continue;
    }

    // read file from sd card
    File file = SD_MMC.open("/data.json", FILE_READ);

    if(!file){
        
        Serial.println("Failed to open file for reading");
        return;
    }

    // allocate buffer for json file
    size_t bufferSize = file.size();
    std::unique_ptr<char[]> buf(new char[bufferSize]);

    file.readBytes(buf.get(), bufferSize);

    // close file
    file.close();

    // overwrite file
    file = SD_MMC.open("/data.json", FILE_WRITE);
    file.close();

    // give mutex
    xSemaphoreGive(sdCardMutex);

    // parse json file
    JsonDocument doc;
    // DynamicJsonDocument doc(1024); //deprecated  
    DeserializationError error = deserializeJson(doc, buf.get());

    if(error){
      Serial.println("Failed to parse json file");
      return;
    }

    // json to data struct
    JsonArray array = doc.as<JsonArray>();

    for (int i = 0; i < array.size(); i++)
    {
      dataStruct data;
      data.meterId = array[i]["meterId"].as<String>();
      data.accumulatedValue = array[i]["accumulatedValue"].as<int>();
      apiCall(data);
    }

    // wait 10 seconds
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}


void apiCall(dataStruct data){

  // open wifi connection
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println("Connected to WiFi");

  // open http.client
  HTTPClient http;

  // send data to api
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");

  String dataString = "{\n\t\"meterId\": \"" + data.meterId + "\",\n\t\"accumulatedValue\": " + String(data.accumulatedValue) + "\n}\n";

  int httpResponseCode = http.POST(dataString);

  if(httpResponseCode > 0){
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  // close http.client
  http.end();

  // close wifi connection
  WiFi.disconnect(true);

}


void loop(){

}