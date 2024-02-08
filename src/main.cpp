#include <Arduino.h>
#include <SD_MMC.h>
#include <HTTPClient.h>
#include <ETH.h> 
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

// const char* apiUrl = "http://192.168.5.132:2050/api/EnergyData/Test"; // Jonas IIS Api
const char* apiUrl = "http://192.168.21.7:2050/api/EnergyData"; // Virtuel Server SKP
// const char* apiUrl = "http://10.233.134.112:2050/api/EnergyData"; // energymeter room laptop server

const char* filename = "/EnergyData.csv";

// Define the pins
const int impulsePin1 = 13; // Impulse pin
const int impulsePin2 = 16; // Impulse pin
const int impulsePin3 = 32; // Impulse pin
const int impulsePin4 = 33; // Impulse pin

const int builtInBtn = 34; // bultin button to simmulate impulse


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
void buttonTest();

void queueDataHandling(void *pvParameters);
void sendToApi(void *pvParameters);


bool setupSdCard();
bool setupMutex();
bool setupInterrupts();

// setup starts here
void setup() {

  Serial.begin(115200);

  pinMode(impulsePin1, INPUT); // sets pin to input
  pinMode(impulsePin2, INPUT);
  pinMode(impulsePin3, INPUT);
  pinMode(impulsePin4, INPUT);


  //Ethernet setup Begin
  if(!ETH.begin()){
    Serial.println("Failed to initialize Ethernet");
    delay(1000);
    while(1);
  }

  if(!setupSdCard()){
    Serial.println("sd-card setup failed...");
    while(1);
  }
 
  if(!setupMutex()){
    Serial.println("Mutex Creation Failed");
    while(1);
  }

  dataQueue = xQueueCreate(20, sizeof(String));

  if(!setupInterrupts()){
    Serial.println("Interrupts setup failed...");
    while(1);
  }

  attachInterrupt(builtInBtn, buttonTest, FALLING);

  xTaskCreate(                  // create a new rtos  task
    queueDataHandling,          // the name of what function will run
    "Queue Data Handling",      // the name of the task
    4096,                       // the stack size of the task
    NULL,                       // the parameter passed to the task
    1,                          // the priority of the task
    NULL                        // the task handle
  );

    // Serial.println("First xTaskCreate Completed");

  xTaskCreate(
    sendToApi,
    "Api Call",
    4096,
    NULL,
    2,
    NULL
  );

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
  }
  Serial.println("SD-Card Mounted");

  // check if sd card file exists
  // if it does not exist create it

  if(!SD_MMC.exists(filename))
  {

    // Serial.println("File does not exist, creating file");
    File file = SD_MMC.open(filename, FILE_WRITE);
    if(file)
    {
      file.println("EnergyMeterID,AccumulatedValue");
    }
    file.close();

  }

  // Serial.println("File exists / is created");
  return true;

}

bool setupInterrupts(){
  attachInterrupt(impulsePin1, impulseDetected1, RISING); // sets interrupt when pin goes from low to high

  attachInterrupt(impulsePin2, impulseDetected2, RISING);

  attachInterrupt(impulsePin3, impulseDetected3, RISING); 
    
  attachInterrupt(impulsePin4, impulseDetected4, RISING);
  
  // Serial.println("Interrupts attached");

  return true;
}


// Interrupt functions

void impulseDetected1() {

  xQueueSendFromISR(dataQueue, &meterId1, 0);

}


void impulseDetected2() {

  xQueueSendFromISR(dataQueue, &meterId2, 0);

}


void impulseDetected3() {

  xQueueSendFromISR(dataQueue, &meterId3, 0);

}


void impulseDetected4() {

  xQueueSendFromISR(dataQueue, &meterId4, 0);

}

void buttonTest(){
  String meterId = "CCC6C8C4-B9DB-4C8D-39D8-08DBEF4C21FB";
  xQueueSendFromISR(dataQueue, &meterId, 0);
}


// RTOS Functions
// need to change if the sd_card library isn't working
void queueDataHandling(void *pvParameters){
  vTaskDelay(500 / portTICK_PERIOD_MS);
  while(1)
  {
    vTaskDelay(20 / portTICK_PERIOD_MS);
    Serial.println("QueueDataHandling Started");

    dataStruct data;
    String meterId;

    // take mutex
    if(xSemaphoreTake(sdCardMutex, portMAX_DELAY) != pdTRUE){
      Serial.println("Mutex failed to be taken within max delay");
      xSemaphoreGive(sdCardMutex);
      return;
    }


    if(xQueueReceive(dataQueue, &meterId, 0))
    {
      data.accumulatedValue = ++accumulation;
      data.meterId = meterId;

      // open sd card
      File file = SD_MMC.open(filename, FILE_APPEND);

      // check if file opened
      if(!file){
        Serial.println("Failed to open file for appending");
        xSemaphoreGive(sdCardMutex);
        return;

      }
      // write data to sd card
    
      file.println(data.meterId + "," + String(data.accumulatedValue));
      Serial.println(data.meterId + "," + String(data.accumulatedValue));

      // close sd card
      file.close();

      // give mutex
      xSemaphoreGive(sdCardMutex);

      // data is remove from queue on recieve
    }

  }

}


void sendToApi(void *pvParameters){
  vTaskDelay(10000 / portTICK_PERIOD_MS);
  while(1)
  {
    // Serial.println("sendToApi Started");

    // take mutex
                      // mutex     // max delay
    if(xSemaphoreTake(sdCardMutex, portMAX_DELAY) != pdTRUE){
      // Serial.println("Mutex failed to be taken within max delay");
      xSemaphoreGive(sdCardMutex);
      return;
    }

    // read file from sd card
    File file = SD_MMC.open(filename, FILE_READ);

    if(!file){
      // Serial.println("Failed to open file for reading");
      xSemaphoreGive(sdCardMutex);
      return;
    }

    // Skip the first line (header)
    if (file.available()) {
      file.readStringUntil('\n');
    }

    int httpResponseCode = 0;

    if(file.available()){


    String payload = "[";

    while (file.available()) {
      String line = file.readStringUntil('\n');
      Serial.println(line);

      // Split the CSV line into values
      String name = line.substring(0, line.indexOf(','));
      String value = line.substring(line.indexOf(',') + 1);

      // Add data to the JSON document
      JsonDocument jsonDocument;
      jsonDocument["EnergyMeterID"] = name;
      jsonDocument["AccumulatedValue"] = value.toInt();

      String temp;
      serializeJson(jsonDocument, temp);
      payload += temp;
      if (file.available())
      {
        payload += ",";
      }
    }

    payload += "]";

    HTTPClient http;
      
    // send data to api
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");

    httpResponseCode = http.POST(payload);

    Serial.println(httpResponseCode);

    // close http.client
    http.end();

    }

    // close file
    file.close();
    if(httpResponseCode == 201)
    {
      // overwrite file
      file = SD_MMC.open(filename, FILE_WRITE);
      if(file)
      {
        file.println("EnergyMeterID,AccumulatedValue");
      }
      file.close();  
    }
    

    // give mutex
    xSemaphoreGive(sdCardMutex);

    vTaskDelay(10000 / portTICK_PERIOD_MS);

  }
}



void loop(){

}