#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"

// Replace these with your WiFi credentials
const char* ssid = "Dialog 4G 370";
const char* password = "Fc5814ED";

// Replace these with your Firebase project credentials
#define API_KEY "AIzaSyAxgTKliDO4MNop_gZEH_led3kpI2MlfBs"
#define FIREBASE_PROJECT_ID "first-year-hardware-project"
#define USER_EMAIL "your_email@example.com"
#define USER_PASSWORD "your_password"
#define DATABASE_URL "https://first-year-hardware-project.firebaseio.com" // Add your Firebase database URL

// Initialize Firebase data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void FirestoreTokenStatusCallback(TokenInfo info) {
  Serial.printf("Token Info: type = %s, status = %s\n", getTokenType(info), getTokenStatus(info));
}

void firebaseInit() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL; // Set the database URL
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = FirestoreTokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

String getFormattedDate() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "";
  }
  char buffer[11];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

String getFormattedTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "";
  }
  char buffer[9];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

void firestoreDataUpdate() {
  if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
    String collectionPath = "time";

    FirebaseJson content;
    content.set("fields/date/stringValue", getFormattedDate());
    content.set("fields/time/stringValue", getFormattedTime());

    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", collectionPath.c_str(), content.raw())) {
      Serial.printf("Document created: %s\n\n", fbdo.payload().c_str());
    } else {
      Serial.println(fbdo.errorReason());
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // Configure time service
  firebaseInit();
}

void loop() {
  firestoreDataUpdate();
  delay(60000); // Delay for a minute before next reading
}
