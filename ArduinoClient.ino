#include <WiFi.h> // For ESP32. If ESP8266, use <ESP8266WiFi.h>
#include <WiFiClient.h> // For TCP client functionality
#include "secrets.h"
// --- Network Configuration ---
// REPLACE with your network credentials
const char ssid[] = SECRET_SSID;
const char password[] = SECRET_PASS;

// REPLACE with your C# server's IP address (e.g., "192.168.1.105")
// Find this using 'ipconfig' on Windows or 'ifconfig'/'ip a' on Linux/macOS
const char* serverIp = "192.168.12.188";
// This port MUST match the port defined in your C# server (e.g., 8888)
const int serverPort = 8080;

// Create a WiFiClient object to connect to the TCP server
WiFiClient client;

bool LED_On = false; 

// --- Timing for sending data to server ---
const long sendInterval = 5000; // Send data every 5 seconds (5000 milliseconds)
unsigned long lastSendTime = 0; // Tracks when we last sent data


void setup() {
  Serial.begin(115200); // Start serial communication for debugging output
  delay(100); // Small delay to let serial warm up

  pinMode(LED_BUILTIN, OUTPUT); // Configure the LED pin as an output

  Serial.println("\n");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password); // Start Wi-Fi connection process

  // Wait until connected to Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // Print the IP address the Arduino got from the router

  delay(1000); // Wait a bit before trying to connect to the server
}

void loop() {
  // Check if the client is NOT connected to the server
  if (!client.connected()) {
    Serial.println("Disconnected from server. Attempting to reconnect...");
    // If not connected, try to connect
    if (client.connect(serverIp, serverPort)) {
      Serial.println("Reconnected to server!");
      client.print("Arduino reconnected!"); // Send a message upon reconnection
    } else {
      Serial.print("Failed to reconnect to server at ");
      Serial.print(serverIp);
      Serial.print(":");
      Serial.println(serverPort);
      delay(10000); // Wait 5 seconds before trying again to avoid rapid retries
      return; // Skip the rest of the loop for this iteration
    }
  }

  // --- Send Data to Server ---
  // Check if it's time to send data
  unsigned long currentTime = millis(); // Get current time in milliseconds
  if (currentTime - lastSendTime >= sendInterval) {
    String message = "Hello from Arduino! Uptime: " + String(currentTime / 1000) + "s";
    Serial.print("Sending to server: ");
    Serial.println(message);
    client.print(message + "\n"); // Send the message, add newline for C# server parsing
    lastSendTime = currentTime; // Update the last send time
  }

  // --- Receive Data from Server ---
  // Check if there's any incoming data from the server
  if (client.available()) {
    String command = client.readStringUntil('\n'); // Read until newline character
    command.trim(); // Remove any leading/trailing whitespace

    Serial.print("Received command: ");
    Serial.println(command);

    // --- Command Parsing Logic ---
    if (command == "LIGHT_ON") {\
      LED_On = true;
      digitalWrite(LED_BUILTIN, LED_On); // Turn LED on
      Serial.println("LED turned ON");
      client.print("ACK:LIGHT_ON\n"); // Acknowledge command
    } else if (command == "LIGHT_OFF") {
      LED_On = false;
      digitalWrite(LED_BUILTIN, LED_On); // Turn LED off
      Serial.println("LED turned OFF");
      client.print("ACK:LIGHT_OFF\n"); // Acknowledge command
    } else if (command == "LIGHT_TOG"){
      LED_On = !LED_On;
      digitalWrite(LED_BUILTIN, LED_On);
      Serial.println("LED Toggled");
      client.print("ACK:LIGHT_TOG\n");
    } else if (command.startsWith("ACK:")) {
      Serial.print("Server Acknowledged: ");
      Serial.println(command.substring(4)); // Print the ACK message without "ACK:"
    }
    // Add more else if blocks for other commands (e.g., "READ_TEMP", "SET_SERVO:90")
  }

  delay(10); // Small delay to prevent watchdog timer resets on ESP boards
}