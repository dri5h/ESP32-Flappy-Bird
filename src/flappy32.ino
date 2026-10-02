#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define BUTTON_PIN 25
#define BUZZER_PIN 33

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define GROUND_Y 220

#define SKY_COLOR     0x867D
#define CLOUD_COLOR   0xFFFF
#define CLOUD_LIGHT   0xDFFF
#define PIPE_COLOR    0x7EEC
#define PIPE_DARK     0x3D47
#define GROUND_COLOR  0x8A44
#define GROUND_TOP    0xC680
#define BIRD_COLOR    TFT_YELLOW
#define BIRD_WING     TFT_ORANGE
#define BIRD_BEAK     TFT_ORANGE

#define PIPE_WIDTH 36
#define PIPE_GAP   82
#define BIRD_X      70
#define BIRD_RADIUS 8

#define FRAME_TIME    30
#define DEBOUNCE_TIME 200

enum GameState { START_SCREEN, PLAYING, GAME_OVER };
GameState gameState = START_SCREEN;

float birdY = 120;
float birdVelocity = 0;
float gravity = 0.22;
float flapStrength = -4.2;
float pipeX = SCREEN_WIDTH + 30;
int   pipeGapY = 120;
int score = 0;
int highScore = 0;
float pipeSpeed = 3.0;

unsigned long lastFrame = 0;
unsigned long lastButtonPress = 0;
unsigned long flapLockoutUntil = 0;
unsigned long beepEndTime = 0;
bool lastButtonState = HIGH;

void beep(int freq, int durationMs) {
  ledcAttach(BUZZER_PIN, freq, 8);
  ledcWrite(BUZZER_PIN, 128);
  beepEndTime = millis() + durationMs;
}

void updateBeep() {
  if (beepEndTime != 0 && millis() >= beepEndTime) {
    ledcWrite(BUZZER_PIN, 0);
    ledcDetach(BUZZER_PIN);
    beepEndTime = 0;
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  tft.init();
  tft.setRotation(1);

  randomSeed(analogRead(34));

  showStartScreen();
}

void loop() {
  updateBeep();

  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && lastButtonState == HIGH) {
    if (millis() - lastButtonPress > DEBOUNCE_TIME) {
      delay(5);
      if (digitalRead(BUTTON_PIN) == LOW) {
        handleButton();
        lastButtonPress = millis();
      }
    }
  }

  lastButtonState = buttonState;

  if (gameState == PLAYING) {
    if (millis() - lastFrame >= FRAME_TIME) {
      lastFrame = millis();

      int oldBirdY = (int)birdY;
      int oldPipeX = (int)pipeX;
      int oldPipeGapY = pipeGapY;
      int oldScore = score;

      updateGame();

      if (gameState == PLAYING) {
        drawGame(oldBirdY, oldPipeX, oldPipeGapY, oldScore);
      }
    }
  }
}

void handleButton() {
  if (gameState == START_SCREEN) {
    startGame();
  }
  else if (gameState == PLAYING) {
    flap();
  }
  else if (gameState == GAME_OVER) {
    startGame();
  }
}

void startGame() {
  gameState = PLAYING;
  birdY = SCREEN_HEIGHT / 2;
  birdVelocity = 0;
  pipeX = SCREEN_WIDTH + 30;
  pipeGapY = random(65, 155);
  score = 0;
  pipeSpeed = 3.0;
  flapLockoutUntil = millis() + 250;

  beep(1000, 70);

  tft.fillScreen(SKY_COLOR);
  drawClouds();
  drawGround();
  drawPipe();
  drawBird();
  drawScore();
}

void flap() {
  if (millis() < flapLockoutUntil) return;
  birdVelocity = flapStrength;
  beep(1200, 50);
}

void updateGame() {
  birdVelocity += gravity;
  birdY += birdVelocity;
  pipeX -= pipeSpeed;

  if (pipeX + PIPE_WIDTH < 0) {
    pipeX = SCREEN_WIDTH + random(20, 80);
    pipeGapY = random(65, 155);
    score++;
    if (score > highScore) highScore = score;
    if (score % 5 == 0) pipeSpeed += 0.35;
    beep(1600, 70);
  }

  if (birdY - BIRD_RADIUS <= 0 || birdY + BIRD_RADIUS >= GROUND_Y) {
    endGame();
    return;
  }

  if (checkPipeCollision()) {
    endGame();
    return;
  }
}

bool checkPipeCollision() {
  int birdLeft   = BIRD_X - BIRD_RADIUS;
  int birdRight  = BIRD_X + BIRD_RADIUS;
  int birdTop    = (int)birdY - BIRD_RADIUS;
  int birdBottom = (int)birdY + BIRD_RADIUS;
  int currentPipeX = (int)pipeX;
  int pipeLeft  = currentPipeX;
  int pipeRight = currentPipeX + PIPE_WIDTH;

  if (birdRight > pipeLeft && birdLeft < pipeRight) {
    if (birdTop < pipeGapY - PIPE_GAP / 2 ||
        birdBottom > pipeGapY + PIPE_GAP / 2) {
      return true;
    }
  }
  return false;
}

void endGame() {
  gameState = GAME_OVER;
  beep(300, 350);
  showGameOver();
}

void drawGame(int oldBirdY, int oldPipeX, int oldPipeGapY, int oldScore) {
  tft.startWrite();
  erasePipe(oldPipeX, oldPipeGapY);
  eraseBird(oldBirdY);
  drawPipe();
  drawBird();
  if (score != oldScore) {
    clearScoreArea();
    drawScore();
  }
  tft.endWrite();
}

void erasePipe(int x, int gapY) {
  if (x < -50 || x > SCREEN_WIDTH + 10) return;
  int topPipeBottom = gapY - PIPE_GAP / 2;
  int bottomPipeTop = gapY + PIPE_GAP / 2;
  int eraseX = x - 5;
  int eraseWidth = PIPE_WIDTH + 10;
  if (eraseX < 0) { eraseWidth += eraseX; eraseX = 0; }
  if (eraseX + eraseWidth > SCREEN_WIDTH) eraseWidth = SCREEN_WIDTH - eraseX;
  if (eraseWidth <= 0) return;

  if (topPipeBottom > 0) {
    tft.fillRect(eraseX, 0, eraseWidth, topPipeBottom, SKY_COLOR);
    restoreClouds(eraseX, 0, eraseWidth, topPipeBottom);
  }
  if (bottomPipeTop < GROUND_Y) {
    tft.fillRect(eraseX, bottomPipeTop, eraseWidth, GROUND_Y - bottomPipeTop, SKY_COLOR);
    restoreClouds(eraseX, bottomPipeTop, eraseWidth, GROUND_Y - bottomPipeTop);
  }
}

void eraseBird(int oldY) {
  int x = BIRD_X - BIRD_RADIUS - 8;
  int y = oldY - BIRD_RADIUS - 8;
  int w = BIRD_RADIUS * 2 + 16;
  int h = BIRD_RADIUS * 2 + 16;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
  if (y + h > GROUND_Y) h = GROUND_Y - y;
  if (w <= 0 || h <= 0) return;
  tft.fillRect(x, y, w, h, SKY_COLOR);
  restoreClouds(x, y, w, h);
}

void restoreClouds(int rx, int ry, int rw, int rh) {
  restoreCloud(45,  45, 1.0, rx, ry, rw, rh);
  restoreCloud(180, 75, 0.8, rx, ry, rw, rh);
  restoreCloud(275, 35, 1.2, rx, ry, rw, rh);
}

void restoreCloud(int x, int y, float scale, int rx, int ry, int rw, int rh) {
  int cloudLeft   = x - 20;
  int cloudRight  = x + 60 * scale;
  int cloudTop    = y - 25 * scale;
  int cloudBottom = y + 25 * scale;
  int regionRight  = rx + rw;
  int regionBottom = ry + rh;
  bool intersects = cloudRight >= rx && cloudLeft <= regionRight &&
                    cloudBottom >= ry && cloudTop <= regionBottom;
  if (intersects) drawCloud(x, y, scale);
}

void clearScoreArea() {
  tft.fillRect(135, 0, 50, 50, SKY_COLOR);
  restoreClouds(135, 0, 50, 50);
}

void drawClouds() {
  drawCloud(45,  45, 1.0);
  drawCloud(180, 75, 0.8);
  drawCloud(275, 35, 1.2);
}

void drawCloud(int x, int y, float scale) {
  int r1 = 12 * scale, r2 = 17 * scale, r3 = 11 * scale;
  tft.fillCircle(x, y, r1, CLOUD_COLOR);
  tft.fillCircle(x + 17 * scale, y - 5 * scale, r2, CLOUD_COLOR);
  tft.fillCircle(x + 38 * scale, y, r3, CLOUD_COLOR);
  tft.fillRect(x, y, 38 * scale, 13 * scale, CLOUD_COLOR);
  tft.fillCircle(x + 17 * scale, y - 7 * scale, 7 * scale, CLOUD_LIGHT);
}

void drawPipe() {
  int currentPipeX = (int)pipeX;
  int topPipeBottom = pipeGapY - PIPE_GAP / 2;
  int bottomPipeTop = pipeGapY + PIPE_GAP / 2;
  if (currentPipeX + PIPE_WIDTH < 0 || currentPipeX > SCREEN_WIDTH) return;

  if (topPipeBottom > 0) {
    tft.fillRect(currentPipeX, 0, PIPE_WIDTH, topPipeBottom, PIPE_COLOR);
    tft.fillRect(currentPipeX - 4, topPipeBottom - 15, PIPE_WIDTH + 8, 15, PIPE_DARK);
  }
  if (bottomPipeTop < GROUND_Y) {
    tft.fillRect(currentPipeX, bottomPipeTop, PIPE_WIDTH, GROUND_Y - bottomPipeTop, PIPE_COLOR);
    tft.fillRect(currentPipeX - 4, bottomPipeTop, PIPE_WIDTH + 8, 15, PIPE_DARK);
  }
}

void drawBird() {
  int y = (int)birdY;
  tft.fillCircle(BIRD_X, y, BIRD_RADIUS, BIRD_COLOR);
  tft.fillCircle(BIRD_X + 4, y - 3, 2, TFT_BLACK);
  tft.fillTriangle(BIRD_X + 7, y, BIRD_X + 15, y + 3, BIRD_X + 7, y + 5, BIRD_BEAK);
  tft.fillCircle(BIRD_X - 3, y + 4, 4, BIRD_WING);
}

void drawGround() {
  tft.fillRect(0, GROUND_Y, SCREEN_WIDTH, SCREEN_HEIGHT - GROUND_Y, GROUND_COLOR);
  tft.fillRect(0, GROUND_Y, SCREEN_WIDTH, 5, GROUND_TOP);
  for (int x = 0; x < SCREEN_WIDTH; x += 16) {
    tft.drawLine(x, GROUND_Y + 10, x + 8, GROUND_Y + 18, GROUND_TOP);
  }
}

void drawScore() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, SKY_COLOR);
  tft.setTextSize(3);
  tft.drawString(String(score), SCREEN_WIDTH / 2, 25);
  tft.setTextDatum(TL_DATUM);
}

void showStartScreen() {
  tft.fillScreen(SKY_COLOR);
  drawClouds();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, SKY_COLOR);
  tft.setTextSize(4);
  tft.drawString("FLAPPY", SCREEN_WIDTH / 2, 65);
  tft.drawString("BIRD",   SCREEN_WIDTH / 2, 105);
  tft.setTextSize(2);
  tft.drawString("PRESS BUTTON", SCREEN_WIDTH / 2, 160);
  tft.drawString("TO START",     SCREEN_WIDTH / 2, 185);
  tft.setTextDatum(TL_DATUM);
}

void showGameOver() {
  tft.fillScreen(SKY_COLOR);
  drawClouds();
  drawGround();
  tft.fillRoundRect(45, 50, 230, 135, 12, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("GAME OVER", SCREEN_WIDTH / 2, 78);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("SCORE: " + String(score),    SCREEN_WIDTH / 2, 115);
  tft.drawString("BEST: "  + String(highScore), SCREEN_WIDTH / 2, 140);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("PRESS TO RESTART", SCREEN_WIDTH / 2, 170);
  tft.setTextDatum(TL_DATUM);
}
