#include <WiFi.h>
#include <WebSocketsServer.h>

const char* ssid = "Dialog 4G 370";
const char* password = "Fc5814ED";

WebSocketsServer webSocket = WebSocketsServer(81);

const int sensorPins[4] = {13, 14, 12, 18}; // Pins for touch sensors

int receivedInt = -1; // Initialize received integer to an invalid value
int lastActivatedSensor = -1; // To keep track of last activated sensor

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // Set sensor pins as inputs
  for (int i = 0; i < 4; i++) {
    pinMode(sensorPins[i], INPUT);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());  // Print the IP address

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();

  // Check for sensor activation
  for (int i = 0; i < 4; i++) {
    int sensorState = digitalRead(sensorPins[i]);
    if (sensorState == HIGH && lastActivatedSensor != i) {
      lastActivatedSensor = i;
      Serial.print("Sensor ");
      Serial.print(i);
      Serial.println(" activated.");

      // Check if received integer matches the activated sensor index
      if (receivedInt == i) {
        Serial.println("Correct tile activated.");
        webSocket.sendTXT(0, "0"); // Send response "0" to React app
      } else {
        Serial.println("Incorrect tile activated. Waiting for correct tile.");
      }
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    receivedInt = atoi((char*)payload);
    Serial.print("Received integer: ");
    Serial.println(receivedInt);
  }
}
