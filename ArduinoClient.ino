#include <WiFi.h>
#include <WiFiClient.h> 
#include "secrets.h"
#include <SensirionI2cScd4x.h>
#include <Wire.h>

//Retrieved from secrets.h
const char ssid[] = SECRET_SSID;
const char password[] = SECRET_PASS;

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

//Pin for relay control
#define RELAY1_PIN 7
#define RELAY2_PIN 8

SensirionI2cScd4x sensor;

static char errorMessage[64];
static int16_t error;

void PrintUint64(uint64_t& value) {
  Serial.print("0x");
  Serial.print((uint32_t)(value >> 32), HEX);
  Serial.print((uint32_t)(value & 0xFFFFFFFF), HEX);
}

const char* serverIp = "192.168.12.188";
const int serverPort = 8080;
WiFiClient client;

bool LED_On = false;

//Timing for sending data to server
const long sendInterval = 5000;
unsigned long lastSendTime = 0;


void setup() {
  Serial.begin(115200);
  delay(100);            

  Wire.begin();
  sensor.begin(Wire, SCD41_I2C_ADDR_62);

  uint64_t serialNumber = 0;
  delay(30);
  
  error = sensor.wakeUp();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute wakeUp(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }
  error = sensor.stopPeriodicMeasurement();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }
  error = sensor.reinit();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute reinit(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }
  
  error = sensor.getSerialNumber(serialNumber);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }
  Serial.print("serial number: 0x");
  PrintUint64(serialNumber);
  Serial.println();

  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  //wifi set up
  Serial.println("\n");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  
  while (WiFi.status() != WL_CONNECTED) {//Wait until connected to wifi
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); 
  delay(1000);
}

void loop() {
  //Check if the client is *not* connected to the server
  if (!client.connected()) {
    Serial.println("Disconnected from server. Attempting to reconnect...");
    // If not connected, try to connect
    if (client.connect(serverIp, serverPort)) {
      Serial.println("Reconnected to server!");
      client.print("Arduino reconnected!");  // Send a message upon reconnectioAcknowledgen
    } else {
      Serial.print("Failed to reconnect to server at ");
      Serial.print(serverIp);
      Serial.print(":");
      Serial.println(serverPort);
      delay(10000);
      return;
    }
  }

  //-Receive Data from Server-
  if (client.available()) {
    String command = client.readStringUntil('\n');  //Read until newline
    command.trim();                                 //then trim

    Serial.print("Received command: ");
    Serial.println(command);

    //-Command Parsing Logic-
    if (command == "LIGHT_ON") {
      digitalWrite(LED_BUILTIN, HIGH);  //LED on
      Serial.println("LED turned ON");
      client.print("ACK:LIGHT_ON\n");  
    } else if (command == "LIGHT_OFF") {
      digitalWrite(LED_BUILTIN, LOW);  //LED off
      Serial.println("LED turned OFF");
      client.print("ACK:LIGHT_OFF\n");  
    }  else if (command == "RELAY1_ON"){ //relay on
      digitalWrite(RELAY1_PIN, HIGH);
      Serial.println("Relay turned ON");
      client.print("ACK:RELAY_ON;" + String(digitalRead(RELAY1_PIN)) + "\n");
    } else if (command == "RELAY1_OFF"){
      digitalWrite(RELAY1_PIN, LOW); //relay off
      Serial.println("Relay turned OFF");
      client.print("ACK:RELAY_OFF;" + String(digitalRead(RELAY1_PIN)) + "\n");
    } else if (command == "RELAY2_ON"){ //relay on
      digitalWrite(RELAY2_PIN, HIGH);
      Serial.println("Relay turned ON");
      client.print("ACK:RELAY_ON;" + String(digitalRead(RELAY2_PIN)) + "\n");
    } else if (command == "RELAY2_OFF"){
      digitalWrite(RELAY2_PIN, LOW); //relay off
      Serial.println("Relay turned OFF");
      client.print("ACK:RELAY_OFF;" + String(digitalRead(RELAY2_PIN)) + "\n");
    }
    //Add more else if blocks for other commands here
  }

  uint16_t co2Concentration = 0;
  float temperature = 0.0;
  float relativeHumidity = 0.0; 

  //
  // Wake the sensor up from sleep mode.
  //
  error = sensor.wakeUp();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute wakeUp(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }
  //
  // Ignore first measurement after wake up.
  //
  error = sensor.measureSingleShot();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute measureSingleShot(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }

  //Perform single shot measurement and read data.
  error = sensor.measureAndReadSingleShot(co2Concentration, temperature,
                                          relativeHumidity);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute measureAndReadSingleShot(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }

  

  //-Send Data to Server-
  //Check if it's time to send data
  unsigned long currentTime = millis();  // Get current time in milliseconds
  if (currentTime - lastSendTime >= sendInterval) {
    String message = "SCD4X;" + String(co2Concentration) + ";" + String(temperature) + ";" + String(relativeHumidity);
    client.print(message + "\n");  // Send the message, add newline for C# server parsing
    lastSendTime = currentTime;    // Update the last send time
  }

 
  delay(10);
}