#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <math.h>
#include <vector>

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// I2C pins for OLED
#define I2C_SDA 21
#define I2C_SCL 22

// Joystick pins
const int JOY_VRX_PIN = 15; // VRX connected to GPIO 15
const int JOY_VRY_PIN = 2;  // VRY connected to GPIO 2
const int JOY_SW_PIN = 4;   // SW connected to GPIO 4

// Joystick thresholds and deadzone
const int JOYSTICK_CENTER = 2048; // Approximate center value
const int JOYSTICK_DEADZONE_LOW = 1500;
const int JOYSTICK_DEADZONE_HIGH = 2500;
const int JOYSTICK_THRESHOLD_LOW = 1000; // Threshold for definite up/left
const int JOYSTICK_THRESHOLD_HIGH = 3000; // Threshold for definite down/right


Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Game states
enum GameState {
  MENU,
  GOOGLE_DINO,
  CROSSY_ROAD,
  AIRPLANE,
  TETRIS
};
GameState currentGameState = MENU;

// Menu variables
const char* gameNames[] = {"Google Dino", "Crossy Road", "Airplane", "Tetris"};
int selectedGame = 0;
int numGames = sizeof(gameNames) / sizeof(gameNames[0]);

// Joystick state variables
int lastJoySWState = HIGH;
unsigned long lastJoySWPressTime = 0;
unsigned long lastJoySWReleaseTime = 0;
const unsigned long debounceDelay = 50; // 50ms debounce
const unsigned long doubleClickDelay = 300; // 300ms for double click detection
int clickCount = 0;
bool singleClickForMenu = false; // Flag for menu selection

// Google Dino Game Variables
namespace DinoGame {
  // Dino properties
  int dinoX = 10;
  int dinoY = SCREEN_HEIGHT - 20; // Initial Y position
  int dinoBaseY = SCREEN_HEIGHT - 20;
  int dinoWidth = 15;
  int dinoHeight = 20;
  bool isJumping = false;
  bool isDucking = false;
  float jumpVelocity = 0;
  const float gravity = 0.6;
  const float jumpPower = -10; // Negative for upward movement
  int duckHeight = 10;

  // Obstacle properties
  struct Obstacle {
    int x;
    int y;
    int width;
    int height;
    bool isBird; // true for bird, false for cactus
  };
  std::vector<Obstacle> obstacles;
  int obstacleSpeed = 2;
  unsigned long lastObstacleSpawnTime = 0;
  unsigned long obstacleSpawnInterval = 2000; // milliseconds

  // Game state
  bool gameOver = false;
  int score = 0;
  unsigned long lastScoreIncrementTime = 0;
  const unsigned long scoreIncrementInterval = 100; // Increment score every 100ms when not game over
}

// Function Prototypes
void displayMenu();
void handleMenuInput();
void runGoogleDino();
void handleGoogleDinoInput();
void drawDino();
void drawObstacles();
void updateObstacles();
void checkCollisions();
void displayGameOver();
void resetDinoGame();
void runCrossyRoad();
void runAirplane();
void runTetris();
void checkJoystickEvents();


void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(JOY_SW_PIN, INPUT_PULLUP);
  // ADC for joystick X and Y are typically already configured.

  if(!display.begin(0x3C, true)) { // Address 0x3C for 128x64
    Serial.println(F("SH1106 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  delay(100); // Pause for 100 ms
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);

  resetDinoGame(); // Initialize Dino game variables
}

void loop() {
  checkJoystickEvents(); // Check for clicks first

  switch (currentGameState) {
    case MENU:
      handleMenuInput();
      displayMenu();
      break;
    case GOOGLE_DINO:
      if (DinoGame::gameOver) {
        displayGameOver();
        handleGoogleDinoInput(); // To catch restart
      } else {
        handleGoogleDinoInput();
        runGoogleDino();
      }
      break;
    case CROSSY_ROAD:
      // Placeholder for Crossy Road
      runCrossyRoad();
      break;
    case AIRPLANE:
      // Placeholder for Airplane
      runAirplane();
      break;
    case TETRIS:
      // Placeholder for Tetris
      runTetris();
      break;
  }
  display.display();
  delay(30); // Frame rate delay
}

void checkJoystickEvents() { // New function name
  singleClickForMenu = false; // Reset at the beginning of each check
  int currentJoySWState = digitalRead(JOY_SW_PIN);
  unsigned long currentTime = millis();

  // Press detection (falling edge)
  if (currentJoySWState == LOW && lastJoySWState == HIGH && (currentTime - lastJoySWPressTime > debounceDelay)) {
    lastJoySWPressTime = currentTime; // Time of *this* press
    clickCount++;
    if (clickCount == 1) {
      lastJoySWReleaseTime = currentTime; // Time of the *first* press in a sequence
    }
    // Serial.print("Press. clickCount: "); Serial.println(clickCount); // Debug
  }
  lastJoySWState = currentJoySWState;

  if (clickCount > 0) { // If there's an active click sequence
    // Check for Double Click: 2nd press detected within doubleClickDelay of the 1st press
    if (clickCount >= 2 && (currentTime - lastJoySWReleaseTime < doubleClickDelay)) {
      // Serial.println("Double Click Action!"); // Debug
      if (currentGameState != MENU) {
        currentGameState = MENU;
        selectedGame = 0;
        if (DinoGame::gameOver) resetDinoGame();
        Serial.println("Double click: Returning to Menu");
      }
      clickCount = 0; // Consumed the double click
      // lastJoySWPressTime was just updated by the 2nd press, this helps debounce next sequence
    }
    // Check for Single Click Maturation or Sequence Timeout
    else if (currentTime - lastJoySWReleaseTime >= doubleClickDelay) {
      // The window for a double click (stemming from the first press at lastJoySWReleaseTime) has passed.
      if (clickCount == 1) {
        // Serial.println("Single Click Ready for Menu"); // Debug
        singleClickForMenu = true; // Signal menu system that a single click is ready
        // clickCount will be reset by handleMenuInput when it consumes this.
      } else {
        // If clickCount wasn't 1 (e.g., was >=2 but the second click was too LATE, or 0 due to some other reset)
        // or if it was some other invalid state, just reset the sequence.
        // Serial.println("Click sequence timed out / invalid, resetting clickCount."); // Debug
        clickCount = 0; // Reset for safety or if sequence was >1 but not a valid double.
      }
    }
  }
}


void displayMenu() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 5);
  display.println("GAMES");
  display.setTextSize(1);

  for (int i = 0; i < numGames; i++) {
    display.setCursor(10, 25 + i * 10);
    if (i == selectedGame) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.println(gameNames[i]);
  }
}

void handleMenuInput() {
  int joyYVal = analogRead(JOY_VRY_PIN);
  // int currentJoySWState = digitalRead(JOY_SW_PIN); // Not directly needed here for selection anymore
  static unsigned long lastInputTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastInputTime < 150) { // Input cooldown for navigation
    return;
  }
  
  // Vertical navigation
  if (joyYVal < JOYSTICK_THRESHOLD_LOW) { // Up
    selectedGame = (selectedGame - 1 + numGames) % numGames;
    lastInputTime = currentTime;
  } else if (joyYVal > JOYSTICK_THRESHOLD_HIGH) { // Down
    selectedGame = (selectedGame + 1) % numGames;
    lastInputTime = currentTime;
  }

  // Selection (single click) - now driven by the flag from checkJoystickEvents
  if (singleClickForMenu) {
    // Reset game if re-entering
    if (static_cast<GameState>(selectedGame + 1) == GOOGLE_DINO) {
        resetDinoGame();
    }
    currentGameState = static_cast<GameState>(selectedGame + 1); // MENU is 0, games start from 1
    Serial.print("Selected game (single click): "); Serial.println(gameNames[selectedGame]);
    
    clickCount = 0;          // Consume the click sequence
    singleClickForMenu = false; // Consume the flag
    // Update lastJoySWPressTime to prevent checkJoystickEvents from immediately
    // re-interpreting the button state if it's still held down from the single click.
    lastJoySWPressTime = millis(); 
  }
  // lastJoySWState is updated in checkJoystickEvents
}


// --- Google Dino Game ---
void resetDinoGame() {
  using namespace DinoGame;
  dinoX = 10;
  dinoY = dinoBaseY;
  isJumping = false;
  isDucking = false;
  jumpVelocity = 0;
  obstacles.clear();
  gameOver = false;
  score = 0;
  obstacleSpeed = 2;
  lastObstacleSpawnTime = millis(); // Start spawning immediately
  obstacleSpawnInterval = 2000 + random(0,1000); // Randomize first spawn
}

void drawDino() {
  using namespace DinoGame;
  display.setTextColor(SH110X_WHITE);
  int currentDinoHeight = isDucking ? duckHeight : dinoHeight;
  int currentDinoY = isDucking ? dinoBaseY + (dinoHeight - duckHeight) : dinoY;
  display.fillRect(dinoX, currentDinoY, dinoWidth, currentDinoHeight, SH110X_WHITE);
}

void drawObstacles() {
  using namespace DinoGame;
  display.setTextColor(SH110X_WHITE);
  for (const auto& obs : obstacles) {
    display.fillRect(obs.x, obs.y, obs.width, obs.height, SH110X_WHITE);
  }
}

void updateObstacles() {
  using namespace DinoGame;
  unsigned long currentTime = millis();

  // Move existing obstacles
  for (size_t i = 0; i < obstacles.size(); ++i) {
    obstacles[i].x -= obstacleSpeed;
    // Remove obstacles that are off-screen
    if (obstacles[i].x + obstacles[i].width < 0) {
      obstacles.erase(obstacles.begin() + i);
      i--; // Adjust index after removal
      score++; // Increment score for passing an obstacle
    }
  }

  // Spawn new obstacles
  if (currentTime - lastObstacleSpawnTime > obstacleSpawnInterval) {
    lastObstacleSpawnTime = currentTime;
    obstacleSpawnInterval = 1500 + random(0, 1500) - (score * 10); // Decrease interval as score increases
    if (obstacleSpawnInterval < 500) obstacleSpawnInterval = 500; // Minimum spawn interval

    Obstacle newObs;
    newObs.x = SCREEN_WIDTH;
    newObs.width = 10 + random(0,10); // Random width

    if (random(0, 3) == 0) { // 1 in 3 chance for a bird
        newObs.isBird = true;
        newObs.height = 8 + random(0,5);
        // Birds can be at two heights: one dino can jump, one dino must duck
        if (random(0,2) == 0) {
             newObs.y = dinoBaseY - newObs.height - 5 - random(0,5) ; // Higher bird, requires ducking or precise jump
        } else {
             newObs.y = dinoBaseY - newObs.height + 5 ; // Lower bird, jumpable
        }

    } else { // Cactus
        newObs.isBird = false;
        newObs.height = 15 + random(0,15);
        newObs.y = SCREEN_HEIGHT - newObs.height;
    }
    obstacles.push_back(newObs);
    
    // Increase speed over time/score
    if (score > 0 && score % 5 == 0) { // Every 5 points
        obstacleSpeed++;
        if (obstacleSpeed > 6) obstacleSpeed = 6; // Max speed
    }
  }
}

void handleGoogleDinoInput() {
  using namespace DinoGame;
  int joyYVal = analogRead(JOY_VRY_PIN);
  // int currentJoySWState = digitalRead(JOY_SW_PIN); // Re-evaluate if needed, single click logic below
  // static unsigned long lastButtonTime = 0; // May not be needed if using global click system
  unsigned long currentTime = millis();

  if (gameOver) {
    // For game restart, we can also use the singleClickForMenu flag,
    // but need to ensure it's not confused with menu navigation if checkJoystickEvents runs first.
    // Simpler: direct check for a fresh press, separate from the menu system's single click.
    // This part might need its own debounced click detection or tie into the global one carefully.

    // Let's use a simplified version of a debounced press for restart:
    int currentJoySWStateForRestart = digitalRead(JOY_SW_PIN);
    static int lastJoySWStateForRestart = HIGH;
    static unsigned long lastPressTimeForRestart = 0;

    if (currentJoySWStateForRestart == LOW && lastJoySWStateForRestart == HIGH && (currentTime - lastPressTimeForRestart > debounceDelay)) {
        lastPressTimeForRestart = currentTime;
        // Check if this press could have been part of a double click that returned to menu
        // This check might be complex. For now, assume any click here is a restart.
        // A more robust way would be to ensure that checkJoystickEvents didn't just process a double click.
        // However, if a double click happened, currentGameState would be MENU, so we wouldn't be in GOOGLE_DINO gameOver.
        
        // Serial.println("Game Over - Click for restart"); // Debug
        resetDinoGame();
        // clickCount = 0; // Reset global clickCount too, as this click is consumed.
        // lastJoySWPressTime = currentTime; // And global press time.
        // This is better handled if the global click system signals a general purpose single click.
        // For now, this independent click detection for restart should work.
    }
    lastJoySWStateForRestart = currentJoySWStateForRestart;
    return;
  }

  // Jump
  if (joyYVal < JOYSTICK_THRESHOLD_LOW && !isJumping && !isDucking) {
    isJumping = true;
    jumpVelocity = jumpPower;
  }

  // Ducking - Hold to duck
  if (joyYVal > JOYSTICK_THRESHOLD_HIGH && !isJumping) {
    isDucking = true;
  } else {
    // If not holding down and not jumping, stand up
    if (isDucking && !(joyYVal > JOYSTICK_THRESHOLD_HIGH)) {
         isDucking = false;
    }
  }
}

void checkCollisions() {
  using namespace DinoGame;
  int currentDinoHeight = isDucking ? duckHeight : dinoHeight;
  int currentDinoY = isDucking ? dinoBaseY + (dinoHeight - duckHeight) : dinoY;

  for (const auto& obs : obstacles) {
    // Simple AABB collision detection
    // Dino: dinoX, currentDinoY, dinoWidth, currentDinoHeight
    // Obstacle: obs.x, obs.y, obs.width, obs.height
    if (dinoX < obs.x + obs.width &&
        dinoX + dinoWidth > obs.x &&
        currentDinoY < obs.y + obs.height &&
        currentDinoY + currentDinoHeight > obs.y) {
      gameOver = true;
      return;
    }
  }
}

void displayGameOver() {
  display.clearDisplay(); // Clear previous game screen
  int16_t x1, y1;
  uint16_t w, h;

  // "Game Over" text
  display.setTextSize(2);
  const char* gameOverText = "Game Over";
  display.getTextBounds(gameOverText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 - 10);
  display.println(gameOverText);

  // Score text
  display.setTextSize(1);
  String scoreStr = "Score: " + String(DinoGame::score);
  display.getTextBounds(scoreStr.c_str(), 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 + 10);
  display.println(scoreStr);

  // "Click to Restart" text
  // display.setTextSize(1); // Already set
  const char* restartText = "Click to Restart";
  display.getTextBounds(restartText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 + 20);
  display.println(restartText);
  // display.display() is called in loop()
}


void runGoogleDino() {
  using namespace DinoGame;
  display.clearDisplay();

  // Update dino position (jump physics)
  if (isJumping) {
    dinoY += jumpVelocity;
    jumpVelocity += gravity;
    if (dinoY >= dinoBaseY) {
      dinoY = dinoBaseY;
      isJumping = false;
      jumpVelocity = 0;
    }
  }

  updateObstacles();
  checkCollisions();
  
  if (!gameOver) {
    // Increment score over time
    if(millis() - lastScoreIncrementTime > scoreIncrementInterval) {
        score++;
        lastScoreIncrementTime = millis();
    }
  }


  // Drawing
  drawDino();
  drawObstacles();

  // Display Score
  display.setCursor(SCREEN_WIDTH - 40, 0);
  display.print("S: ");
  display.println(score);
  
  // Display current game mode (optional, for debugging)
  // display.setCursor(0,0);
  // display.print("DINO");

  // display.display() is called in loop()
}


// --- Placeholder Game Functions ---
void runCrossyRoad() {
  display.clearDisplay();
  display.setCursor(10, SCREEN_HEIGHT / 2 - 5);
  display.setTextSize(1);
  display.println("Crossy Road - WIP");
  // display.display() is called in loop()
  // Implement double click to go back to menu, or single click for game specific action
}

void runAirplane() {
  display.clearDisplay();
  display.setCursor(10, SCREEN_HEIGHT / 2 - 5);
  display.setTextSize(1);
  display.println("Airplane - WIP");
  // display.display() is called in loop()
}

void runTetris() {
  display.clearDisplay();
  display.setCursor(10, SCREEN_HEIGHT / 2 - 5);
  display.setTextSize(1);
  display.println("Tetris - WIP");
  // display.display() is called in loop()
}