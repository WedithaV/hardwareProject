#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Adjust the LCD address if needed
LiquidCrystal_I2C lcd(0x27, 16, 2);

int buttons[4] = {12, 13, 14, 15}; // Update with suitable GPIO pins for ESP32
int leds[4]    = {25, 26, 18, 19}; // Update with suitable GPIO pins for ESP32

boolean button[4] = {0, 0, 0, 0};

#define buzzer  4// Update with a suitable GPIO pin for ESP32

#define levelsInGame 50

int bt_simonSaid[100];
int led_simonSaid[100];

boolean lost;
int game_play, level, stage;

void setup() {
  Serial.begin(9600);

  // Initialize button and LED pins
  for (int i = 0; i <= 3; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
    pinMode(leds[i], OUTPUT);
  }

  pinMode(buzzer, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("   Welcome To   ");
  lcd.setCursor(0, 1);
  lcd.print("> Memory  Game <");
  delay(1000);
  lcd.clear();

  randomSeed(analogRead(0));  // Seed the random number generator
}

void loop() {
  switch (stage) {
    case 0:
      lcd.setCursor(0, 0); lcd.print("Press Red Button");
      lcd.setCursor(0, 1); lcd.print(" for Start Game ");
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
      lcd.print("Level: ");
      lcd.print((level / 10) % 10);
      lcd.print(level % 10);

      lcd.setCursor(0, 1);
      lcd.print(" -- Memorize -- ");
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
      lcd.print("   -- Play --   ");
      break;

    case 3:
      for (int i = 0; i <= 3; i++) {
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
          lost = 1;
          break;
        }
      }
      if (lost == 1) stage = 5;
      else stage = 6;
      break;

    case 5:
      lcd.setCursor(0, 0);
      lcd.print(" !! You Lost !! ");
      tone(buzzer, 350);
      for (int i = 0; i <= 3; i++) {
        digitalWrite(leds[i], HIGH);
      }
      delay(1000);
      lcd.setCursor(0, 1);
      lcd.print("   Score : ");
      lcd.print(((level-1) / 10) % 10);
      lcd.print((level-1) % 10);
      lcd.print("   ");
      noTone(buzzer);
      delay(3000);
      for (int i = 0; i <= 3; i++) {
        digitalWrite(leds[i], LOW);
      }
      level = 1;
      stage = 0;
      lost = 0;
      break;

    case 6:
      lcd.setCursor(0, 1);
      lcd.print(" ** You  Win ** ");
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

void playBuzzer(int x) {
  tone(buzzer, 650 + (x * 100));
  delay(300);
  noTone(buzzer);
}
