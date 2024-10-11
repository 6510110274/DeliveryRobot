#include <ESP8266WiFi.h>
#include <WiFiClient.h>

const char* ssid = "papun";           // เปลี่ยนเป็น SSID ของ Wi-Fi
const char* password = "6510110274";  // เปลี่ยนเป็นรหัสผ่าน Wi-Fi
const char* host = "esp32-server.local"; // ใส่ IP ของ PC ที่เป็น host
const uint16_t port = 80;           // พอร์ตที่ PC ใช้

WiFiClient client;

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
}

void connectToServer() {
  Serial.println("Connecting to PC server...");
  
  while (!client.connect(host,port)) {
    Serial.println("Connection to PC failed! Retrying...");
    delay(1000);
  }

  Serial.println("Connected to PC server!");
}

void setup() {
  Serial.begin(115200);  // Initialize UART for receiving data from another device (e.g., Arduino)
  connectToWiFi();       // Connect to Wi-Fi
  connectToServer();     // Connect to PC server
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost connection. Reconnecting...");
    connectToWiFi();
  }

  if (!client.connected()) {
    Serial.println("Disconnected from server! Reconnecting...");
    connectToServer();
  }

  // Accumulate the incoming data from Arduino until a newline is received
  static String usartData = "";

  // Check if there is data available from Arduino (UART)
  while (Serial.available()) {
    char received = Serial.read();
    usartData += received;

    // If we receive a newline, it's time to send the full data to the server
    if (received == '\n') {
      Serial.println("Received from Arduino: " + usartData);  // Debugging line

      // Send the data to the server
      client.print(usartData);  // Send the complete message including newline
      Serial.println("Sent to server: " + usartData);

      // Clear the buffer for the next message
      usartData = "";
    }
  }

  delay(200);  // Reduce the delay between loops for better performance
}
