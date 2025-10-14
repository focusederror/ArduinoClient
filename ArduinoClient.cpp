#include <WiFi.h>
#include <WiFiClient.h> 
#include "secrets.h"
#include <SensirionI2cScd4x.h>
#include <Wire.h>
#include <Arduino.h>


//Retrieved from secrets.h
const char ssid[] = SECRET_SSID;
const char password[] = SECRET_PASS;

const char* serverIp = "192.168.12.188";
const int serverPort = 8080;
String macAddress = "";
boolean macSent = false;
WiFiClient client;
bool isConnected = false;

#define HUM_RELAY_PIN 7 
#define FAN_RELAY_PIN 8

#define NO_ERROR 0
SensirionI2cScd4x sensor;

static char errorMessage[64];
static int16_t error;

String commandBuffer = "";

void PrintUint64(uint64_t& value) {
  Serial.print("0x");
  Serial.print((uint32_t)(value >> 32), HEX);
  Serial.print((uint32_t)(value & 0xFFFFFFFF), HEX);
}

String macToString(byte mac[]) {
  char buf[18];
  snprintf(buf, sizeof(buf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

//System timings for non-blocking operations
unsigned long now = 0;
const long connectAttemptInterval = 10000;
unsigned long lastConnectAttempt = 0;


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

  error = sensor.startLowPowerPeriodicMeasurement();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute startLowPowerPeriodicMeasurement(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return;
  }

  Serial.print("serial number: 0x");
  PrintUint64(serialNumber);
  Serial.println();

  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(HUM_RELAY_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);

  //wifi set up
  Serial.println("\n");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); 
  
  byte mac[6];
  WiFi.macAddress(mac);
  macAddress = macToString(mac);
}

void loop() {
  now = millis();
  if (!client.connected() && now - lastConnectAttempt >= connectAttemptInterval) {
    macSent = false; //reset mac sent flag on disconnect. Haults data sending
    lastConnectAttempt = now;
    Serial.println("Attempting to connect...");
    if (client.connect(serverIp, serverPort)) {
      Serial.println("Connected to client!");
      client.print("Arduino connected!");
    } else {
      Serial.print("Failed to connect to client at ");
      Serial.print(serverIp);
      Serial.print(":");
      Serial.println(serverPort);
      return;
    }
  }

  //Non-blocking command receive: read bytes and assemble command
  while (client.available()) {
    char c = client.read();
    if (c == '\n') {
      commandBuffer.trim();
      if (commandBuffer.length() > 0) {
        Serial.print("Received command: ");
        Serial.println(commandBuffer);

        if (commandBuffer == "LIGHT_ON") {
          digitalWrite(LED_BUILTIN, HIGH); // LED on
          Serial.println("LED turned ON");
          client.print("Received: LIGHT_ON\n");
        } else if (commandBuffer == "LIGHT_OFF") {
          digitalWrite(LED_BUILTIN, LOW); // LED off
          Serial.println("LED turned OFF");
          client.print("Received: LIGHT_OFF\n");
        } else if (commandBuffer == "HUM_ON") {
          digitalWrite(HUM_RELAY_PIN, HIGH);
          Serial.println("Relay turned ON");
          client.print("Received: RELAY_ON;" + String(digitalRead(HUM_RELAY_PIN)) + "\n");
        } else if (commandBuffer == "HUM_OFF") {
          digitalWrite(HUM_RELAY_PIN, LOW);
          Serial.println("Relay turned OFF");
          client.print("Received: RELAY_OFF;" + String(digitalRead(HUM_RELAY_PIN)) + "\n");
        } else if (commandBuffer == "FAN_ON") {
          digitalWrite(FAN_RELAY_PIN, HIGH);
          Serial.println("Relay turned ON");
          client.print("Received: RELAY_ON;" + String(digitalRead(FAN_RELAY_PIN)) + "\n");
        } else if (commandBuffer == "FAN_OFF") {
          digitalWrite(FAN_RELAY_PIN, LOW);
          Serial.println("Relay turned OFF");
          client.print("Received: RELAY_OFF;" + String(digitalRead(FAN_RELAY_PIN)) + "\n");
        } else if (commandBuffer == "MAC_ACK") {  //Client will not send data until it receives MAC_ACK
          macSent = true;
          Serial.println("MAC acknowledged by server");
        }
        //Add more else if blocks for other commands here
      }
      commandBuffer = "";
    } else {
      commandBuffer += c;
    }
  }

  uint16_t co2 = 0;
  float temp = 0.0;
  float humid = 0.0; 

  bool dataReady = false;
  error = sensor.getDataReadyStatus(dataReady);

  if (!macSent) {
    client.print("MAC;" + macAddress + "\n");
    Serial.println("Sent MAC address to server: " + macAddress);
  } else if (error == NO_ERROR && dataReady) {
    error = sensor.readMeasurement(co2, temp, humid);

    String humState;
    String fanState;

    if (String(digitalRead(HUM_RELAY_PIN)) == "1") {
      humState = "True";
    } else {
      humState = "False";
    } 
    if (String(digitalRead(FAN_RELAY_PIN)) == "1") {
      fanState = "True";
    } else {
      fanState = "False";
    }

    String message = "SCD4X;" + String(co2) + ";" + String(temp) + ";" + String(humid) + ";" + humState + ";" + fanState;
    client.print(message + "\n");
    
  }
}