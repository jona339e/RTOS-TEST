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
const int impulsePin1 = 13; // Impulse pin
const int impulsePin2 = 16; // Impulse pin
const int impulsePin3 = 32; // Impulse pin
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

void queueDataHandling(void *pvParameters);
void sendToApi(void *pvParameters);

void apiCall(dataStruct data);

bool setupSdCard();
bool setupMutex();
bool setupInterrupts();

// setup starts here
void setup() {

  Serial.begin(115200);
  Serial.println("Starting...");

  pinMode(impulsePin1, INPUT); // sets pin to input
  pinMode(impulsePin2, INPUT);
  pinMode(impulsePin3, INPUT);
  pinMode(impulsePin4, INPUT);

  Serial.println("Pins set to input");
  // delay(2000);

  // WiFi.begin(ssid, password);

  // while(WiFi.status() != WL_CONNECTED){
  //   delay(500);
  //   Serial.println("Connecting to WiFi..");
  // }

  if(!setupSdCard()){
    Serial.println("sd-card setup failed...");
    while(1);
  }
 
  if(!setupMutex()){
    Serial.println("Mutex Creation Failed");
    while(1);
  }

  // delay(2000);

  Serial.println("Starting dataQueue creation");

  // delay(200);

  dataQueue = xQueueCreate(10, sizeof(String));

  // delay(200);

  Serial.println("dataQueue creation complete");

  // delay(2000);



  
  
  if(!setupInterrupts()){
    Serial.println("Interrupts setup failed...");
    while(1);
  }

  // delay(2000);
    
  Serial.println("Starting xTaskCreate");
  // delay(500);

  xTaskCreate(                  // create a new rtos  task
    queueDataHandling,          // the name of what function will run
    "Queue Data Handling",      // the name of the task
    4096,                      // the stack size of the task
    NULL,                       // the parameter passed to the task
    1,                          // the priority of the task
    NULL                        // the task handle
  );

  // delay(2000);
    Serial.println("First xTaskCreate Completed");
  // delay(2000);

  xTaskCreate(
    sendToApi,
    "Api Call",
    4096,
    NULL,
    2,
    NULL
  );

  // delay(200);
  
  Serial.println("xTaskCreate complete");



  // delay(2000);

  // vTaskStartScheduler(); // starts rtos
  // delay(2000);
  Serial.println("Setup done");
}

// setup functions

bool setupMutex(){
  // create semaphore to lock sd-card access
  while(sdCardMutex == NULL)
  {
    // delay(2000);
    sdCardMutex = xSemaphoreCreateMutex();
  }
  // delay(2000);
  Serial.println("Mutex created");
  return true;
}


bool setupSdCard(){
   // initialize sd card

  while (!SD_MMC.begin())
  {
    Serial.println("SD-Card Mounting");
    // delay(500);
  }
  Serial.println("SD-Card Mounted");
  // delay(2000);

  // check if sd card file exists
  // if it does not exist create it

  if(!SD_MMC.exists("/data.json"))
  {
    // delay(2000);
    Serial.println("File does not exist, creating file");
    File file = SD_MMC.open("/data.json", FILE_WRITE);
    file.close();
  }
  // delay(2000);

  Serial.println("File exists / is created");
  return true;

}

bool setupInterrupts(){
  attachInterrupt(impulsePin1, impulseDetected1, RISING); // sets interrupt when pin goes from low to high
  // delay(500);

  attachInterrupt(impulsePin2, impulseDetected2, RISING);
  // delay(500);

  attachInterrupt(impulsePin3, impulseDetected3, RISING); 
  // delay(500);
    
  attachInterrupt(impulsePin4, impulseDetected4, RISING);
  Serial.println("Interrupts attached");

  return true;
}


// Interrupt functions

void impulseDetected1() {

  Serial.println("Impulse detected on impulsePin1!");
  delay(20);
  xQueueSendFromISR(dataQueue, &meterId1, 0);

}


void impulseDetected2() {

  Serial.println("Impulse detected on impulsePin2!");
  delay(20);
  xQueueSendFromISR(dataQueue, &meterId2, 0);

}


void impulseDetected3() {

  Serial.println("Impulse detected on impulsePin3!");
  delay(20);
  xQueueSendFromISR(dataQueue, &meterId3, 0);

}


void impulseDetected4() {

  Serial.println("Impulse detected on impulsePin4!");
  delay(20);
  xQueueSendFromISR(dataQueue, &meterId4, 0);

}


// RTOS Functions
// need to change if the sd_card library isn't working
void queueDataHandling(void *pvParameters){
  vTaskDelay(10000 / portTICK_PERIOD_MS);
  while(1)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("QueueDataHandling Started");

    dataStruct data;
    String meterId;

    if(xQueueReceive(dataQueue, &meterId, 0))
    {
      data.accumulatedValue = ++accumulation;
      data.meterId = meterId;
      // take mutex
      if(xSemaphoreTake(sdCardMutex, portMAX_DELAY) != pdTRUE){
        Serial.println("Mutex failed to be taken within max delay");
        return;
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
  vTaskDelay(10000 / portTICK_PERIOD_MS);
  while(1)
  {
    Serial.println("sendToApi Started");

    // take mutex
                      // mutex     // max delay
    if(xSemaphoreTake(sdCardMutex, portMAX_DELAY) != pdTRUE){
      Serial.println("Mutex failed to be taken within max delay");
      return;
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
    }

    if(!error){

      // json to data struct
      JsonArray array = doc.as<JsonArray>();

      

      for (int i = 0; i < array.size(); i++)
      {
        dataStruct data;
        data.meterId = array[i]["meterId"].as<String>();
        data.accumulatedValue = array[i]["accumulatedValue"].as<int>();
        apiCall(data);
      }
    }
    Serial.println("sendToApi Finished");
    vTaskDelay(10000 / portTICK_PERIOD_MS);

  }
}


void apiCall(dataStruct data){

  // open wifi connection


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
  // WiFi.disconnect(true);

}


void loop(){

}