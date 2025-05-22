#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <math.h>
#include <vector>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

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

// LED Pin for Online Game
const int LED_PIN = 13;

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
  TETRIS,
  ONLINE_GAME
};
GameState currentGameState = MENU;

// Menu variables
const char* gameNames[] = {"Google Dino", "Crossy Road", "Airplane", "Tetris", "Online Game"};
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
  // --- Bitmaps ---
  // Standing Dino Frame 1 (24w x 17h - from user)
  const unsigned char dino_stand_frame1_bmp[] PROGMEM = {
    0b00000000, 0b11111111, 0b00000000, //         ********
    0b00000011, 0b11111111, 0b11000000, //       ************
    0b00000011, 0b11111111, 0b11000000, //       ************
    0b00000011, 0b11111111, 0b11000000, //       ************
    0b00000011, 0b11110000, 0b00000000, //       ****
    0b00000011, 0b11111111, 0b00000000, //       *********
    0b00000001, 0b11111111, 0b00000000, //        ********
    0b00000011, 0b11111111, 0b11000000, //       ************
    0b00000111, 0b11111111, 0b11100000, //      **************
    0b00000111, 0b11111111, 0b11100000, //      **************
    0b00000011, 0b11111111, 0b11000000, //       ************
    0b00000001, 0b11111111, 0b10000000, //        ***********
    0b00000000, 0b11111111, 0b00000000, //         **********
    0b00000000, 0b01110011, 0b00000000, //          ***  **
    0b00000000, 0b00110001, 0b00000000, //           **   *
    0b00000000, 0b00010000, 0b10000000, //            *    *
    0b00000000, 0b00110000, 0b11000000  //           **    **
  };

  // Standing Dino Frame 2 (for walking animation, 24w x 17h)
  const unsigned char dino_stand_frame2_bmp[] PROGMEM = {
    0b00000000, 0b11111111, 0b00000000, // Row 0
    0b00000011, 0b11111111, 0b11000000, // Row 1
    0b00000011, 0b11111111, 0b11000000, // Row 2
    0b00000011, 0b11111111, 0b11000000, // Row 3
    0b00000011, 0b11110000, 0b00000000, // Row 4
    0b00000011, 0b11111111, 0b00000000, // Row 5
    0b00000001, 0b11111111, 0b00000000, // Row 6
    0b00000011, 0b11111111, 0b11000000, // Row 7
    0b00000111, 0b11111111, 0b11100000, // Row 8
    0b00000111, 0b11111111, 0b11100000, // Row 9
    0b00000011, 0b11111111, 0b11000000, // Row 10
    0b00000001, 0b11111111, 0b10000000, // Row 11
    0b00000000, 0b11111111, 0b00000000, // Row 12
    0b00000000, 0b01100110, 0b00000000, // Row 13 (Legs changed)
    0b00000000, 0b00100010, 0b00000000, // Row 14 (Legs changed)
    0b00000000, 0b00010001, 0b10000000, // Row 15 (Legs changed)
    0b00000000, 0b00010001, 0b11000000  // Row 16 (Legs changed)
  };
  const int DINO_STAND_WIDTH = 24;
  const int DINO_STAND_HEIGHT = 17;

  // Ducking Dino (approx 22w x 10h)
  const unsigned char dino_duck_bmp[] PROGMEM = {
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00011111, 0b11111110, 0b00000000,
    0b00111111, 0b11111111, 0b11000000,
    0b01111111, 0b11111111, 0b11000000,
    0b11111111, 0b11111000, 0b00000000,
    0b01110011, 0b11001100, 0b00000000,
    0b00110011, 0b00001100, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000
  };
  const int DINO_DUCK_WIDTH = 22;
  const int DINO_DUCK_HEIGHT = 10;

  // Cactus Type 1 (10w x 13h - from user diff observation)
  const unsigned char cactus_1_bmp[] PROGMEM = {
    0b00011000, 0b00000000, //    **
    0b00011000, 0b00000000, //    **
    0b00011000, 0b00000000, //    **
    0b11011011, 0b00000000, // ** ** ** (arms)
    0b11011011, 0b00000000, // ** ** **
    0b00011000, 0b00000000, //    **
    0b00011000, 0b00000000, //    **
    0b00011000, 0b00000000, //    **
    0b00011000, 0b00000000, //    **
    0b00011000, 0b00000000, //    **
    0b00011000, 0b00000000, //    **
    0b00011000, 0b00000000, //    **
    0b00011000, 0b00000000  //    **
  };
  const int CACTUS_1_WIDTH = 10;
  const int CACTUS_1_HEIGHT = 13; // Corrected height

  // Cloud Bitmap (20w x 8h)
  const unsigned char cloud_1_bmp[] PROGMEM = {
    0b00000000, 0b00000000, 0b00000000,
    0b00000111, 0b11100000, 0b00000000,
    0b00011111, 0b11111000, 0b00000000,
    0b00111111, 0b11111110, 0b00000000,
    0b00111111, 0b11111110, 0b00000000,
    0b00011111, 0b11111000, 0b00000000,
    0b00000111, 0b11100000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000
  };
  const int CLOUD_1_WIDTH = 20;
  const int CLOUD_1_HEIGHT = 8;

  // Bird Frame 1 (16w x 10h)
  const unsigned char bird_frame1_bmp[] PROGMEM = {
    0b00000000, 0b00000000, // 
    0b00000110, 0b00000000, //      **
    0b00001111, 0b00000000, //     ****
    0b00011111, 0b11000000, //    ******** (body & head)
    0b00111111, 0b11100000, //   **********
    0b01111000, 0b11110000, //  *****  **** (wing up)
    0b00110000, 0b01110000, //   **     ***
    0b00010000, 0b00110000, //    *      **
    0b00000000, 0b00000000, // 
    0b00000000, 0b00000000  // 
  };
  // Bird Frame 2 (16w x 10h)
  const unsigned char bird_frame2_bmp[] PROGMEM = {
    0b00000000, 0b00000000, // 
    0b00000000, 0b00000000, // 
    0b00000110, 0b00000000, //      **
    0b00001111, 0b00000000, //     ****
    0b00011111, 0b11000000, //    ******** (body & head)
    0b00111111, 0b11100000, //   **********
    0b01111000, 0b11110000, //  *****  **** (wing down)
    0b00110000, 0b01110000, //   **     ***
    0b00010000, 0b00110000, //    *      **
    0b00000000, 0b00000000  // 
  };
  const int BIRD_WIDTH = 16;
  const int BIRD_HEIGHT = 10;
  const int BIRD_ANIM_INTERVAL = 200; // ms for bird flapping

  // Ground line
  const int groundLineY = SCREEN_HEIGHT - 12; // Y position of the ground line surface

  // Dino properties
  int dinoX = 10;
  int dinoBaseY = groundLineY - DINO_STAND_HEIGHT;
  int dinoY = dinoBaseY;
  bool isJumping = false;
  bool isDucking = false;
  float jumpVelocity = 1; // User changed
  const float gravity = 0.6;
  const float jumpPower = -5; // User changed
  bool dinoIsWalkFrame1 = true;
  unsigned long lastDinoWalkTime = 0;
  const int dinoWalkInterval = 180; // milliseconds for walk animation frame

  // Obstacle properties
  struct Obstacle {
    int x;
    int y;
    int width;
    int height;
    bool isBird;
    const unsigned char* bmp;
    // Bird animation properties
    bool isFrame1; // For bird animation
    unsigned long lastAnimTime; // For bird animation
    // animInterval is global BIRD_ANIM_INTERVAL for now
  };
  std::vector<Obstacle> obstacles;
  int obstacleSpeed = 2;
  unsigned long lastObstacleSpawnTime = 0;
  unsigned long obstacleSpawnInterval = 2000;

  // Cloud properties
  struct Cloud {
    float x; // Use float for smoother movement
    int y;
    float speed;
    const unsigned char* bmp;
    int width;
    int height;
  };
  std::vector<Cloud> clouds;
  const int MAX_CLOUDS = 3;
  const float CLOUD_MIN_SPEED = 0.2f;
  const float CLOUD_MAX_SPEED = 0.5f;

  // Game state
  bool gameOver = false;
  int score = 0;
  unsigned long lastScoreIncrementTime = 0;
  const unsigned long scoreIncrementInterval = 100;
}

// Crossy Road Game Variables
namespace CrossyRoadGame {
  // --- Bitmaps for Crossy Road ---
  // Chicken Frame 1 (8w x 8h)
  const unsigned char chicken_frame1_bmp[] PROGMEM = {
    0b00111100, //   ****
    0b01111110, //  ******
    0b11111111, // ********
    0b11011011, // ** * ** (eyes)
    0b11111111, // ********
    0b01111110, //  ******
    0b00100100, //   *  *
    0b00100100  //   *  * (feet)
  };
  // Chicken Frame 2 (8w x 8h)
  const unsigned char chicken_frame2_bmp[] PROGMEM = {
    0b00111100, //   ****
    0b01111110, //  ******
    0b11111111, // ********
    0b11011011, // ** * ** (eyes)
    0b11111111, // ********
    0b01111110, //  ******
    0b01000010, //  *    * (feet spread)
    0b01000010  //  *    * 
  };
  const int CHICKEN_WIDTH = 8;
  const int CHICKEN_HEIGHT = 8;

  // Car Type 1 (Sedan-like, 18w x 10h)
  const unsigned char car_type1_bmp[] PROGMEM = {
    0b00000000, 0b00000000, 0b00000000, //Line 0
    0b00011111, 0b11111000, 0b00000000, //   ************ (roof)
    0b00111111, 0b11111110, 0b00000000, //  **************
    0b01111111, 0b11111111, 0b10000000, // ******************
    0b11111111, 0b11111111, 0b11000000, // ******************** (body)
    0b11111111, 0b11111111, 0b11000000, // ********************
    0b01111111, 0b11111111, 0b10000000, // ******************
    0b00000000, 0b00000000, 0b00000000, //Line 7 (gap)
    0b00110000, 0b00001100, 0b00000000, //   **      **   (wheels)
    0b00110000, 0b00001100, 0b00000000  //   **      **
  };
  const int CAR_TYPE1_WIDTH = 18;
  const int CAR_TYPE1_HEIGHT = 10;

  // Car speed variables (will be initialized in reset)
  int currentCarMinSpeed;
  int currentCarMaxSpeed;
  const int BASE_CAR_MIN_SPEED = 1;
  const int BASE_CAR_MAX_SPEED = 2;
  const int MAX_POSSIBLE_CAR_MIN_SPEED = 3;
  const int MAX_POSSIBLE_CAR_MAX_SPEED = 4;

  // --- End Bitmaps ---

  // Player (Chicken)
  int playerX, playerY;
  bool chickenIsFrame1 = true;
  unsigned long lastChickenAnimTime = 0;
  const int chickenAnimInterval = 250; // ms for chicken animation frame

  // Obstacles (Cars)
  struct Car {
    int x, y;         // Position (y is lane y-coordinate)
    int width, height;
    int speed;        // Can be negative for leftward movement
    const unsigned char* bmp; // Bitmap for the car
  };
  std::vector<Car> cars;
  const int numLanes = 5;
  int laneYPositions[numLanes];
  const int topSafeZoneY = 5; // Y-coordinate chicken needs to reach
  unsigned long lastPlayerMoveTime = 0;
  const unsigned long playerMoveCooldown = 150; // ms between player moves

  // Game State
  bool gameOverCR = false;
  int scoreCR = 0;

  // Forward declarations for Crossy Road
  void resetCrossyRoadGame();
  void handleCrossyRoadInput();
  void drawPlayerCR();
  void drawObstaclesCR();
  void updateObstaclesCR();
  void checkCollisionsCR();
  void checkWinConditionCR();
  void displayGameOverCR();
} // namespace CrossyRoadGame

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
void runOnlineGame();
void checkJoystickEvents();
void drawGroundLine();
void drawClouds();    // New prototype
void updateClouds();  // New prototype
void initializeOnlineGame();
void cleanupOnlineGame(); // <<< ADDED THIS

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // WebSocket server on path "/ws"

// Access Point credentials
const char* ssid = "S2speed";
const char* password = "Jalal123"; // Min 8 characters

// Online Game variables
struct Player {
  String id;
  String name;
  float x;
  float y;
  bool active;
  unsigned long spottedTime;
  bool isSpotted;
};

std::vector<Player> players;
bool gameStarted = false;
unsigned long gameStartTime = 0;
const unsigned long gameDuration = 180000; // 3 minutes in milliseconds
float adminX = 64.0; // Admin spotlight center X (default to center)
float adminY = 32.0; // Admin spotlight center Y (default to center)
const float spotlightRadius = 15.0; // Radius of admin's spotlight
bool adminWon = false;

// Forward declarations for WebSocket functions
void handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len);
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void notifyClients();
void broadcastGameState();

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(JOY_SW_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT); // Set LED_PIN as OUTPUT
  digitalWrite(LED_PIN, LOW); // Initially turn LED off

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

  // Initialize SPIFFS for serving web files
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  resetDinoGame(); // Initialize Dino game variables
  CrossyRoadGame::resetCrossyRoadGame(); // Initialize Crossy Road game variables

  // --- Online Game Web Server Setup ---
  // WebSocket event handler
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  // Route for game page
  server.on("/game", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/game.html", "text/html");
  });

  // Route for JavaScript and CSS files
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
  
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/script.js", "application/javascript");
  });

  // Route for login
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("playerName", true)) {
      String playerName = request->getParam("playerName", true)->value();
      Serial.print("Player joined: ");
      Serial.println(playerName);
      
      // For now, just redirect to the game page
      // The actual player registration will happen via WebSocket
      request->redirect("/game");
    } else {
      request->redirect("/");
    }
  });
  
  Serial.println("HTTP server routes configured.");
  // server.begin(); // Will be started in initializeOnlineGame when AP is up
  // --- End Online Game Web Server Setup ---
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
      if (CrossyRoadGame::gameOverCR) {
        CrossyRoadGame::displayGameOverCR();
        CrossyRoadGame::handleCrossyRoadInput(); // To catch restart
      } else {
        CrossyRoadGame::handleCrossyRoadInput();
        runCrossyRoad(); // This will call the internal logic
      }
      break;
    case AIRPLANE:
      // Placeholder for Airplane
      runAirplane();
      break;
    case TETRIS:
      // Placeholder for Tetris
      runTetris();
      break;
    case ONLINE_GAME:
      runOnlineGame();
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
        if (currentGameState == ONLINE_GAME) {
          cleanupOnlineGame();
        }
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

// Forward declaration of Tetris namespace and its initialize_game function
namespace Tetris {
    void initialize_game();
}

void handleMenuInput() {
  int joyYVal = analogRead(JOY_VRY_PIN);
  static unsigned long lastInputTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastInputTime < 150) { // Input cooldown for navigation
    return;
  }
  
  if (joyYVal < JOYSTICK_THRESHOLD_LOW) { // Up
    selectedGame = (selectedGame - 1 + numGames) % numGames;
    lastInputTime = currentTime;
  } else if (joyYVal > JOYSTICK_THRESHOLD_HIGH) { // Down
    selectedGame = (selectedGame + 1) % numGames;
    lastInputTime = currentTime;
  }

  if (singleClickForMenu) {
    GameState newGameState = static_cast<GameState>(selectedGame + 1);
    if (newGameState == GOOGLE_DINO) {
            resetDinoGame();
    } else if (newGameState == CROSSY_ROAD) {
        CrossyRoadGame::resetCrossyRoadGame();
    } else if (newGameState == TETRIS) {
        Tetris::initialize_game(); // Tetris namespace is now defined before this function
    } else if (newGameState == ONLINE_GAME) {
        initializeOnlineGame();
    }

    currentGameState = newGameState; 
    Serial.print("Selected game (single click): "); Serial.println(gameNames[selectedGame]);
    
    clickCount = 0;          
    singleClickForMenu = false; 
    lastJoySWPressTime = millis(); 
  }
}


// --- Google Dino Game ---
void resetDinoGame() {
  using namespace DinoGame;
  dinoX = 10;
  dinoBaseY = groundLineY - DINO_STAND_HEIGHT;
  dinoY = dinoBaseY;
  isJumping = false;
  isDucking = false;
  jumpVelocity = 1; // Keep user change
  obstacles.clear();
  gameOver = false;
  score = 0;
  obstacleSpeed = 2;
  lastObstacleSpawnTime = millis();
  obstacleSpawnInterval = 2000 + random(0,1000);
  dinoIsWalkFrame1 = true;
  lastDinoWalkTime = millis();

  // Initialize clouds
  clouds.clear();
  for (int i = 0; i < MAX_CLOUDS; ++i) {
    Cloud newCloud;
    newCloud.x = random(0, SCREEN_WIDTH + SCREEN_WIDTH / 2); // Some on screen, some off to the right
    newCloud.y = random(5, groundLineY - DINO_STAND_HEIGHT - CLOUD_1_HEIGHT - 10); // Above dino's head range
    newCloud.speed = (random( (int)(CLOUD_MIN_SPEED * 100), (int)(CLOUD_MAX_SPEED * 100) ) / 100.0f);
    newCloud.bmp = cloud_1_bmp;
    newCloud.width = CLOUD_1_WIDTH;
    newCloud.height = CLOUD_1_HEIGHT;
    clouds.push_back(newCloud);
  }
}

void drawDino() {
  using namespace DinoGame;
  display.setTextColor(SH110X_WHITE);

  const unsigned char* currentBmp;
  int currentWidth, currentHeight;
  int currentDrawY = dinoY;

  if (isDucking) {
    currentBmp = dino_duck_bmp;
    currentWidth = DINO_DUCK_WIDTH;
    currentHeight = DINO_DUCK_HEIGHT;
    currentDrawY = groundLineY - DINO_DUCK_HEIGHT;
    if (isJumping) { 
        currentDrawY = dinoY; 
    }
  } else {
    currentWidth = DINO_STAND_WIDTH;
    currentHeight = DINO_STAND_HEIGHT;
    if (!isJumping) { // Only animate walking if on the ground
      if (millis() - lastDinoWalkTime > dinoWalkInterval) {
        dinoIsWalkFrame1 = !dinoIsWalkFrame1;
        lastDinoWalkTime = millis();
      }
      currentBmp = dinoIsWalkFrame1 ? dino_stand_frame1_bmp : dino_stand_frame2_bmp;
      currentDrawY = groundLineY - DINO_STAND_HEIGHT; // Ensure on ground if not jumping
    } else { // Jumping
      currentBmp = dino_stand_frame1_bmp; // Static frame while jumping
      // currentDrawY is already dinoY (dynamic)
    }
  }
  display.drawBitmap(dinoX, currentDrawY, currentBmp, currentWidth, currentHeight, SH110X_WHITE);
}

void drawObstacles() {
  using namespace DinoGame;
  display.setTextColor(SH110X_WHITE);
  for (const auto& obs : obstacles) {
    if (obs.bmp) {
        display.drawBitmap(obs.x, obs.y, obs.bmp, obs.width, obs.height, SH110X_WHITE);
    } else if (obs.isBird) {
        display.fillRect(obs.x, obs.y, obs.width, obs.height, SH110X_WHITE);
    } else {
    display.fillRect(obs.x, obs.y, obs.width, obs.height, SH110X_WHITE);
    }
  }
}

void updateObstacles() {
  using namespace DinoGame;
  unsigned long currentTime = millis();

  for (size_t i = 0; i < obstacles.size(); ++i) {
    obstacles[i].x -= obstacleSpeed;

    // Update bird animation frame
    if (obstacles[i].isBird && (currentTime - obstacles[i].lastAnimTime > BIRD_ANIM_INTERVAL)) {
      obstacles[i].isFrame1 = !obstacles[i].isFrame1;
      obstacles[i].bmp = obstacles[i].isFrame1 ? bird_frame1_bmp : bird_frame2_bmp;
      obstacles[i].lastAnimTime = currentTime;
    }

    if (obstacles[i].x + obstacles[i].width < 0) {
      obstacles.erase(obstacles.begin() + i);
      i--;
    }
  }

  if (currentTime - lastObstacleSpawnTime > obstacleSpawnInterval) {
    lastObstacleSpawnTime = currentTime;
    obstacleSpawnInterval = 1800 + random(0, 1200) - (score * 15);
    if (obstacleSpawnInterval < 600) obstacleSpawnInterval = 600;

    Obstacle newObs;
    newObs.x = SCREEN_WIDTH;

    if (random(0, 3) == 0) { // 1 in 3 chance for a bird (re-enabled)
        newObs.isBird = true;
        newObs.width = BIRD_WIDTH; 
        newObs.height = BIRD_HEIGHT;
        newObs.isFrame1 = true;
        newObs.bmp = bird_frame1_bmp;
        newObs.lastAnimTime = currentTime;
        // Bird Y position logic:
        if (random(0,2) == 0) { // High bird (duck or precise jump)
             // Top of bird Y = dino's base Y (top of standing dino) - bird height - small gap
             newObs.y = dinoBaseY - BIRD_HEIGHT - random(3, 8);
        } else { // Low bird (jumpable by standing, might hit ducking)
             // Top of bird Y = ground - bird_height - some space dino can clear
             newObs.y = groundLineY - BIRD_HEIGHT - (DINO_STAND_HEIGHT / 2) + random(0,5) ;
             // Ensure it's not too low to hit a non-jumping dino on ground
             if (newObs.y + BIRD_HEIGHT > groundLineY - DINO_DUCK_HEIGHT + 3) { // if bird bottom is lower than top of ducked dino + buffer
                newObs.y = groundLineY - BIRD_HEIGHT - DINO_DUCK_HEIGHT - random(3,6); // Place it clearly above ducking dino
             }
        }
        // Ensure bird is not off the top of the screen
        if (newObs.y < 0) newObs.y = 0; 

    } else { // Cactus
        newObs.isBird = false;
        newObs.bmp = cactus_1_bmp;
        newObs.width = CACTUS_1_WIDTH;
        newObs.height = CACTUS_1_HEIGHT;
        newObs.y = groundLineY - newObs.height;
    }
    obstacles.push_back(newObs);
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
  int currentDinoCollisionY;
  int currentDinoCollisionWidth;
  int currentDinoCollisionHeight;

  if (isDucking) {
    currentDinoCollisionWidth = DINO_DUCK_WIDTH;
    currentDinoCollisionHeight = DINO_DUCK_HEIGHT;
    currentDinoCollisionY = groundLineY - DINO_DUCK_HEIGHT;
     if (isJumping) currentDinoCollisionY = dinoY;
  } else {
    currentDinoCollisionWidth = DINO_STAND_WIDTH;
    currentDinoCollisionHeight = DINO_STAND_HEIGHT;
    currentDinoCollisionY = dinoY;
  }

  for (const auto& obs : obstacles) {
    if (dinoX < obs.x + obs.width &&
        dinoX + currentDinoCollisionWidth > obs.x &&
        currentDinoCollisionY < obs.y + obs.height &&
        currentDinoCollisionY + currentDinoCollisionHeight > obs.y) {
      gameOver = true;
      Serial.println("Collision with Dino Game object!");
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

  if (isJumping) {
    dinoY += jumpVelocity;
    jumpVelocity += gravity;
    if (dinoY >= (groundLineY - (isDucking ? DINO_DUCK_HEIGHT : DINO_STAND_HEIGHT))) {
      dinoY = groundLineY - (isDucking ? DINO_DUCK_HEIGHT : DINO_STAND_HEIGHT);
      isJumping = false;
      jumpVelocity = 1; // Reset to initial jump impulse value (user changed)
      if (isDucking) {
          dinoY = groundLineY - DINO_DUCK_HEIGHT;
      } else {
          dinoY = groundLineY - DINO_STAND_HEIGHT;
      }
    }
  } else if (!isDucking) {
     dinoY = groundLineY - DINO_STAND_HEIGHT;
  } else { 
     dinoY = groundLineY - DINO_DUCK_HEIGHT;
  }

  updateObstacles();
  updateClouds(); // Update cloud positions
  checkCollisions();
  
  if (!gameOver) {
    if(millis() - lastScoreIncrementTime > scoreIncrementInterval) {
        score++;
        lastScoreIncrementTime = millis();
        if (score > 0 && score % 8 == 0) { 
            if (obstacleSpeed < 6) obstacleSpeed++;
        }
        if (score > 0 && score % 12 == 0) {
            if (obstacleSpawnInterval > 650) obstacleSpawnInterval -= 75;
        }
    }
  }

  display.clearDisplay();
  drawClouds();      // Draw clouds first (background)
  drawGroundLine();
  drawDino();
  drawObstacles();

  display.setCursor(SCREEN_WIDTH - 40, 0);
  display.print("S: ");
  display.println(score);
}

// New function to draw the ground line
void drawGroundLine() {
  using namespace DinoGame; // To access groundLineY
  display.drawLine(0, groundLineY, SCREEN_WIDTH - 1, groundLineY, SH110X_WHITE);
}

// New function to draw clouds
void drawClouds() {
  using namespace DinoGame;
  for (const auto& cloud : clouds) {
    display.drawBitmap((int)cloud.x, cloud.y, cloud.bmp, cloud.width, cloud.height, SH110X_WHITE);
  }
}

// New function to update clouds
void updateClouds() {
  using namespace DinoGame;
  for (auto& cloud : clouds) {
    cloud.x -= cloud.speed;
    if (cloud.x + cloud.width < 0) { // If cloud moved off screen to the left
      cloud.x = SCREEN_WIDTH + random(0, SCREEN_WIDTH / 4); // Reappear on the right
      cloud.y = random(5, groundLineY - DINO_STAND_HEIGHT - CLOUD_1_HEIGHT - 10); // New random Y
      cloud.speed = (random( (int)(CLOUD_MIN_SPEED * 100), (int)(CLOUD_MAX_SPEED * 100) ) / 100.0f); // New random speed
    }
  }
}


// --- Crossy Road Game Functions ---
void CrossyRoadGame::resetCrossyRoadGame() {
  playerX = SCREEN_WIDTH / 2 - CHICKEN_WIDTH / 2;
  playerY = SCREEN_HEIGHT - CHICKEN_HEIGHT - 2; // Start at the bottom
  scoreCR = 0;
  gameOverCR = false;
  cars.clear();
  chickenIsFrame1 = true;
  lastChickenAnimTime = millis();

  // Initialize/reset car speeds for a new game
  currentCarMinSpeed = BASE_CAR_MIN_SPEED;
  currentCarMaxSpeed = BASE_CAR_MAX_SPEED;

  // Initialize lane Y positions (top-down)
  int laneHeightSpace = (SCREEN_HEIGHT - topSafeZoneY - CHICKEN_HEIGHT - 10) / numLanes; 
  for (int i = 0; i < numLanes; ++i) {
    laneYPositions[i] = topSafeZoneY + 5 + (i * laneHeightSpace) + CAR_TYPE1_HEIGHT / 2 ; // Center default car height
  }

  // Create cars for each lane
  for (int i = 0; i < numLanes; ++i) {
    Car newCar;
    newCar.bmp = car_type1_bmp; // Assign default car bitmap
    newCar.width = CAR_TYPE1_WIDTH;
    newCar.height = CAR_TYPE1_HEIGHT;
    newCar.y = laneYPositions[i] - newCar.height/2; 
    
    newCar.speed = random(currentCarMinSpeed, currentCarMaxSpeed + 1);
    if (newCar.speed == 0) newCar.speed = (random(0,2) == 0) ? currentCarMinSpeed : -currentCarMinSpeed; // Ensure non-zero speed
    if (random(0, 2) == 0) { 
      newCar.speed *= -1;
    }
    if (newCar.speed > 0) { 
        newCar.x = random(0, SCREEN_WIDTH / 2) * -1 - newCar.width; // Start off-screen to the left
    } else { 
        newCar.x = SCREEN_WIDTH + random(0, SCREEN_WIDTH / 2); // Start off-screen to the right
    }
    cars.push_back(newCar);

    if (i % 2 != 0 && numLanes > 2) { 
      Car newCar2 = newCar; // Properties will be based on the first car in this lane
      // Ensure it doesn't overlap too much with the first car initially
      if (newCar2.speed > 0) { 
          newCar2.x = newCar.x - newCar.width - random(40, SCREEN_WIDTH/2);
      } else {
          newCar2.x = newCar.x + newCar.width + random(40, SCREEN_WIDTH/2);
      }
      // Ensure it's still in a reasonable starting range relative to the first car
      if (newCar2.x < -newCar2.width - SCREEN_WIDTH) newCar2.x = -newCar2.width - random(10,50);
      if (newCar2.x > SCREEN_WIDTH * 2) newCar2.x = SCREEN_WIDTH + random(10,50);
      cars.push_back(newCar2);
    }
  }
  lastPlayerMoveTime = millis(); // Allow immediate first move
}

void CrossyRoadGame::drawPlayerCR() {
  // Animate chicken
  if (millis() - lastChickenAnimTime > chickenAnimInterval) {
    chickenIsFrame1 = !chickenIsFrame1;
    lastChickenAnimTime = millis();
  }
  const unsigned char* currentPlayerBmp = chickenIsFrame1 ? chicken_frame1_bmp : chicken_frame2_bmp;
  display.drawBitmap(playerX, playerY, currentPlayerBmp, CHICKEN_WIDTH, CHICKEN_HEIGHT, SH110X_WHITE);
}

void CrossyRoadGame::drawObstaclesCR() {
  for (const auto& car : cars) {
    if (car.bmp) {
        display.drawBitmap(car.x, car.y, car.bmp, car.width, car.height, SH110X_WHITE);
    } else { // Fallback if somehow a car has no bitmap
        display.fillRect(car.x, car.y, car.width, car.height, SH110X_WHITE);
    }
  }
}

void CrossyRoadGame::updateObstaclesCR() {
  for (auto& car : cars) {
    car.x += car.speed;
    
    // Adjust speed if it falls outside current dynamic range (e.g. after score increase)
    // This helps existing cars adapt slightly to difficulty changes.
    int absSpeed = abs(car.speed);
    if (absSpeed < currentCarMinSpeed) {
        car.speed = (car.speed > 0) ? currentCarMinSpeed : -currentCarMinSpeed;
    } else if (absSpeed > currentCarMaxSpeed) {
        car.speed = (car.speed > 0) ? currentCarMaxSpeed : -currentCarMaxSpeed;
    }
    if (car.speed == 0) { // safety check, should not happen if spawn logic is good
        car.speed = (random(0,2) == 0) ? currentCarMinSpeed : -currentCarMinSpeed;
    }


    // Wrap around logic with difficulty scaling for respawn gap
    int maxRespawnGap = 50 - (scoreCR * 2); // Gap decreases as score increases
    if (maxRespawnGap < 10) maxRespawnGap = 10; // Minimum gap
    int minRespawnGap = 5 - (scoreCR / 2);
    if (minRespawnGap < 1) minRespawnGap = 1;
    if (minRespawnGap >= maxRespawnGap) minRespawnGap = maxRespawnGap -1; 
    if (minRespawnGap <=0) minRespawnGap = 1;

    if (car.speed > 0 && car.x > SCREEN_WIDTH) { // Moving right, off right edge
      car.x = -car.width - random(minRespawnGap, maxRespawnGap + 1); 
      // Potentially re-evaluate speed based on current difficulty when wrapping
      car.speed = random(currentCarMinSpeed, currentCarMaxSpeed + 1);
      if (car.speed == 0) car.speed = currentCarMinSpeed;
      if (random(0,10) < 2 && abs(car.speed) > currentCarMinSpeed) car.speed = (car.speed/abs(car.speed)) * currentCarMinSpeed; // 20% chance to slow down to min

    } else if (car.speed < 0 && car.x + car.width < 0) { // Moving left, off left edge
      car.x = SCREEN_WIDTH + random(minRespawnGap, maxRespawnGap + 1); 
      // Potentially re-evaluate speed
      car.speed = -random(currentCarMinSpeed, currentCarMaxSpeed + 1);
      if (car.speed == 0) car.speed = -currentCarMinSpeed;
      if (random(0,10) < 2 && abs(car.speed) > currentCarMinSpeed) car.speed = (car.speed/abs(car.speed)) * currentCarMinSpeed; // 20% chance to slow down to min
    }
  }
}

void CrossyRoadGame::checkCollisionsCR() {
  for (const auto& car : cars) {
    // AABB collision detection (player is CHICKEN_WIDTH x CHICKEN_HEIGHT)
    if (playerX < car.x + car.width &&
        playerX + CHICKEN_WIDTH > car.x &&
        playerY < car.y + car.height &&
        playerY + CHICKEN_HEIGHT > car.y) {
      gameOverCR = true;
      Serial.println("Crossy Road: Collision!");
      return;
    }
  }
}

void CrossyRoadGame::checkWinConditionCR() {
  if (playerY <= topSafeZoneY) {
    scoreCR++;
    Serial.print("Crossy Road: Reached top! Score: "); Serial.println(scoreCR);

    // Increase difficulty based on score
    if (scoreCR % 2 == 0 && currentCarMinSpeed < MAX_POSSIBLE_CAR_MIN_SPEED) { // Every 2 points
        currentCarMinSpeed++;
        Serial.print("Min Car Speed increased to: "); Serial.println(currentCarMinSpeed);
    }
    if (scoreCR % 3 == 0 && currentCarMaxSpeed < MAX_POSSIBLE_CAR_MAX_SPEED) { // Every 3 points
        currentCarMaxSpeed++;
        Serial.print("Max Car Speed increased to: "); Serial.println(currentCarMaxSpeed);
        // Ensure min speed doesn't exceed max speed
        if (currentCarMinSpeed > currentCarMaxSpeed) {
            currentCarMinSpeed = currentCarMaxSpeed;
        }
    }

    // Reset player to start for another run
    playerX = SCREEN_WIDTH / 2 - CHICKEN_WIDTH / 2;
    playerY = SCREEN_HEIGHT - CHICKEN_HEIGHT - 2;
    // Optionally, could increase difficulty here (e.g., car speeds)
    lastPlayerMoveTime = millis(); // Allow immediate move after scoring
  }
}

void CrossyRoadGame::handleCrossyRoadInput() {
  unsigned long currentTime = millis();

  if (gameOverCR) {
    // Simplified click detection for restart (similar to Dino game)
    int currentJoySWStateForRestart = digitalRead(JOY_SW_PIN);
    static int lastJoySWStateForRestartCR = HIGH;
    static unsigned long lastPressTimeForRestartCR = 0;

    if (currentJoySWStateForRestart == LOW && lastJoySWStateForRestartCR == HIGH && (currentTime - lastPressTimeForRestartCR > debounceDelay)) {
        lastPressTimeForRestartCR = currentTime;
        Serial.println("Crossy Road: Restarting game.");
        resetCrossyRoadGame(); 
    }
    lastJoySWStateForRestartCR = currentJoySWStateForRestart;
    return;
  }

  if (currentTime - lastPlayerMoveTime > playerMoveCooldown) {
    int joyXVal = analogRead(JOY_VRX_PIN);
    int joyYVal = analogRead(JOY_VRY_PIN);

    bool moved = false;
    int playerStep = CHICKEN_HEIGHT; // Move one chicken height at a time

    if (joyYVal < JOYSTICK_THRESHOLD_LOW) { // Up
      playerY -= playerStep;
      moved = true;
    } else if (joyYVal > JOYSTICK_THRESHOLD_HIGH) { // Down
      // Prevent moving off bottom of screen if already at start
      if (playerY + playerStep + CHICKEN_HEIGHT <= SCREEN_HEIGHT) {
        playerY += playerStep;
      }
      moved = true;
    } else if (joyXVal < JOYSTICK_THRESHOLD_LOW) { // Left
      playerX -= playerStep;
      moved = true;
    } else if (joyXVal > JOYSTICK_THRESHOLD_HIGH) { // Right
      playerX += playerStep;
      moved = true;
    }

    if (moved) {
      // Constrain player to screen boundaries
      if (playerX < 0) playerX = 0;
      if (playerX + CHICKEN_WIDTH > SCREEN_WIDTH) playerX = SCREEN_WIDTH - CHICKEN_WIDTH;
      if (playerY < 0) playerY = 0; 
      // if (playerY + CHICKEN_HEIGHT > SCREEN_HEIGHT) playerY = SCREEN_HEIGHT - CHICKEN_HEIGHT; // Already handled by start pos & down move check
      // Ensure player cannot go below starting Y position unless they started there
      int startY = SCREEN_HEIGHT - CHICKEN_HEIGHT - 2;
      if (playerY > startY) playerY = startY;

      lastPlayerMoveTime = currentTime;
    }
  }
}

void CrossyRoadGame::displayGameOverCR() {
  display.clearDisplay();
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(2);
  const char* gameOverText = "Game Over";
  display.getTextBounds(gameOverText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 - 15);
  display.println(gameOverText);

  display.setTextSize(1);
  String scoreStr = "Score: " + String(scoreCR);
  display.getTextBounds(scoreStr.c_str(), 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 + 5);
  display.println(scoreStr);

  const char* restartText = "Click to Restart";
  display.getTextBounds(restartText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 + 15);
  display.println(restartText);
}

void runCrossyRoad() {
  using namespace CrossyRoadGame;
  // Note: Input handling is called from the main loop based on gameOverCR state
  
  updateObstaclesCR();
  checkCollisionsCR(); // Check collisions after moving obstacles and before player moves (or after)
  
  if (!gameOverCR) {
    checkWinConditionCR(); // Check if player reached the top
  }

  // Drawing
  display.clearDisplay();
  drawPlayerCR();
  drawObstaclesCR();

  // Display Score during gameplay
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(SCREEN_WIDTH - 30, 0);
  display.print("S:");
  display.println(scoreCR);

  // display.display() is called in the main loop()
}

// --- Placeholder Game Functions ---
void runAirplane() {
  display.clearDisplay();
  display.setCursor(10, SCREEN_HEIGHT / 2 - 5);
  display.setTextSize(1);
  display.println("Airplane - WIP");
  // display.display() is called in loop()
}

// --- Placeholder for Online Game ---
void initializeOnlineGame() {
  Serial.println("Initializing Online Game...");
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.println("Starting AP...");
  display.display();

  // Disconnect from any existing WiFi connection
  WiFi.disconnect(true);
  delay(1000);

  // Configure AP mode
  WiFi.mode(WIFI_AP);
  delay(100);

  // Start AP with proper configuration
  bool ap_started = WiFi.softAP(ssid, password, 1, 0, 4); // Channel 1, hidden=false, max_connection=4
  if (!ap_started) {
    Serial.println("Failed to start AP!");
    display.setCursor(5, 15);
    display.println("AP Start Failed!");
    display.display();
    delay(2000);
    cleanupOnlineGame(); // <<< ADDED THIS
    currentGameState = MENU;
    return;
  }

  IPAddress AP_IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(AP_IP);
  
  display.setCursor(5, 15);
  display.print("AP IP: ");
  display.println(AP_IP);
  display.setCursor(5, 25);
  display.print("Connect to ");
  display.println(ssid);
  display.display();

  // Reset game state
  players.clear();
  gameStarted = false;
  adminX = 64.0;
  adminY = 32.0;
  adminWon = false;

  // Start server with proper error handling
  try {
    server.begin();
    Serial.println("HTTP server started");
    display.setCursor(5, 35);
    display.println("Server Ready!");
    display.display();
    digitalWrite(LED_PIN, HIGH);
  } catch (const std::exception& e) {
    Serial.print("Server start failed: ");
    Serial.println(e.what());
    display.setCursor(5, 35);
    display.println("Server Start Failed!");
    display.display();
    delay(2000);
    cleanupOnlineGame(); // <<< ADDED THIS
    currentGameState = MENU;
    return;
  }
}

void runOnlineGame() {
  static unsigned long lastUpdateTime = 0;
  static unsigned long lastSpotCheckTime = 0;
  static unsigned long lastBroadcastTime = 0;
  unsigned long currentTime = millis();
  
  // Handle joystick input to move admin spotlight
  int joyXVal = analogRead(JOY_VRX_PIN);
  int joyYVal = analogRead(JOY_VRY_PIN);
  
  // Update admin position based on joystick with a dead zone
  if (currentTime - lastUpdateTime > 50) { // Update at 20Hz
    float adminSpeed = 2.0;
    
    if (joyXVal < JOYSTICK_DEADZONE_LOW) {
      adminX -= adminSpeed;
    } else if (joyXVal > JOYSTICK_DEADZONE_HIGH) {
      adminX += adminSpeed;
    }
    
    if (joyYVal < JOYSTICK_DEADZONE_LOW) {
      adminY -= adminSpeed;
    } else if (joyYVal > JOYSTICK_DEADZONE_HIGH) {
      adminY += adminSpeed;
    }
    
    // Constrain admin spotlight within the screen
    if (adminX < spotlightRadius) adminX = spotlightRadius;
    if (adminX > SCREEN_WIDTH - spotlightRadius) adminX = SCREEN_WIDTH - spotlightRadius;
    if (adminY < spotlightRadius) adminY = spotlightRadius;
    if (adminY > SCREEN_HEIGHT - spotlightRadius) adminY = SCREEN_HEIGHT - spotlightRadius;
    
    lastUpdateTime = currentTime;
  }
  
  // Check for spotted players every 100ms
  if (currentTime - lastSpotCheckTime > 100) {
    bool anyPlayerSpotted = false;
    
    // Process each player
    for (auto& player : players) {
      if (!player.active) continue; // Skip eliminated players
      
      // Calculate distance between admin spotlight and player
      float dx = player.x - adminX;
      float dy = player.y - adminY;
      float distance = sqrt(dx*dx + dy*dy);
      
      // Check if player is within spotlight radius
      if (distance < spotlightRadius) {
        anyPlayerSpotted = true;
        
        // If player wasn't previously spotted, start timing
        if (!player.isSpotted) {
          player.isSpotted = true;
          player.spottedTime = currentTime;
        } 
        // If player has been spotted for 3 seconds, eliminate them
        else if (currentTime - player.spottedTime > 3000) {
          player.active = false;
          player.isSpotted = false;
          
          // Check if all players are eliminated (admin wins)
          bool allEliminated = true;
          for (const auto& p : players) {
            if (p.active) {
              allEliminated = false;
              break;
            }
          }
          
          if (allEliminated) {
            adminWon = true;
            gameStarted = false;
          }
        }
      } else {
        // Player moved out of spotlight
        player.isSpotted = false;
      }
    }
    
    // Turn on LED if any player is spotted
    digitalWrite(LED_PIN, anyPlayerSpotted ? HIGH : LOW);
    
    lastSpotCheckTime = currentTime;
  }
  
  // Broadcast game state to all clients every 100ms
  if (currentTime - lastBroadcastTime > 100) {
    broadcastGameState();
    lastBroadcastTime = currentTime;
  }
  
  // Handle game timeout
  if (gameStarted && (currentTime - gameStartTime) > gameDuration) {
    gameStarted = false;
    // Players win if any are still active
    bool playersWon = false;
    for (const auto& player : players) {
      if (player.active) {
        playersWon = true;
        break;
      }
    }
    adminWon = !playersWon;
  }
  
  // Update OLED display with game status
  display.clearDisplay();
  display.setTextSize(1);
  
  if (!gameStarted) {
    display.setCursor(5, 0);
    display.print("Players: ");
    display.println(players.size());
    
    display.setCursor(5, 10);
    if (players.size() >= 2) {
      display.println("Ready to start");
    } else {
      display.println("Need 2+ players");
    }
    
    display.setCursor(5, 20);
    display.print("Press button");
    display.setCursor(5, 30);
    display.print("to start game");
  } else {
    // Draw game timer
    unsigned long elapsedTime = currentTime - gameStartTime;
    if (elapsedTime > gameDuration) elapsedTime = gameDuration;
    unsigned long remainingTime = (gameDuration - elapsedTime) / 1000;
    
    display.setCursor(5, 0);
    display.print("Time: ");
    display.print(remainingTime);
    display.print("s");
    
    display.setCursor(5, 10);
    display.print("Active: ");
    int activePlayers = 0;
    for (const auto& player : players) {
      if (player.active) activePlayers++;
    }
    display.println(activePlayers);
    
    // Draw admin's spotlight position
    display.drawCircle(adminX, adminY, spotlightRadius, SH110X_WHITE);
  }
  
  display.display();
  
  // Check for joystick button press to start game
  if (!gameStarted && players.size() >= 2) {
    int joySWState = digitalRead(JOY_SW_PIN);
    static int lastJoySWState = HIGH;
    static unsigned long lastJoyPressTime = 0;
    
    if (joySWState == LOW && lastJoySWState == HIGH && (currentTime - lastJoyPressTime > 500)) {
      // Button pressed, start game
      gameStarted = true;
      gameStartTime = currentTime;
      Serial.println("Game started by admin!");
      lastJoyPressTime = currentTime;
      broadcastGameState();
    }
    
    lastJoySWState = joySWState;
  }
}

void cleanupOnlineGame() {
  Serial.println("Cleaning up Online Game...");
  ws.closeAll(); // Close all WebSocket connections
  ws.cleanupClients(); // Cleanup WebSocket clients
  server.end(); // Stop the web server
  WiFi.softAPdisconnect(true); // Disconnect clients and stop the AP
  WiFi.mode(WIFI_OFF); // Turn off WiFi
  digitalWrite(LED_PIN, LOW); // Turn off LED
  Serial.println("Online Game cleanup complete.");
}

// --- Tetris Game Implementation ---
namespace Tetris {
    // Forward declarations within the namespace
    void clear_completed_lines();
    void draw_game_board();
    void spawn_new_shape(); 
    void initialize_game(); // Also forward declare if its definition is after a call within namespace (it's called by handleMenuInput externally though)


    // Color definitions
    enum Color {
        BLACK = 0, // Must be 0
        WHITE = 1, // Must be 1 for SH110X_WHITE mapping
        BLUE = 2,
        GREEN = 3,
        RED = 4,
        PINK = 5,
        YELLOW = 6
    };

    // Shape definitions
    const bool O[4][4] = {
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0}
    };

    const bool I[4][4] = {
        {0, 0, 0, 0},
        {1, 1, 1, 1},
        {0, 0, 0, 0},
        {0, 0, 0, 0}
    };

    const bool L[4][4] = {
        {0, 0, 0, 0},
        {0, 1, 0, 0}, 
        {0, 1, 0, 0},
        {0, 1, 1, 0}
    };

    const bool J[4][4] = {
        {0, 0, 0, 0},
        {0, 0, 1, 0}, 
        {0, 0, 1, 0},
        {0, 1, 1, 0}
    };

    const bool T[4][4] = {
        {0, 0, 0, 0},
        {0, 1, 1, 1},
        {0, 0, 1, 0},
        {0, 0, 0, 0}
    };

    const bool Z[4][4] = {
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 1},
        {0, 0, 0, 0}
    };

    const bool S[4][4] = {
        {0, 0, 0, 0},
        {0, 0, 1, 1},
        {0, 1, 1, 0},
        {0, 0, 0, 0}
    };

    const bool (*shapes[7])[4][4] = {
        &O, &I, &L, &J, &T, &Z, &S
    };

    int board[20][10] = {0}; 
    int cur_top = 0, cur_left = 0;
    Color cur_color = BLACK;
    bool cur_shape[4][4] = {0};
    bool tetrisGameOver = false;
    unsigned long lastFallTime = 0;
    unsigned long fallInterval = 500; 
    unsigned long lastMoveTime = 0;   
    const unsigned long moveDebounceInterval = 150; 
    unsigned long lastRotateTime = 0; 
    const unsigned long rotateDebounceInterval = 200; 

    bool check_collision(int test_top, int test_left, const bool shape_matrix[4][4]) {
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (shape_matrix[r][c]) { 
                    int board_r = test_top + r;
                    int board_c = test_left + c;

                    if (board_c < 0 || board_c >= 10 || board_r >= 20) {
                        return true; 
                    }
                    if (board_r >=0 && board[board_r][board_c] != BLACK) {
                        return true;
                    }
                }
            }
        }
        return false; 
    }

    void rotate_current_shape() { 
        if (tetrisGameOver) return;
        bool temp_shape[4][4];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                temp_shape[j][3 - i] = cur_shape[i][j];
            }
        }
        if (!check_collision(cur_top, cur_left, temp_shape)) {
            memcpy(cur_shape, temp_shape, sizeof(cur_shape));
        }
    }

    void move_piece_sideways(int direction) { 
        if (tetrisGameOver) return;
        int new_left = cur_left + direction;
        if (!check_collision(cur_top, new_left, cur_shape)) {
            cur_left = new_left;
        }
    }
    
    void draw_game_board() { 
        // Calculate starting position to center the board
        // Board is 10 columns * 4 pixels = 40 pixels wide
        // Board is 16 rows * 4 pixels = 64 pixels high (fits screen height)
        const int board_width = 10 * 4;
        const int board_height = 16 * 4;
        const int start_x = (SCREEN_WIDTH - board_width) / 2;
        const int start_y = 0; // Start from top since we're using full height

        // Draw the board (only visible rows)
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 10; j++) {
                Color block_color_enum = static_cast<Color>(board[i + 4][j]); // Offset by 4 rows to show middle of board
                uint16_t display_color = (block_color_enum == BLACK) ? SH110X_BLACK : SH110X_WHITE;
                display.fillRect(start_x + (j * 4), 
                               start_y + (i * 4), 
                               3, // 3 pixels wide to leave 1px gap
                               3, // 3 pixels high to leave 1px gap
                               display_color);
            }
        }
    }

    void clear_completed_lines() {
        for (int i = 19; i >= 0; i--) { 
            bool line_complete = true;
            for (int j = 0; j < 10; j++) {
                if (board[i][j] == BLACK) {
                    line_complete = false;
                    break;
                }
            }
            if (line_complete) {
                for (int k = i; k > 0; k--) {
                    memcpy(board[k], board[k - 1], sizeof(board[k]));
                }
                memset(board[0], BLACK, sizeof(board[0]));
                i++; 
            }
        }
    }
    
    void spawn_new_shape() { 
        clear_completed_lines();

        int random_shape_index = random(0, 7);
        memcpy(cur_shape, *shapes[random_shape_index], sizeof(cur_shape));
        cur_top = 0; 
        bool shape_starts_lower = true;
        for(int c=0; c<4; ++c) if(cur_shape[0][c]) shape_starts_lower = false;
        if(shape_starts_lower) {
            bool shape_starts_even_lower = true;
            for(int c=0; c<4; ++c) if(cur_shape[1][c]) shape_starts_even_lower = false;
            if(shape_starts_even_lower) cur_top = -2;
            else cur_top = -1;
        }

        cur_left = (10 - 4) / 2; 
        cur_color = static_cast<Color>(random(WHITE, YELLOW + 1)); 

        if (check_collision(cur_top, cur_left, cur_shape)) {
            tetrisGameOver = true;
            Serial.println("Tetris: Game Over!");
            if (cur_top < 0) cur_top = 0; 
            return;
        }
        tetrisGameOver = false; 
    }

    void land_current_piece() {
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (cur_shape[r][c]) {
                    int board_r = cur_top + r;
                    int board_c = cur_left + c;
                    if (board_r >= 0 && board_r < 20 && board_c >= 0 && board_c < 10) {
                        board[board_r][board_c] = cur_color;
                    }
                }
            }
        }
        spawn_new_shape();
    }

    void update_frame() { 
        if (tetrisGameOver) return;
        if (!check_collision(cur_top + 1, cur_left, cur_shape)) {
            cur_top++; 
        } else {
            land_current_piece();
        }
    }

    void render_display() { 
        display.clearDisplay();
        draw_game_board(); 

        if (!tetrisGameOver) {
            // Calculate starting position to match draw_game_board
            const int board_width = 10 * 4;
            const int board_height = 16 * 4;
            const int start_x = (SCREEN_WIDTH - board_width) / 2;
            const int start_y = 0;

            // Draw current piece (adjusted for visible area)
            for (int r = 0; r < 4; r++) {
                for (int c = 0; c < 4; c++) {
                    if (cur_shape[r][c]) {
                        // Only draw if the piece is in the visible area
                        int visible_row = cur_top + r - 4; // Adjust for 4-row offset
                        if (visible_row >= 0 && visible_row < 16) {
                            uint16_t display_color = (cur_color == BLACK) ? SH110X_BLACK : SH110X_WHITE; 
                            display.fillRect(start_x + ((cur_left + c) * 4),
                                           start_y + (visible_row * 4),
                                           3, // 3 pixels wide to leave 1px gap
                                           3, // 3 pixels high to leave 1px gap
                                           display_color);
                        }
                    }
                }
            }
        }
        
        if (tetrisGameOver) {
            display.setTextSize(2);
            int16_t x1, y1;
            uint16_t w, h;
            const char* gameOverText = "Game Over";
            display.getTextBounds(gameOverText, 0, 0, &x1, &y1, &w, &h);
            display.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT / 2 - h / 2);
            display.println(gameOverText);
        }
        display.display();
    }
    
    void initialize_game() {
        memset(board, BLACK, sizeof(board)); 
        tetrisGameOver = false;
        lastFallTime = millis();
        spawn_new_shape(); 
    }

    void handle_tetris_input() {
        if (tetrisGameOver) return;

        unsigned long current_time = millis();
        int joy_x_val = analogRead(JOY_VRX_PIN);
        int joy_y_val = analogRead(JOY_VRY_PIN);
        int joy_sw_state = digitalRead(JOY_SW_PIN);

        if (current_time - lastMoveTime > moveDebounceInterval) {
            if (joy_x_val < JOYSTICK_THRESHOLD_LOW) { 
                move_piece_sideways(-1);
                lastMoveTime = current_time;
            } else if (joy_x_val > JOYSTICK_THRESHOLD_HIGH) { 
                move_piece_sideways(1);
                lastMoveTime = current_time;
            }
        }

        static int last_joy_sw_state_tetris = HIGH;
        if (joy_sw_state == LOW && last_joy_sw_state_tetris == HIGH && current_time - lastRotateTime > rotateDebounceInterval) {
            rotate_current_shape();
            lastRotateTime = current_time;
        }
        last_joy_sw_state_tetris = joy_sw_state;

        if (joy_y_val > JOYSTICK_THRESHOLD_HIGH) { 
            if (current_time - lastFallTime > 50) { 
                 update_frame(); 
                 lastFallTime = current_time; 
            }
        }
    }
} // End namespace Tetris

void runTetris() {
    if (Tetris::tetrisGameOver) {
        Tetris::render_display(); 
        return;
    }

    Tetris::handle_tetris_input(); 

    unsigned long currentTime = millis();
    if (currentTime - Tetris::lastFallTime > Tetris::fallInterval) {
        Tetris::update_frame(); 
        Tetris::lastFallTime = currentTime;
    }

    Tetris::render_display(); 
}

// WebSocket event handler
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      // Remove player when they disconnect
      for (size_t i = 0; i < players.size(); i++) {
        if (players[i].id == String(client->id())) {
          players.erase(players.begin() + i);
          break;
        }
      }
      broadcastGameState(); // Update all clients about the disconnection
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(client, arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    // Null-terminate the data for string handling
    data[len] = 0;
    String message = String((char*)data);
    
    // Parse JSON message
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    
    // Process message based on type
    String messageType = doc["type"];
    
    if (messageType == "register") {
      // Register new player
      Player newPlayer;
      newPlayer.id = String(client->id());
      newPlayer.name = doc["name"].as<String>();
      newPlayer.x = random(10, SCREEN_WIDTH - 10);
      newPlayer.y = random(10, SCREEN_HEIGHT - 10);
      newPlayer.active = true;
      newPlayer.spottedTime = 0;
      newPlayer.isSpotted = false;
      
      players.push_back(newPlayer);
      
      // Send acknowledgment
      String response;
      DynamicJsonDocument responseDoc(1024);
      responseDoc["type"] = "registered";
      responseDoc["id"] = newPlayer.id;
      responseDoc["name"] = newPlayer.name;
      serializeJson(responseDoc, response);
      client->text(response);
      
      // Broadcast updated player list
      broadcastGameState();
      
    } else if (messageType == "movePlayer") {
      // Handle player movement
      String playerId = String(client->id());
      float newX = doc["x"];
      float newY = doc["y"];
      
      // Update player position
      for (auto& player : players) {
        if (player.id == playerId) {
          player.x = newX;
          player.y = newY;
          break;
        }
      }
      
      // No need to broadcast on every player move as we'll do periodic updates
      
    } else if (messageType == "startGame") {
      // Start game if there are at least 2 players
      if (players.size() >= 2 && !gameStarted) {
        gameStarted = true;
        gameStartTime = millis();
        Serial.println("Game started!");
        broadcastGameState();
      }
    }
  }
}

// Send game state to all connected clients
void broadcastGameState() {
  String gameState;
  DynamicJsonDocument doc(2048);
  
  doc["type"] = "gameState";
  doc["gameStarted"] = gameStarted;
  doc["adminX"] = adminX;
  doc["adminY"] = adminY;
  doc["spotlightRadius"] = spotlightRadius;
  
  if (gameStarted) {
    unsigned long elapsedTime = millis() - gameStartTime;
    if (elapsedTime > gameDuration) {
      doc["timeRemaining"] = 0;
      // Game over - players win if any are still active
      bool playersWon = false;
      for (const auto& player : players) {
        if (player.active) {
          playersWon = true;
          break;
        }
      }
      doc["gameOver"] = true;
      doc["adminWon"] = !playersWon;
    } else {
      doc["timeRemaining"] = (gameDuration - elapsedTime) / 1000;
      doc["gameOver"] = false;
    }
  } else {
    doc["timeRemaining"] = gameDuration / 1000;
    doc["gameOver"] = false;
  }
  
  // Add player data
  JsonArray playersArray = doc.createNestedArray("players");
  for (const auto& player : players) {
    JsonObject playerObj = playersArray.createNestedObject();
    playerObj["id"] = player.id;
    playerObj["name"] = player.name;
    playerObj["x"] = player.x;
    playerObj["y"] = player.y;
    playerObj["active"] = player.active;
    playerObj["isSpotted"] = player.isSpotted;
  }
  
  serializeJson(doc, gameState);
  ws.textAll(gameState);
}

//for god sake
//help me