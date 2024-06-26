#include <WiFi.h>
#include <WebSocketsServer.h>

const char* ssid = "Dialog 4G 370";
const char* password = "Fc5814ED";

WebSocketsServer webSocket = WebSocketsServer(81);

const int sensorPins[4] = {13, 14, 12, 18}; // Pins for touch sensors
boolean sensorStates[4] = {false, false, false, false}; // Array of sensor states
const char* directions[4] = {"LEFT", "UP", "RIGHT", "DOWN"}; // Corresponding directions

int receivedInt = -1; // Initialize received integer to an invalid value

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
    int sensorValue = digitalRead(sensorPins[i]);
    if (sensorValue == HIGH) {
      sensorStates[i] = true;
      Serial.print("Sensor ");
      Serial.print(i);
      Serial.println(" activated.");
    } else {
      if (sensorStates[i] && sensorValue == LOW) {
        sensorStates[i] = false;
        Serial.println(directions[i]);
        
        if (receivedInt == i) {
          Serial.println("Correct tile activated.");
          webSocket.sendTXT(0, "0"); // Send response "0" to React app
        } else {
          Serial.println("Incorrect tile activated. Waiting for correct tile.");
        }
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
