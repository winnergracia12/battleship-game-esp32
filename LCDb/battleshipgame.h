/*
 * BATTLESHIP GAME - HEADER FILE (.h)
 * (MODIFIED with ship colors and new Lobby UI flow)
 */

#ifndef BATTLESHIP_GAME_H
#define BATTLESHIP_GAME_H

#include <TFT_eSPI.h>

// --- Game States ---
enum GameState {
  STANDBY,
  PLAYING,
  GAME_OVER 
};

// --- Game Settings ---
#define GRID_SIZE 8     
#define CELL_SIZE 12    
#define SHIP_COUNT 3    
extern const int shipLengths[SHIP_COUNT];
extern const char* shipNames[SHIP_COUNT];
extern const uint16_t shipColors[SHIP_COUNT]; // <<< NEW: Ship Colors

// --- UI Layout ---
#define GRID_Y 40       
#define LEFT_GRID_X 20  
#define RIGHT_GRID_X 136 
#define STATUS_Y 150    
#define LOBBY_STATUS_Y 180 // <<< NEW: Y-pos for lobby text

// --- Cell Status (for game logic) ---
#define EMPTY 0
#define HIT 10
#define MISS 11

// --- Random Seed Pin ---
#define RANDOM_SEED_PIN 34  

class BattleshipGame {
  
public:
  BattleshipGame(TFT_eSPI* tft_display);
  
  void initialize(bool isPlayerOneLayout); // <<< RENAMED from begin()
  void showLobbyScreen();                  // <<< NEW
  void updateLobbyText(String text, uint16_t color); // <<< NEW
  void startGame(); 
  
  bool handleInput(bool up, bool down, bool left, bool right, bool fire);
  void updateStatus(String text, uint16_t color);

  // --- Network Functions ---
  int checkMyGrid(int x, int y);
  void handleShotResult(int x, int y, int result);
  
  // --- Win/Loss/Reset Functions ---
  bool didILose();
  void showWinScreen();
  void showLoseScreen();
  void resetGame();

  // --- Getter/Setter Functions ---
  int getShotX();
  int getShotY();
  GameState getState();
  void setState(GameState newState);

private:
  // --- Private Variables ---
  TFT_eSPI* tft; 
  GameState currentState; 
  
  uint8_t playerGrid[GRID_SIZE][GRID_SIZE];
  uint8_t enemyGrid_View[GRID_SIZE][GRID_SIZE]; 
  
  int playerShipHealth[SHIP_COUNT];
  int enemyShipHealth[SHIP_COUNT]; 

  int cursorX;
  int cursorY;
  
  int lastShotX; 
  int lastShotY;

  unsigned long lastButtonPress;
  const long debounceDelay = 200;
  
  bool isPlayerOne;
  int myGrid_X_Pos;
  int enemyGrid_X_Pos;

  // --- Private Helper Functions ---
  void initializeGrids();
  void placePlayerShips(); 
  void initializeUnknownEnemyShips(); 
  void drawGameUI();
  void drawGrid(int x_offset, int y_offset, uint8_t grid[GRID_SIZE][GRID_SIZE], bool showShips);
  void drawShipStatus();
  void redrawCell(int x, int y);
  void drawCursorFrame(int x, int y, uint16_t color);
};

#endif // BATTLESHIP_GAME_H
