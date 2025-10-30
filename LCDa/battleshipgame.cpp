/*
 * BATTLESHIP GAME - IMPLEMENTATION FILE (.cpp)
 * (MODIFIED with ship colors, centered text, and new Lobby UI flow)
 */

#include "battleshipgame.h"
#include <Arduino.h> 

// Define the global variables
const int shipLengths[SHIP_COUNT] = {4, 3, 2};
const char* shipNames[SHIP_COUNT] = {"B-Ship", "Cruiser", "D-stroy"};
// <<< NEW: Ship colors
const uint16_t shipColors[SHIP_COUNT] = {TFT_LIGHTGREY, TFT_GREEN, TFT_BLUE}; // Gray, Green, Blue

// Constructor: Initializes the class
BattleshipGame::BattleshipGame(TFT_eSPI* tft_display) {
  tft = tft_display; 
  cursorX = 0;
  cursorY = 0;
  lastButtonPress = 0;
  isPlayerOne = true; 
  myGrid_X_Pos = LEFT_GRID_X; 
  enemyGrid_X_Pos = RIGHT_GRID_X; 
  currentState = STANDBY;
}

// <<< RENAMED & MODIFIED >>>
// initialize() just sets up variables, no drawing.
void BattleshipGame::initialize(bool isPlayerOneLayout) {
  isPlayerOne = isPlayerOneLayout;
  
  if (isPlayerOne) {
    myGrid_X_Pos = LEFT_GRID_X;
    enemyGrid_X_Pos = RIGHT_GRID_X;
  } else {
    myGrid_X_Pos = RIGHT_GRID_X;
    enemyGrid_X_Pos = LEFT_GRID_X;
  }
  
  pinMode(RANDOM_SEED_PIN, INPUT);
  randomSeed(analogRead(RANDOM_SEED_PIN));
  
  initializeGrids();
  // drawGameUI(); // <<< REMOVED: Don't draw UI yet
}

// <<< NEW FUNCTION >>>
// Draws the lobby screen
void BattleshipGame::showLobbyScreen() {
  tft->fillScreen(TFT_BLACK);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  
  tft->setTextSize(3);
  tft->drawString("ESP-NOW", tft->width() / 2 - tft->textWidth("ESP-NOW") / 2, 60);
  tft->drawString("BATTLESHIP", tft->width() / 2 - tft->textWidth("BATTLESHIP") / 2, 90);
  
  tft->setTextSize(2);
  tft->setTextColor(TFT_CYAN, TFT_BLACK);
  tft->drawString("Press FIRE to Ready", tft->width() / 2 - tft->textWidth("Press FIRE to Ready") / 2, 140);
}

// <<< NEW FUNCTION >>>
// Updates the status text on the lobby screen
void BattleshipGame::updateLobbyText(String text, uint16_t color) {
  // Clear only the lobby status area
  tft->fillRect(0, LOBBY_STATUS_Y, 240, 30, TFT_BLACK);
  tft->setTextSize(2);
  tft->setTextColor(color, TFT_BLACK);
  tft->drawString(text, tft->width() / 2 - tft->textWidth(text) / 2, LOBBY_STATUS_Y);
}


// <<< MODIFIED >>>
// This function now draws the entire game UI from scratch
void BattleshipGame::startGame() {
  // Guard against running twice
  if (currentState != STANDBY) return;
  
  tft->fillScreen(TFT_BLACK); // <<< NEW: Clear the lobby screen
  
  placePlayerShips(); 
  initializeUnknownEnemyShips(); 
  
  drawGameUI(); // <<< MOVED: Draw the empty grids and titles
  
  // Now, redraw our own grid to show our ships
  drawGrid(myGrid_X_Pos, GRID_Y, playerGrid, true); 
  
  drawShipStatus(); 
  drawCursorFrame(cursorX, cursorY, TFT_YELLOW); 
  
  currentState = PLAYING;
}


// Main input handler
bool BattleshipGame::handleInput(bool up, bool down, bool left, bool right, bool fire) {
  
  if (millis() - lastButtonPress < debounceDelay) {
    return false; 
  }

  int oldX = cursorX;
  int oldY = cursorY;
  bool moved = false;
  
  if (up) {
    if (cursorY > 0) { cursorY--; moved = true; }
  } else if (down) {
    if (cursorY < GRID_SIZE - 1) { cursorY++; moved = true; }
  } else if (left) {
    if (cursorX > 0) { cursorX--; moved = true; }
  } else if (right) {
    if (cursorX < GRID_SIZE - 1) { cursorX++; moved = true; }
  } else if (fire) {
    lastButtonPress = millis(); 
    
    if (enemyGrid_View[cursorY][cursorX] != EMPTY) {
      updateStatus("Already fired here!", TFT_YELLOW);
      return false; 
    }
    
    lastShotX = cursorX;
    lastShotY = cursorY;
    return true; 
  }

  if (moved) {
    drawCursorFrame(oldX, oldY, TFT_BLACK); 
    redrawCell(oldX, oldY);                 
    drawCursorFrame(cursorX, cursorY, TFT_YELLOW); 
    lastButtonPress = millis(); 
  }

  return false; 
}

// --- Getters / Setters ---
int BattleshipGame::getShotX() { return lastShotX; }
int BattleshipGame::getShotY() { return lastShotY; }
GameState BattleshipGame::getState() { return currentState; }
void BattleshipGame::setState(GameState newState) { currentState = newState; }


// --- Network Functions ---
int BattleshipGame::checkMyGrid(int x, int y) {
  if (playerGrid[y][x] > EMPTY && playerGrid[y][x] <= SHIP_COUNT) {
    int shipIndex = playerGrid[y][x] - 1;
    
    playerGrid[y][x] = HIT; 
    playerShipHealth[shipIndex]--; 
    
    drawShipStatus(); 
    drawGrid(myGrid_X_Pos, GRID_Y, playerGrid, true);

    if (playerShipHealth[shipIndex] == 0) {
    //  updateStatus("Opponent sank our ship!", TFT_RED); // <<< Kept your commented-out text
      return shipIndex + 1; 
    } else {
    //  updateStatus("Our ship was hit!", TFT_RED); // <<< Kept your commented-out text
      return HIT;
    }
    
  } else {
    playerGrid[y][x] = MISS;
    // updateStatus("Opponent missed!", TFT_BLUE); // <<< Kept your commented-out text
    drawGrid(myGrid_X_Pos, GRID_Y, playerGrid, true);
    return MISS;
  }
}

void BattleshipGame::handleShotResult(int x, int y, int result) {
  
  if (result == HIT) {
    enemyGrid_View[y][x] = HIT;
    updateStatus("It's a HIT!", TFT_RED);
    
  } else if (result == MISS) {
    enemyGrid_View[y][x] = MISS;
    updateStatus("It's a MISS!", TFT_BLUE);
    
  } else { 
    int shipIndex = result - 1;
    enemyGrid_View[y][x] = HIT; 
    
    if(enemyShipHealth[shipIndex] > 0) {
      enemyShipHealth[shipIndex] = 0; 
      char statusText[25];
      // <<< KEPT YOUR TEXT CHANGE >>>
      sprintf(statusText, "sank a %s!", shipNames[shipIndex]); 
      updateStatus(statusText, TFT_RED);
      drawShipStatus(); 
    }
  }

  drawGrid(enemyGrid_X_Pos, GRID_Y, enemyGrid_View, false);
  drawCursorFrame(cursorX, cursorY, TFT_YELLOW);
}

// --- Win/Loss/Reset Functions ---

bool BattleshipGame::didILose() {
  for (int i = 0; i < SHIP_COUNT; i++) {
    if (playerShipHealth[i] > 0) {
      return false; 
    }
  }
  return true; 
}

void BattleshipGame::showWinScreen() {
  tft->fillScreen(TFT_GREEN);
  tft->setTextColor(TFT_BLACK, TFT_GREEN);
  tft->setTextSize(4); 
  tft->drawString("YOU WIN!", tft->width() / 2 - tft->textWidth("YOU WIN!") / 2, 100);
  
  tft->setTextSize(2); 
  tft->drawString("Press FIRE to Reset", tft->width() / 2 - tft->textWidth("Press FIRE to Reset") / 2, 140);
}

void BattleshipGame::showLoseScreen() {
  tft->fillScreen(TFT_RED);
  tft->setTextColor(TFT_WHITE, TFT_RED);
  tft->setTextSize(4); 
  tft->drawString("YOU LOSE!", tft->width() / 2 - tft->textWidth("YOU LOSE!") / 2, 100);
  
  tft->setTextSize(2); 
  tft->drawString("Press FIRE to Reset", tft->width() / 2 - tft->textWidth("Press FIRE to Reset") / 2, 140);
}

// <<< MODIFIED >>>
// Resets the game and shows the lobby screen
void BattleshipGame::resetGame() {
  initializeGrids();
  currentState = STANDBY;
  cursorX = 0;
  cursorY = 0;
  // drawGameUI(); // <<< REMOVED
  showLobbyScreen(); // <<< NEW: Go back to lobby
}


// =======================================================
// PRIVATE HELPER FUNCTIONS
// =======================================================

void BattleshipGame::initializeGrids() {
  memset(playerGrid, EMPTY, sizeof(playerGrid));
  memset(enemyGrid_View, EMPTY, sizeof(enemyGrid_View));
  
  for (int i=0; i < SHIP_COUNT; i++) {
    playerShipHealth[i] = 0;
    enemyShipHealth[i] = 0;
  }
}

void BattleshipGame::placePlayerShips() {
  for (int i = 0; i < SHIP_COUNT; i++) {
    int len = shipLengths[i];
    int shipId = i + 1;

    while (true) { 
      int orientation = random(2);
      int x, y;
      
      if (orientation == 0) { 
        x = random(GRID_SIZE - len + 1);
        y = random(GRID_SIZE);
      } else { 
        x = random(GRID_SIZE);
        y = random(GRID_SIZE - len + 1);
      }

      bool canPlace = true;
      for (int k = 0; k < len; k++) {
        if (orientation == 0) {
          if (playerGrid[y][x + k] != EMPTY) { canPlace = false; break; }
        } else {
          if (playerGrid[y + k][x] != EMPTY) { canPlace = false; break; }
        }
      }

      if (canPlace) {
        playerShipHealth[i] = len; 
        for (int k = 0; k < len; k++) {
          if (orientation == 0) {
            playerGrid[y][x + k] = shipId;
          } else {
            playerGrid[y + k][x] = shipId;
          }
        }
        break; 
      }
    }
  }
}

void BattleshipGame::initializeUnknownEnemyShips() {
  for (int i = 0; i < SHIP_COUNT; i++) {
    enemyShipHealth[i] = shipLengths[i];
  }
}

// <<< MODIFIED >>>
// This function now assumes a black screen
void BattleshipGame::drawGameUI() {
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextSize(1);
  tft->drawString("YOUR FLEET", myGrid_X_Pos, 10, 2); 
  tft->drawString("ENEMY FLEET", enemyGrid_X_Pos, 10, 2); 
  
  // <<< MODIFIED: Draw empty player grid, not 'true' >>>
  drawGrid(myGrid_X_Pos, GRID_Y, playerGrid, false); 
  drawGrid(enemyGrid_X_Pos, GRID_Y, enemyGrid_View, false); 
}


void BattleshipGame::drawGrid(int x_offset, int y_offset, uint8_t grid[GRID_SIZE][GRID_SIZE], bool showShips) {
  
  tft->setTextSize(1);
  
  for (int j = 0; j < GRID_SIZE; j++) {
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawChar('A' + j, x_offset + j * CELL_SIZE + (CELL_SIZE / 2) - 4, y_offset - 15);
  }

  for (int i = 0; i < GRID_SIZE; i++) {
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawNumber(i + 1, x_offset - 12, y_offset + i * CELL_SIZE + (CELL_SIZE / 2) - 4);

    for (int j = 0; j < GRID_SIZE; j++) {
      int cell_x = x_offset + j * CELL_SIZE;
      int cell_y = y_offset + i * CELL_SIZE;
      uint8_t cellValue = grid[i][j];
      
      tft->fillRect(cell_x, cell_y, CELL_SIZE, CELL_SIZE, TFT_BLACK);

      switch (cellValue) {
        case HIT:
          tft->drawLine(cell_x, cell_y, cell_x + CELL_SIZE, cell_y + CELL_SIZE, TFT_RED);
          tft->drawLine(cell_x + CELL_SIZE, cell_y, cell_x, cell_y + CELL_SIZE, TFT_RED);
          break;
        case MISS:
          tft->fillCircle(cell_x + CELL_SIZE / 2, cell_y + CELL_SIZE / 2, CELL_SIZE / 3, TFT_BLUE);
          break;
        case EMPTY:
          break; 
        default: // It's a ship ID (1, 2, or 3)
          if (showShips) {
            // <<< MODIFIED: Use specific ship color
            tft->fillRect(cell_x, cell_y, CELL_SIZE, CELL_SIZE, shipColors[cellValue - 1]);
          }
          break;
      }
      tft->drawRect(cell_x, cell_y, CELL_SIZE, CELL_SIZE, TFT_DARKGREY);
    }
  }
}

void BattleshipGame::updateStatus(String text, uint16_t color) {
  tft->fillRect(0, STATUS_Y, 240, 30, TFT_BLACK);
  tft->setCursor(10, STATUS_Y + 5);
  tft->setTextColor(color, TFT_BLACK);
  tft->setTextSize(2);
  tft->println(text);
}

void BattleshipGame::drawShipStatus() {
  int y_start = STATUS_Y + 30;
  tft->fillRect(0, y_start, 240, 240 - y_start, TFT_BLACK);
  
  tft->setTextSize(1);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  
  if (isPlayerOne) {
    tft->drawString("Player", 10, y_start, 2); 
    tft->drawString("Enemy", 130, y_start, 2); 
  } else {
    tft->drawString("Enemy", 10, y_start, 2); 
    tft->drawString("Player", 130, y_start, 2); 
  }
  
  y_start += 15;
  
  for (int i = 0; i < SHIP_COUNT; i++) {
    char statusText[20];
    
    // Player Ship Status (use ship color)
    uint16_t p_color = (playerShipHealth[i] > 0) ? shipColors[i] : TFT_RED; // <<< MODIFIED
    tft->setTextColor(p_color, TFT_BLACK);
    sprintf(statusText, "%s: [%d]", shipNames[i], playerShipHealth[i]);
    tft->drawString(statusText, (isPlayerOne ? 10 : 130), y_start + (i * 15), 2);

    // Enemy Ship Status
    uint16_t e_color = (enemyShipHealth[i] > 0) ? TFT_GREEN : TFT_RED;
    tft->setTextColor(e_color, TFT_BLACK);
    sprintf(statusText, "%s: [%d]", shipNames[i], enemyShipHealth[i]);
    tft->drawString(statusText, (isPlayerOne ? 130 : 10), y_start + (i * 15), 2);
  }
}

void BattleshipGame::redrawCell(int x, int y) {
  int cell_x = enemyGrid_X_Pos + x * CELL_SIZE; 
  int cell_y = GRID_Y + y * CELL_SIZE;
  
  uint8_t cellValue = enemyGrid_View[y][x];
  
  tft->fillRect(cell_x, cell_y, CELL_SIZE, CELL_SIZE, TFT_BLACK); 

  switch (cellValue) {
    case HIT:
      tft->drawLine(cell_x, cell_y, cell_x + CELL_SIZE, cell_y + CELL_SIZE, TFT_RED);
      tft->drawLine(cell_x + CELL_SIZE, cell_y, cell_x, cell_y + CELL_SIZE, TFT_RED);
      break;
    case MISS:
      tft->fillCircle(cell_x + CELL_SIZE / 2, cell_y + CELL_SIZE / 2, CELL_SIZE / 3, TFT_BLUE);
      break;
    case EMPTY:
    default:
      break;
  }
  
  tft->drawRect(cell_x, cell_y, CELL_SIZE, CELL_SIZE, TFT_DARKGREY);
}

void BattleshipGame::drawCursorFrame(int x, int y, uint16_t color) {
  int cell_x = enemyGrid_X_Pos + x * CELL_SIZE; 
  int cell_y = GRID_Y + y * CELL_SIZE;
  tft->drawRect(cell_x - 1, cell_y - 1, CELL_SIZE + 2, CELL_SIZE + 2, color);
}
