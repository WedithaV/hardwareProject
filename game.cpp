#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ld2410.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

//Firebase
#define API_KEY "AIzaSyAxgTKliDO4MNop_gZEH_led3kpI2MlfBs"
#define FIREBASE_PROJECT_ID "first-year-hardware-project"
#define USER_EMAIL "your_email@example.com"
#define USER_PASSWORD "your_password"
#define DATABASE_URL "https://first-year-hardware-project.firebaseio.com" // Add your Firebase database URL
#define RDATABASE_URL "https://first-year-hardware-project-default-rtdb.asia-southeast1.firebasedatabase.app/"

//LD2410
#define MONITOR_SERIAL Serial
#define RADAR_SERIAL Serial1
#define RADAR_RX_PIN 16
#define RADAR_TX_PIN 17
#define LED_PIN 27 

//Memory game
#define levelsInGame 50
#define buzzer  23


LiquidCrystal_I2C lcd(0x27, 16, 4);  // Address 0x27, 16 columns, 4 rows

// Initialize Firebase data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

//WebSockets
WebSocketsServer webSocket = WebSocketsServer(81);
const char* ssid = "Dialog 4G 370";
const char* password = "Fc5814ED";

// Buzwire Game
const int startpin = 32;
const int failpin = 4;
const int endpin = 33;
enum GameState { FAILED, IN_PROGRESS, SUCCESS };
GameState gamestate = FAILED;
unsigned long startTime = 0; // Variable to store the start time
unsigned long highScore = ULONG_MAX; // Variable to store the high score time

// Choose Game
enum GameMode { Buz, Memory, non };
GameMode gamemode = non;

// Memory Game
int buttons[4] = {12, 13, 14, 15};
int leds[4]    = {25, 26, 18, 19};
boolean button[4] = {0, 0, 0, 0};
int bt_memoryGame[100];
int led_memoryGame[100];
boolean lost = false;
int game_play, level, stage;

// LD2410
ld2410 radar;
uint32_t lastReading = 0;
bool radarConnected = false;

unsigned long presenceStartTime = 0;
unsigned long gameStartTime = 0;
bool gameActive = false;
const unsigned long presenceDuration = 5000; // 5 seconds in milliseconds
const unsigned long gameDuration = 30000; // 20 seconds in milliseconds
unsigned long lastHumanDetectionTime = 0;
const unsigned long detectionGracePeriod = 2000; // 2 seconds in milliseconds

int sessions = 0; //count of sesions (Websockets)
int numDetections = 0; // Counter for human presence detections
unsigned long memoryHighScore = ULONG_MAX;
int i = 1;


//------------------------------------------------------------------
void setup() {
  //Buzzer
  pinMode(buzzer, OUTPUT);

  //BuzzGame
  pinMode(startpin, INPUT_PULLUP);
  pinMode(failpin, INPUT_PULLUP);
  pinMode(endpin, INPUT_PULLUP);
  
  //Memory Game  (Initialize button and LED pins)
  for (int i = 0; i < 4; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
    pinMode(leds[i], OUTPUT);
  }

  // LD2410
  MONITOR_SERIAL.begin(115200);
  RADAR_SERIAL.begin(256000, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  pinMode(LED_PIN, OUTPUT);
  pinMode(buzzer, OUTPUT); // Initialize the buzzer pin as output
  digitalWrite(LED_PIN, LOW); // Turn off the LED initially
  digitalWrite(buzzer, LOW); // Turn off the buzzer initially
  delay(500);

  MONITOR_SERIAL.print(F("Initializing LD2410 radar sensor: "));
  if (radar.begin(RADAR_SERIAL)) {
    MONITOR_SERIAL.println(F("OK"));
    radarConnected = true;
  } else {
    MONITOR_SERIAL.println(F("not connected"));
  }

  //WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());  // Print the IP address

  //Websockets
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  //FireBase
  firebaseInit();
  rfirebase();

  //Time
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // Configure time service
  randomSeed(analogRead(0));  // Seed the random number generator

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wait...!");
}

void loop() {
  webSocket.loop();

  if(sessions != 0){
    if(numDetections <= sessions){
    radar.read();

      if (radarConnected) {
        bool humanDetected = false;

        if (radar.presenceDetected()) {
          if ((radar.stationaryTargetDetected() && radar.stationaryTargetDistance() <= 100) || 
              (radar.movingTargetDetected() && radar.movingTargetDistance() <= 100)) {
            humanDetected = true;
            lastHumanDetectionTime = millis();
          }
        }

        if (humanDetected || (millis() - lastHumanDetectionTime <= detectionGracePeriod)) {
          digitalWrite(LED_PIN, HIGH); // Turn on the LED

          if (presenceStartTime == 0) {
            presenceStartTime = millis();
          } else if (millis() - presenceStartTime >= presenceDuration) { // Detected for presenceDuration
            gameActive = true;
            presenceStartTime = 0; // Reset the presence start time
            gameStartTime = millis(); // Set the game start time

            numDetections++; // Increment detection counter
            
            

            // Stop detecting after three detections
            if (numDetections > sessions) {
              stopSystem();
              return;
            }

            // Update the display to indicate game selection is possible
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Choose a Game:");
            lcd.setCursor(0, 1);
            lcd.print("1: Buz 2: Simon");
            
            // Sound the buzzer to indicate game selection is possible
            playBuzzer(4); // Buzzer for 1 second

            // Give a chance to play games
            while (gameActive) {
              if (gamemode == non) {
                if (!digitalRead(startpin)) {
                  gamemode = Buz;
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Buz_Game");
                  delay(1000);
                  game1(); // Start Buz game
                } else if (!digitalRead(endpin)) {
                  gamemode = Memory;
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  lcd.print("Memory_Game");
                  delay(1000);
                  game2(); // Start Memory game
                }
              }

              // Check game timeout
              if (millis() - gameStartTime >= gameDuration) {
                stopGame();
              }
            }
          }
        } else {
            digitalWrite(LED_PIN, LOW); // Turn off the LED
            presenceStartTime = 0;
            gameActive = false;
        }
      }
    } 
    else {
      stopSystem();
    }
  }
}

void stopSystem() {
  if(i == 1){
    numDetections--;
    firestoreDataUpdate();
    i++;
  }
  gameActive = false;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Session is over");
  delay(2000); // Delay before resetting
  lcd.clear();
  
}

void stopGame() {
  gameActive = false;
  gamemode = non;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time is OVER!");
  delay(500); // Wait for 2 seconds before restarting presence check
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wait...!");
  if(level - 1 > memoryHighScore){
      memoryHighScore = level - 1;
      updateMemoryHighScore(memoryHighScore);
    }
  level = 1;
  stage = 0;
  lost = false;
  presenceStartTime = 0; // Reset presence check after game time is over
}

void game1() {
  while (gamemode == Buz) {
    if (millis() - gameStartTime >= gameDuration) {
      stopGame();
      return; // Exit the function
    }
    switch (gamestate) {
      case IN_PROGRESS:
        if (!digitalRead(endpin)) {
          gamestate = SUCCESS;
          unsigned long endTime = millis();
          unsigned long elapsedTime = endTime - startTime;
          if (elapsedTime < highScore) {
          highScore = elapsedTime;
          updateBuzzwireHighScore(highScore); // Update the high score in Firebase
          }
          lcd.clear();
          lcd.setCursor(0, 0);
          playBuzzer(4);
          lcd.print("Congratulations!");
          Serial.println("Game over Congratulations!");
          delay(1000);
          gamemode = non;
          gamestate = FAILED;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Start");
        } else if (!digitalRead(failpin)) {
          gamestate = FAILED;
          lcd.clear();
          lcd.setCursor(0, 0);
          Serial.println("Game over Failed!");
          lcd.print("Failed!");
          tone(buzzer, 350);
          delay(1000);
          noTone(buzzer);
          gamemode = non;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Start");
        }
        break;

      case FAILED:
      case SUCCESS:
        if (!digitalRead(startpin)) {
          gamestate = IN_PROGRESS;
          startTime = millis(); // Record the start time
          lcd.clear();
          lcd.setCursor(0, 0);
          tone(buzzer, 150);
          delay(300);
          noTone(buzzer);
          lcd.print("New game start!");
          Serial.println("New game started.");
        }
        break;
    }
  }
}

void game2() {
  while (gamemode == Memory) {
    if (millis() - gameStartTime >= gameDuration) {
      stopGame();
      return; // Exit the function
    }
    switch (stage) {
      case 0:
        lcd.setCursor(0, 0); 
        lcd.print("Press Red Button");
        lcd.setCursor(0, 1); 
        lcd.print(" for Start Game ");
        button[0] = digitalRead(buttons[0]);
        while (button[0] == HIGH) {
          button[0] = digitalRead(buttons[0]);
        }
        playBuzzer(25);
        level = 1;
        stage = 1;
        game_play = 1;
        break;

      case 1:
        lcd.clear();
        lcd.setCursor(4, 0);
        lcd.print("Level ");
        lcd.print(level);

        lcd.setCursor(0, 1);
        lcd.print("-- Memorize --");
        delay(1500);
        led_memoryGame[level] = leds[random(0, 4)];
        for (int i = 1; i <= level; i++) {
          digitalWrite(led_memoryGame[i], HIGH);
          playBuzzer(led_memoryGame[i] - 15);
          digitalWrite(led_memoryGame[i], LOW);
          delay(400);
        }
        delay(500);
        stage = 2;
        break;

      case 2:
        stage = 3;
        lcd.setCursor(0, 1);
        lcd.print("-- Play --");
        break;

      case 3:
        for (int i = 0; i < 4; i++) {
          button[i] = digitalRead(buttons[i]);
          if (button[i] == LOW) {
            bt_memoryGame[game_play] = leds[i];
            digitalWrite(leds[i], HIGH);
            playBuzzer(i + 1);
            while (button[i] == LOW) {
              button[i] = digitalRead(buttons[i]);
            }
            delay(50);
            digitalWrite(leds[i], LOW);
            game_play++;
            if (game_play - 1 == level) {
              game_play = 1;
              stage = 4;
              break;
            }
          }
        }
        delay(10);
        break;

      case 4:
        lcd.setCursor(0, 1);
        lcd.print("  Verification  ");
        delay(1000);
        for (int i = 1; i <= level; i++) {
          if (led_memoryGame[i] != bt_memoryGame[i]) {
            lost = true;
            break;
          }
        }
        if (lost) stage = 5;
        else stage = 6;
        break;

      case 5:
        lcd.setCursor(0, 0);
        lcd.print("!! You Lost !!");
        tone(buzzer, 350);
        for (int i = 0; i < 4; i++) {
          digitalWrite(leds[i], HIGH);
        }
        delay(1000);
        lcd.setCursor(0, 1);
        lcd.print("   Score  ");
        lcd.print(level - 1);
        noTone(buzzer);
        delay(3000);
        for (int i = 0; i < 4; i++) {
          digitalWrite(leds[i], LOW);
        }
        if(level - 1 > memoryHighScore){
          memoryHighScore = level - 1;
          updateMemoryHighScore(memoryHighScore);
        }
        level = 1;
        stage = 0;
        lost = false;
        gamemode = non;  // Reset game mode to allow new game selection
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Start");
        return; // Exit game2 function to allow game mode selection
        break;

      case 6:
        lcd.setCursor(0, 1);
        lcd.print("  You  Win  ");
        delay(1000);
        if (level == levelsInGame) {
          lcd.setCursor(0, 0);
          lcd.print("Congratulation");
          lcd.setCursor(0, 1);
          lcd.print(" Level Complete");
          delay(1000);
          lcd.clear();
          level = 1;
        } else {
          if (level < levelsInGame) level++;
        }
        stage = 1;
        break;

      default:
        break;
    }
  }
}

void playBuzzer(int x) {
  tone(buzzer, 650 + (x * 100));
  delay(300);
  noTone(buzzer);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    sessions = atoi((char*)payload);
    Serial.print("Received sessions: ");
    Serial.println(sessions);
  }
}


//FireBase
void FirestoreTokenStatusCallback(TokenInfo info) {
  Serial.printf("Token Info: type = %s, status = %s\n", getTokenType(info), getTokenStatus(info));
}

void firebaseInit() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = FirestoreTokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void rfirebase(){
  config.host = RDATABASE_URL;
  config.api_key = API_KEY;
   /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  //initialize
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  // Retrieve the current high score
  if (Firebase.RTDB.getInt(&fbdo, "highScore/buzzwire")) {
    if (fbdo.dataType() == "int") {
      highScore = fbdo.intData();
      Serial.println("Current high score: " + String(highScore) + " ms");
    }
  } else {
    Serial.print("Failed to get high score: ");
    Serial.println(fbdo.errorReason());
  }
   if (Firebase.RTDB.getInt(&fbdo, "highScore/memory")) {
    if (fbdo.dataType() == "int") {
      memoryHighScore = fbdo.intData();
      Serial.println("Current high score: " + String(memoryHighScore));
    }
  } else {
    Serial.print("Failed to get high score: ");
    Serial.println(fbdo.errorReason());
  }
}

void firestoreDataUpdate() {
  if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
    String collectionPath = "sessions";

    FirebaseJson content;
    content.set("fields/date/stringValue", getFormattedDate());
    content.set("fields/session/stringValue", String(numDetections)); // Convert numDetections to String

    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", collectionPath.c_str(), content.raw())) {
      Serial.printf("Document created: %s\n\n", fbdo.payload().c_str());
    } else {
      Serial.println(fbdo.errorReason());
    }
  }
}

void updateBuzzwireHighScore(unsigned long newHighScore) {
  Firebase.RTDB.setInt(&fbdo, "highScore/buzzwire", newHighScore);
  if (fbdo.dataType() == "null") {
    Serial.print("Failed to update high score: ");
    Serial.println(fbdo.errorReason());
  } else {
    Serial.println("Updated high score to: " + String(newHighScore) + " ms");
  }
}

void updateMemoryHighScore(unsigned long memoryNewHighScore) {
  Firebase.RTDB.setInt(&fbdo, "highScore/memory", memoryNewHighScore);
  if (fbdo.dataType() == "null") {
    Serial.print("Failed to update high score: ");
    Serial.println(fbdo.errorReason());
  } else {
    Serial.println("Updated high score to: " + String(memoryNewHighScore) + " ms");
  }
}


//Get Date
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