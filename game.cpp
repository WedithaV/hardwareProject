#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 4);  // Address 0x27, 16 columns, 4 rows

const int startpin = 32;
const int failpin = 4;
const int endpin = 33;

enum GameState { FAILED, IN_PROGRESS, SUCCESS };
GameState gamestate = FAILED;

enum GameMode { Buz, Simon, non };
GameMode gamemode = non;

int buttons[4] = {12, 13, 14, 15};  // Update with suitable GPIO pins for ESP32
int leds[4]    = {25, 26, 18, 19};  // Update with suitable GPIO pins for ESP32

boolean button[4] = {0, 0, 0, 0};

#define levelsInGame 50

int bt_simonSaid[100];
int led_simonSaid[100];

boolean lost = false;
int game_play, level, stage;

void setup() {
  pinMode(startpin, INPUT_PULLUP);
  pinMode(failpin, INPUT_PULLUP);
  pinMode(endpin, INPUT_PULLUP);

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
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Start");
}

void loop() {
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

void game1() {
  for (int i = 0; i < 1000; i++) {
    switch (gamestate) {
      case IN_PROGRESS:
        if (!digitalRead(endpin)) {
          gamestate = SUCCESS;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Congratulations!");
          Serial.println("Game over Congratulations!");
          delay(1000);
          gamemode = non;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Start");
          break;
        } else if (!digitalRead(failpin)) {
          gamestate = FAILED;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Failed!");
          Serial.println("Game over Failed!");
          delay(1000);
          gamemode = non;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Start");
          break;
        }
        break;

      case FAILED:
      case SUCCESS:
        if (!digitalRead(startpin)) {
          gamestate = IN_PROGRESS;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("New game start!");
          Serial.println("New game started.");
        }
        break;
    }
  }
}

void game2() {
  for (int i = 0; i < 1000; i++) {
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
          delay(400);
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
        for (int i = 0; i < 4; i++) {
          digitalWrite(leds[i], HIGH);
        }
        delay(1000);
        lcd.setCursor(0, 1);
        lcd.print("   Score  ");
        lcd.print(level - 1);
        delay(3000);
        for (int i = 0; i < 4; i++) {
          digitalWrite(leds[i], LOW);
        }
        level = 1;
        stage = 0;
        lost = false;
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
