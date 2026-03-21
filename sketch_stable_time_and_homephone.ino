// Библиотеки для работы с RFID RC522 и шиной SPI
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <IRremote.h>
#include <DHT.h>

// --- ПИНЫ ---
#define RST_PIN 9    // Пин сброса RC522 (Reset)
#define SS_PIN 10    // Пин выбора ведомого RC522 (SDA/CS)

#define LAMP_PIN 7   // Пин для лампы/светодиода
#define BUZZER_PIN 3 // Пин зуммера

#define DHTPIN A1 
#define DHTTYPE DHT11 

#define WATER_SENSOR A0 

#define DO 4

#define JOY_X A3
#define JOY_Y A2
#define JOY_SW 2

// Прототипы игр
void runSnake();
void runPong();
void runGuess();
void runReaction();
void updateSnake();
void drawSnake();
void initSnake();
void spawnFood();

IRrecv irrecv(8);
decode_results results;

Servo myServo;

LiquidCrystal_I2C lcd(0x27, 16, 2);

DHT dht(DHTPIN, DHTTYPE);

int water;

unsigned long denyTimerStart = 0;
bool denyActive = false;

bool sound;
bool light;

// --- АВТОРИЗОВАННЫЙ UID ---
byte AUTHORIZED_UID[] = {0xA4, 0xF7, 0x33, 0x06}; 

MFRC522 mfrc522(SS_PIN, RST_PIN);  

bool compareUids(byte *uid1, byte *uid2) {
  for (byte i = 0; i < 4; i++) {
    if (uid1[i] != uid2[i]) return false; 
  }
  return true; 
}

// --- ДОБАВЛЕНО ДЛЯ millis ---
bool doorOpen = false;
unsigned long doorTimer = 0;

void openDoor() {
  digitalWrite(LAMP_PIN, HIGH);
  tone(BUZZER_PIN, 1000);
  myServo.write(0); 
  doorOpen = true;
  doorTimer = millis();

  lcd.init();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("ACCESS");
  lcd.setCursor(0,1);
  lcd.print("GRANTED");
}

void setup() {
  myServo.attach(5); 
  IrReceiver.begin(8, ENABLE_LED_FEEDBACK);

  SPI.begin();          
  mfrc522.PCD_Init(); 

  dht.begin();

  irrecv.enableIRIn();

  pinMode(LAMP_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(WATER_SENSOR, INPUT);
  pinMode(JOY_SW, INPUT_PULLUP);

  digitalWrite(LAMP_PIN, LOW);
  noTone(BUZZER_PIN);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Hello to Secured");   
  lcd.setCursor(0,1);
  lcd.print("House!");

  light = false;
}

bool showTime = false; 
bool showTemperature = false; 
bool showWater = false; 

unsigned long lastUpdate = 0;

void loop() {
  // --- ПУЛЬТ ---
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;

    if (cmd == 0xC) { showTime = true; showTemperature = false; showWater = false; lcd.clear(); }
    if (cmd == 0x18){ showTemperature = true; showTime = false; showWater = false; lcd.clear(); }
    if (cmd == 0x5E){ showTemperature = false; showTime = false; showWater = true; lcd.clear(); }

    if (cmd == 0x8) runSnake();
    if (cmd == 0x1C) runPong();
    if (cmd == 0x5A) runGuess();
    if (cmd == 0x42) runReaction();
    IrReceiver.resume();
  }

  // --- ПОКАЗ ТЕМПЕРАТУРЫ ---
  if (showTemperature && millis() - lastUpdate >= 2000) {
    lastUpdate = millis();
    float tC = dht.readTemperature();
    float tF = dht.readTemperature(true);
    float h = dht.readHumidity();

    lcd.setCursor(0,0);
    lcd.print("C:");
    lcd.print(tC);
    lcd.print(" F:");
    lcd.print(tF);

    lcd.setCursor(0,1);
    lcd.print("Hum:");
    lcd.print(h);
    lcd.print("% ");
  }

  // --- ПОКАЗ ВОДЫ ---
  if (showWater && millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    water = analogRead(WATER_SENSOR);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Water level:");
    lcd.print(water);

    lcd.setCursor(0,1);
    if (water > 650) { lcd.print("Much water   "); tone(BUZZER_PIN, 500); digitalWrite(LAMP_PIN, HIGH); } 
    else if (water > 0) { lcd.print("Not much water"); noTone(BUZZER_PIN); digitalWrite(LAMP_PIN, LOW); } 
    else { lcd.print("No water      "); noTone(BUZZER_PIN); digitalWrite(LAMP_PIN, LOW); }
  }
  // --- RFID ---
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    if (compareUids(mfrc522.uid.uidByte, AUTHORIZED_UID)) openDoor();
    else {
      tone(BUZZER_PIN, 300, 500);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ACCESS");
      lcd.setCursor(0,1);
      lcd.print("DENIED");
      denyTimerStart = millis();
      denyActive = true;
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  // --- Проверка таймера Access Denied ---
  if (denyActive && millis() - denyTimerStart >= 2000) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Hello to Secured");
    lcd.setCursor(0,1);
    lcd.print("House!");
    denyActive = false;
  }

  // --- Таймер закрытия двери ---
  if (doorOpen && millis() - doorTimer >= 5000) {
    digitalWrite(LAMP_PIN, LOW);
    noTone(BUZZER_PIN);
    myServo.write(90);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Hello to Secured");
    lcd.setCursor(0,1);
    lcd.print("House!");
    doorOpen = false;
  }

  // Читаем микрофон
  sound = digitalRead(DO);
  static unsigned long lastClick = 0;
  
  if (sound == HIGH && !light && !doorOpen && (millis() - lastClick > 300)) { 
    light = true;    
    digitalWrite(LAMP_PIN, HIGH);
  }
  else if (sound == HIGH && light && !doorOpen) {
    light = false;
    digitalWrite(LAMP_PIN, LOW);
  }
}
// Кастомные символы
byte snakeBody[8] = {0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111};
byte food[8] = {0b00000,0b01110,0b11111,0b11111,0b11111,0b01110,0b00000,0b00000};

#define MAX_LEN 16
int snakeX[MAX_LEN], snakeY[MAX_LEN];
int snakeLen = 3;
int dirX = 1, dirY = 0;
int foodX, foodY;
bool gameOver = false;

void spawnFood() {
  foodX = random(0, 16);
  foodY = random(0, 2);
}

void initSnake() {
  snakeLen = 3;
  dirX = 1; dirY = 0;
  gameOver = false;
  for (int i = 0; i < snakeLen; i++) {
    snakeX[i] = 2 - i;
    snakeY[i] = 0;
  }
  spawnFood();
}

void drawSnake() {
  lcd.clear();
  for (int i = 0; i < snakeLen; i++) {
    lcd.setCursor(snakeX[i], snakeY[i]);
    lcd.write(byte(0));
  }
  lcd.setCursor(foodX, foodY);
  lcd.write(byte(1));
}

void updateSnake() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);

  if (x < 400 && dirX == 0) { dirX = 1; dirY = 0; }  // влево
  if (x > 600 && dirX == 0) { dirX = -1;  dirY = 0; }  // вправо
  if (y > 600 && dirY == 0) { dirX = 0;  dirY = 1; }  // вниз
  if (y < 400 && dirY == 0) { dirX = 0;  dirY = -1; } // вверх

  int newX = snakeX[0] + dirX;
  int newY = snakeY[0] + dirY;

  // Стены
  if (newX < 0 || newX > 15 ||  newY < 0 || newY > 1) {
    gameOver = true; return;
  }

  // Сама себя
  for (int i = 0; i < snakeLen; i++) {
    if (snakeX[i] == newX && snakeY[i] == newY) {
      gameOver = true; return;
    }
  }

  // Сдвиг
  for (int i = snakeLen - 1; i > 0; i--) {
    snakeX[i] = snakeX[i-1];
    snakeY[i] = snakeY[i-1];
  }
  snakeX[0] = newX;
  snakeY[0] = newY;

  // Еда
  if (newX == foodX && newY == foodY) {
    if (snakeLen < MAX_LEN) snakeLen++;
    spawnFood();
  }
}

void runSnake() {
  lcd.createChar(0, snakeBody);
  lcd.createChar(1, food);
  initSnake();

  while (!gameOver) {
    updateSnake();
    drawSnake();
    delay(300);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Game Over!");
  lcd.setCursor(0, 1);
  lcd.print("Score: ");
  lcd.print(snakeLen - 3);
  delay(2000);
}

byte paddle[8] = {0b11111,0b11111,0b00000,0b00000,0b00000,0b00000,0b00000,0b00000};
byte ball[8]   = {0b00000,0b01110,0b01110,0b00000,0b00000,0b00000,0b00000,0b00000};

int ballX = 8, ballY = 0;
int ballDirX = 1, ballDirY = 1;
int paddleY = 0; // 0 = верхняя строка, 1 = нижняя
int score = 0;
bool gameover = false;

void runPong() {
  lcd.createChar(0, paddle);
  lcd.createChar(1, ball);
  ballX = 8; ballY = 0;
  ballDirX = 1; ballDirY = 1;
  score = 0; gameover = false;

  while (!gameover) {
    // Джойстик
    int y = analogRead(JOY_Y);
    if (y < 400) paddleY = 0;
    if (y > 600) paddleY = 1;

    // Мяч
    ballX += ballDirX;
    ballY += ballDirY;

    // Отскок от верха/низа
    if (ballY < 0) { ballY = 0; ballDirY = 1; }
    if (ballY > 1) { ballY = 1; ballDirY = -1; }

    // Отскок от левой стены
    if (ballX <= 0) { ballX = 0; ballDirX = 1; }

    // Ракетка справа (колонка 15)
    if (ballX >= 15) {
      if (ballY == paddleY) {
        ballDirX = -1;
        score++;
      } else {
        gameover = true;
      }
    }

    lcd.clear();
    // Ракетка
    lcd.setCursor(15, paddleY);
    lcd.write(byte(0));
    // Мяч
    lcd.setCursor(ballX, ballY);
    lcd.write(byte(1));
    // Счёт
    lcd.setCursor(0, 0);
    lcd.print(score);

    delay(200);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Game Over!");
  lcd.setCursor(0, 1);
  lcd.print("Score: ");
  lcd.print(score);
  delay(2000);
}

void runGuess() {
  int secret = random(1, 100);
  int guess = 50;
  int attempts = 0;
  bool won = false;

  pinMode(JOY_SW, INPUT_PULLUP);

  while (!won) {
    int x = analogRead(JOY_Y);
    if (x < 400 && guess > 1)  { guess++; delay(150); }
    if (x > 600 && guess < 99) { guess--; delay(150); }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Guess: ");
    lcd.print(guess);
    lcd.setCursor(0, 1);
    lcd.print("Tries: ");
    lcd.print(attempts);

    if (!digitalRead(JOY_SW)) {
      attempts++;
      delay(200);
    if (guess == secret) {
        won = true;
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        if (guess < secret) lcd.print("Higher!");
        else lcd.print("Lower!");
        delay(1000);
      }
    }

    delay(50);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Correct! ");
  lcd.print(secret);
  lcd.setCursor(0, 1);
  lcd.print("Tries: ");
  lcd.print(attempts);
  delay(3000);
}

void runReaction() {
  pinMode(JOY_SW, INPUT_PULLUP);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Get ready...");
  delay(random(2000, 5000));

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PRESS NOW!!!");

  unsigned long start = millis();

  while (digitalRead(JOY_SW)); // ждём нажатия

  unsigned long reaction = millis() - start;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(reaction);
  lcd.print("ms");
  lcd.setCursor(0, 1);
  if (reaction < 200)      lcd.print("Incredible!");
  else if (reaction < 400) lcd.print("Great!");
  else if (reaction < 700) lcd.print("Good!");
  else                     lcd.print("Too slow!");

  delay(3000);
}