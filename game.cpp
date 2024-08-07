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
#define DATABASE_URL "https://first-year-hardware-project.firebaseio.com" 
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

//WiFi LED
#define WiFi_LED 5

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

enum GameMode { Buz, Memory,dance, non };
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
const unsigned long presenceDuration = 1*60*1000; 
const unsigned long gameDuration = 3*60*1000; 
unsigned long lastHumanDetectionTime = 0;
const unsigned long detectionGracePeriod = 2000; 

int sessions = 0; //count of sesions (Websockets)
int numDetections = 0; // Counter for human presence detections
int numExecutedSessions = 0;
int i = 1;

bool changeState = false;
unsigned long lastDisplayUpdateTime = 0; // To track the last time the display was updated
const unsigned long displayUpdateInterval = 1000;
int gameRemainingTime = 0;

void setup() {
  //Buzzer
  pinMode(buzzer, OUTPUT);

  //BuzzGame
  pinMode(startpin, INPUT_PULLUP);
  pinMode(failpin, INPUT_PULLUP);
  pinMode(endpin, INPUT_PULLUP);

  pinMode(WiFi_LED,OUTPUT);
  
  //Memory Game  (Initialize button and LED pins)
  for (int i = 0; i < 4; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
    pinMode(leds[i], OUTPUT);
  }

  // LD2410
  MONITOR_SERIAL.begin(115200);
  RADAR_SERIAL.begin(256000, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  pinMode(LED_PIN, OUTPUT);
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

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  //WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print(" Not  Connected ");
    lcd.setCursor(-4,2);
    lcd.print("     To WiFi");
  }
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(" WiFi Connected ");

  digitalWrite(WiFi_LED, HIGH);

  Serial.println("Connected to WiFi");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());  // Print the IP address

  //Debug Red led in memory game
  pinMode(25, OUTPUT);

  //Websockets
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  //FireBase
  firebaseInit();

  //Time
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // Configure time service
  randomSeed(analogRead(0));  // Seed the random number generator

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Send session");
  lcd.setCursor(5, 1);
  lcd.print("count");
  lcd.setCursor(-3, 2);
  lcd.print("via the WebApp");

  while(sessions == 0){
    webSocket.loop();
    checkWiFi();
  }
  playBuzzer(4); // Buzzer for 1 second

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Push Blue Button");
  lcd.setCursor(0, 2);
  lcd.print("To Start    ");

  while (digitalRead(15) == HIGH) {
    // Wait for button press
  }

  lastHumanDetectionTime = millis();
 
}

void loop() {
  checkWiFi();
   
  if(sessions != 0){
  
    if(numExecutedSessions <= sessions){
      radar.read();
      if (radarConnected) {
        bool humanDetected = false;

        if (radar.presenceDetected()) {
          if ((radar.stationaryTargetDetected() && radar.stationaryTargetDistance() <= 150) || 
              (radar.movingTargetDetected() && radar.movingTargetDistance() <= 150)) {
            humanDetected = true;
            lastHumanDetectionTime = millis();
          }
        }

        if (humanDetected || (millis() - lastHumanDetectionTime <= detectionGracePeriod)) {
          digitalWrite(LED_PIN, HIGH); // Turn on the LED

          if (presenceStartTime == 0) {
            presenceStartTime = millis();
            numExecutedSessions++;
            Serial.println("Start Session");
            changeState = true;
                        
          } else if (millis() - presenceStartTime >= presenceDuration) { 
            digitalWrite(LED_PIN, LOW);
            Serial.println("Start Gaming");
            
            gameActive = true;
            presenceStartTime = 0; // Reset the presence start time
            gameStartTime = millis(); // Set the game start time

            numDetections++; // Increment detection counter
            
            // Update the display to indicate game selection is possible
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Choose a Game");
            lcd.setCursor(0, 1);
            lcd.print("Red :  Buzzwire");
            lcd.setCursor(-4, 2);
            lcd.print("Yellow : Memory");
            lcd.setCursor(-4, 3);
            lcd.print("Green : Dancing");
            
            // Sound the buzzer to indicate game selection is possible
            playBuzzer(4); // Buzzer for 1 second

            // Give a chance to play games
            while (gameActive) {
              checkWiFi();
              if (gamemode == non) {
                if (!digitalRead(12)) {
                  Serial.println("Selected BuzzGame");
                  gamemode = Buz;
                  lcd.clear();
                  lcd.setCursor(0, 1);
                  lcd.print("Selected Buzwire");
                  delay(1000);
                  lcd.clear();
                  lcd.setCursor(1, 1);
                  lcd.print("Touch start pin");
                  game1(); // Start Buz game
                } else if (!digitalRead(13)) {
                  Serial.println("Selected Memory");
                  gamemode = Memory;
                  lcd.clear();
                  lcd.setCursor(0, 1);
                  lcd.print("Selected Memory");
                  
                  delay(1000);
                  game2(); // Start Memory game
                } else if(!digitalRead(14)){
                  Serial.println("Selected Dancing Pad");
                  dancingPad();
                  gamemode = dance;
                  lcd.clear();
                  lcd.setCursor(0, 1);
                  lcd.print("Selected Dancing");
                }
              }
              if(millis() - gameStartTime >= gameDuration && changeState){
                stopGame();
              }
            }
          }else{
            timer();
          }
        } else {
            Serial.println("Not detecting");
            digitalWrite(LED_PIN, LOW); // Turn off the LED
            presenceStartTime = 0;
            gameActive = false;
            if(numExecutedSessions == sessions){
              stopSystem();
            }
            else{
              playBuzzer(4); // Buzzer for 1 second
              lcd.clear();
              lcd.setCursor(0, 1);
              lcd.print("No user detected");
              delay(2000);
              lcd.clear();
              lcd.setCursor(0, 1);
              lcd.print("Push Blue Button");
              lcd.setCursor(-4, 2);
              lcd.print("To Start Session");
              while(digitalRead(15) == HIGH){
              }
              lastHumanDetectionTime = millis();
            }
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
    firestoreDataUpdate();
    i++;
  }
  gameActive = false;
  tone(buzzer, 350);
  delay(1000);
  noTone(buzzer);
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Session is over");
  delay(2000);
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("Push Restart  ");
  lcd.setCursor(-2, 2);
  lcd.print("To start new");
  
  delay(2000); // Delay before resetting
  digitalWrite(LED_PIN, LOW);
  sessions = 0;
  level = 0;
  while(true){}
}

void stopGame() {
  gameActive = false;
  gamemode = non;
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("Time is OVER!");
  delay(1000); // Wait for 1 seconds before restarting presence check
  Serial.println("Time is over");
  if(numExecutedSessions >= sessions){
    stopSystem();
    return;
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  playBuzzer(4); // Buzzer for 1 second
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Push Blue Button");
  lcd.setCursor(-4, 2);
  lcd.print("To start session"); 

  while (digitalRead(15) == HIGH) {
    // Wait for button press
  }

  level = 1;
  stage = 0;
  lost = false;
  presenceStartTime = 0;
  changeState = false;

}

void game1() {
  while (gamemode == Buz) {
    checkWiFi();
    if (millis() - gameStartTime >= gameDuration) {
      stopGame();
      return;
    }
    switch (gamestate) {
      case IN_PROGRESS:
        if (!digitalRead(endpin)) {
          gamestate = SUCCESS;
          lcd.clear();
          lcd.setCursor(0, 1);
          playBuzzer(4);
          lcd.print("Congratulations!");
          Serial.println("Game over");
          delay(1000);
          gamemode = non;
          gamestate = FAILED;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Choose a Game");
          lcd.setCursor(0, 1);
          lcd.print("Red :  Buzzwire");
          lcd.setCursor(-4, 2);
          lcd.print("Yellow : Memory");
          lcd.setCursor(-4, 3);
          lcd.print("Green : Dancing");
          
        } else if (!digitalRead(failpin)) {
          gamestate = FAILED;
          lcd.clear();
          lcd.setCursor(4, 1);
          Serial.println("Game over Failed!");
          lcd.print("Failed!");
          tone(buzzer, 350);
          delay(1000);
          noTone(buzzer);
          gamemode = non;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Choose a Game");
          lcd.setCursor(0, 1);
          lcd.print("Red :  Buzzwire");
          lcd.setCursor(-4, 2);
          lcd.print("Yellow : Memory");
          lcd.setCursor(-4, 3);
          lcd.print("Green : Dancing");
        }
        break;

      case FAILED:
      case SUCCESS:
        if (!digitalRead(startpin)) {
          gamestate = IN_PROGRESS;
          lcd.clear();
          lcd.setCursor(0, 1);
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
  checkWiFi();
  while (gamemode == Memory) {
    if (millis() - gameStartTime >= gameDuration) {
      stopGame();
      return;
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
        lcd.setCursor(5, 0);
        lcd.print("Level ");
        lcd.print(level);

        lcd.setCursor(0, 1);
        lcd.print("--- Memorize ---");
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
        lcd.clear();
        lcd.setCursor(5, 0);
        lcd.print("Level ");
        lcd.print(level);
        lcd.setCursor(0, 1);
        lcd.print("----- Play -----");
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
        lcd.clear();
        lcd.setCursor(5, 0);
        lcd.print("Level ");
        lcd.print(level);
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
        Serial.println("lost Memory");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" !! You Lost !! ");
        tone(buzzer, 350);
        for (int i = 0; i < 4; i++) {
          digitalWrite(leds[i], HIGH);
        }
        delay(1000);
        lcd.setCursor(0, 1);
        lcd.print("    Score  ");
        lcd.print(level - 1);
        noTone(buzzer);
        delay(2000);
        for (int i = 0; i < 4; i++) {
          digitalWrite(leds[i], LOW);
        }
        
        level = 1;
        stage = 0;
        lost = false;
        gamemode = non;  // Reset game mode to allow new game selection
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Choose a Game");
        lcd.setCursor(0, 1);
        lcd.print("Red :  Buzzwire");
        lcd.setCursor(-4, 2);
        lcd.print("Yellow : Memory");
        lcd.setCursor(-4, 3);
        lcd.print("Green : Dancing");
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
  return;
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

void firestoreDataUpdate() {
  if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
    String collectionPath = "sessions";

    FirebaseJson content;
    content.set("fields/date/stringValue", getFormattedDate());
    content.set("fields/numDetections/stringValue", String(numDetections));

    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", collectionPath.c_str(), content.raw())) {
      Serial.printf("Document created: %s\n\n", fbdo.payload().c_str());
    } else {
      Serial.println(fbdo.errorReason());
    }
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

//Timer
void timer(){
  if (millis() - lastDisplayUpdateTime >= displayUpdateInterval){
    unsigned long remainingTime = presenceDuration - (millis() - presenceStartTime);
    unsigned long seconds = (remainingTime / 1000) % 60;
    unsigned long minutes = (remainingTime / 60000) % 60;

    lcd.clear();
    lcd.setCursor(3, 1);
    lcd.print("Time Left:");
    lcd.setCursor(-1, 2);
    lcd.print(minutes);
    lcd.print(":");
    if (seconds < 10) {
      lcd.print("0");
    }
    lcd.print(seconds);

    lastDisplayUpdateTime = millis();
  }
}

void dancingPad(){
  gameRemainingTime = millis() - gameStartTime;
  gameRemainingTime = gameDuration - gameRemainingTime;
  String gameRemainingTimeStr = String(gameRemainingTime);
  // Send the String via webSocket
  webSocket.sendTXT(0, gameRemainingTimeStr); // Send response "0" to React app
  if (millis() - gameStartTime >= gameDuration) {
      stopGame();
      return; // Exit the function
    }
 
  return;
  
}

void checkWiFi(){
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    digitalWrite(WiFi_LED, LOW); // Turn off the LED if not connected
  } else {
    digitalWrite(WiFi_LED, HIGH); // Keep the LED on if connected
  }
}




 
            
