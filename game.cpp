#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ld2410.h>

LiquidCrystal_I2C lcd(0x27, 16, 4);  // Address 0x27, 16 columns, 4 rows


//Buzwire Game
const int startpin = 32;
const int failpin = 4;
const int endpin = 33;

enum GameState { FAILED, IN_PROGRESS, SUCCESS };
GameState gamestate = FAILED;

//Choose Game
enum GameMode { Buz, Simon, non };
GameMode gamemode = non;

//Simon Says Game
int buttons[4] = {12, 13, 14, 15};  // Update with suitable GPIO pins for ESP32
int leds[4]    = {25, 26, 18, 19};  // Update with suitable GPIO pins for ESP32

boolean button[4] = {0, 0, 0, 0};

#define levelsInGame 50
#define buzzer  23// Update with a suitable GPIO pin for ESP32

int bt_simonSaid[100];
int led_simonSaid[100];

boolean lost = false;
int game_play, level, stage;

//-------------------------------------------------------------------
//LD2410
ld2410 radar;
uint32_t lastReading = 0;
bool radarConnected = false;

#define MONITOR_SERIAL Serial
#define RADAR_SERIAL Serial1
#define RADAR_RX_PIN 16
#define RADAR_TX_PIN 17
#define LED_PIN 2  // Define the pin for the LED

unsigned long presenceStartTime = 0;
unsigned long gameStartTime = 0;
bool gameActive = false;
const unsigned long presenceDuration = 15000; // 15 seconds in milliseconds
const unsigned long gameDuration = 20000; // 20 seconds in milliseconds

//------------------------------------------------------------------

void setup() {
  pinMode(startpin, INPUT_PULLUP);
  pinMode(failpin, INPUT_PULLUP);
  pinMode(endpin, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);

  // Initialize LCD
  lcd.init();
  
  // Turn on the backlight
  lcd.backlight();

  Serial.begin(9600);  // Initialize Serial Monitor
  // Initialize button and LED pins
  for (int i = 0; i < 4; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
    pinMode(leds[i], OUTPUT);
  }
  
  randomSeed(analogRead(0));  // Seed the random number generator

  //----------------------------------------------------------------------
  //LD2410
   // Initialize radar
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
 
//----------------------------------------------------------------------
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Start");
}

void loop() {
   // Read data from the radar
  radar.read();

  if (radarConnected) {
    bool humanDetected = false;

    if (radar.presenceDetected()) {
      if ((radar.stationaryTargetDetected() && radar.stationaryTargetDistance() <= 100) || 
          (radar.movingTargetDetected() && radar.movingTargetDistance() <= 100)) {
        humanDetected = true;
      }
    }

    if (humanDetected) {
      digitalWrite(LED_PIN, HIGH); // Turn on the LED

      if (presenceStartTime == 0) {
        presenceStartTime = millis();
      } else if (millis() - presenceStartTime >= presenceDuration) { // Detected for 15 seconds
        gameActive = true;
        presenceStartTime = 0; // Reset the presence start time
        gameStartTime = millis(); // Set the game start time

        // Update the display to indicate game selection is possible
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Choose a Game:");
        lcd.setCursor(0, 1);
        lcd.print("1: Buz 2: Simon");
        
        // Sound the buzzer to indicate game selection is possible
        playBuzzer(4); // Buzzer for 1 second
      }
    } else {
      digitalWrite(LED_PIN, LOW); // Turn off the LED
      presenceStartTime = 0;
      gameActive = false;
    }
  }

  if (gameActive) {
    if (millis() - gameStartTime >= gameDuration) {
      gameActive = false;
      gamemode = non;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Start");
    } else {
      switch (gamemode) {
        case non:
          if (!digitalRead(startpin)) {
            gamemode = Buz;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Buz_Game");
            delay(1000);
          } else if (!digitalRead(endpin)) {
            gamemode = Simon;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Simon_Game");
            delay(1000);
          }
          break;
        case Buz:
          game1();
          break;
        case Simon:
          game2();
          break;
      }
    }
  }
}

void game1() {
  while (gamemode == Buz) {
    switch (gamestate) {
      case IN_PROGRESS:
        if (!digitalRead(endpin)) {
          gamestate = SUCCESS;
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
  while (gamemode == Simon) {
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
        led_simonSaid[level] = leds[random(0, 4)];
        for (int i = 1; i <= level; i++) {
          digitalWrite(led_simonSaid[i], HIGH);
          playBuzzer(led_simonSaid[i] -15);
          digitalWrite(led_simonSaid[i], LOW);
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
            bt_simonSaid[game_play] = leds[i];
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
          if (led_simonSaid[i] != bt_simonSaid[i]) {
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

