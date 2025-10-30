/*
 * ESP-NOW BATTLESHIP - MAIN SKETCH
 * (MODIFIED with new Lobby UI flow and Win/Loss/Reset logic)
 */

#include <TFT_eSPI.h>
#include <SPI.h>
#include <driver/ledc.h>
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// This is our custom game "library"
#include "battleshipgame.h"

// --- Button Pins (!!! YOU MUST CHANGE THESE !!!) ---
#define BTN_UP    32  // GPIO pin for UP
#define BTN_DOWN  33  // GPIO pin for DOWN
#define BTN_LEFT  25  // GPIO pin for LEFT
#define BTN_RIGHT 26  // GPIO pin for RIGHT
#define BTN_FIRE  27  // GPIO pin for FIRE (Bomb/Ready/Reset)

// --- ESP-NOW Configuration ---
// TODO: Set this to the MAC address of the OTHER ESP32
uint8_t peerAddress[] = {0x78, 0x1C, 0x3C, 0xB7, 0x53, 0xF0};
esp_now_peer_info_t peerInfo;

// Define message types
#define MSG_TYPE_READY  0
#define MSG_TYPE_SHOT   1
#define MSG_TYPE_RESULT 2

// Data structure for "Ready" message
typedef struct struct_ready_message {
  uint8_t msg_type = MSG_TYPE_READY;
} struct_ready_message;

// Data structure for sending a shot
typedef struct struct_shot_message {
  uint8_t msg_type = MSG_TYPE_SHOT;
  uint8_t x;
  uint8_t y;
} struct_shot_message;

// Data structure for sending a result
typedef struct struct_result_message {
  uint8_t msg_type = MSG_TYPE_RESULT;
  uint8_t x;
  uint8_t y;
  uint8_t result; 
  bool isGameOver = false; // <<< NEW: To signal a win/loss
} struct_result_message;

// Create one of each message type
struct_ready_message myReadyMsg;
struct_shot_message myShot;
struct_result_message myResult;

// --- Global Objects ---
TFT_eSPI tft = TFT_eSPI(); 
BattleshipGame game(&tft);

// --- Game State ---
bool myTurn = false;
bool amIPlayerOne = true; // <<< NEW: To remember who starts
bool iAmReady = false;
bool opponentIsReady = false;
GameState gameState = STANDBY;

// <<< NEW: Debounce for FIRE button in loop >>>
unsigned long lastButtonPress = 0; 
const long debounceDelay = 300;


// --- ESP-NOW Callbacks ---

void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  
  uint8_t msg_type = incomingData[0]; // First byte is always type

  if (msg_type == MSG_TYPE_READY) {
    // --- Opponent is ready ---
    Serial.println("Opponent is ready!");
    opponentIsReady = true;
    if (gameState == STANDBY) { // Check state to avoid messages during game
        // <<< MODIFIED: Use new lobby function
        game.updateLobbyText("Opponent is Ready!", TFT_GREEN);
    }
    
    // <<< NEW: Check if we can auto-start >>>
    if (iAmReady && opponentIsReady && gameState == STANDBY) {
      Serial.println("Both players ready, starting game!");
      game.startGame();
      gameState = PLAYING;
      if (myTurn) {
        game.updateStatus("Your turn!", TFT_GREEN);
      } else {
        game.updateStatus("Game Started. Wait...", TFT_YELLOW);
      }
    }

  } else if (msg_type == MSG_TYPE_SHOT) {
    // --- We are being fired upon ---
    memcpy(&myShot, incomingData, sizeof(myShot));
    Serial.printf("Shot received at %d, %d\n", myShot.x, myShot.y);
    
    // 1. Check our grid
    int shotResult = game.checkMyGrid(myShot.x, myShot.y);

    // 2. Prepare the result message
    myResult.x = myShot.x;
    myResult.y = myShot.y;
    myResult.result = shotResult;
    
    // 3. Check if this shot made us lose
    bool iHaveLost = game.didILose();
    myResult.isGameOver = iHaveLost;
    
    // 4. Send the result back
    esp_now_send(peerAddress, (uint8_t *) &myResult, sizeof(myResult));
    
    // 5. Update our own game state
    if (iHaveLost) {
      Serial.println("Game Over. I lost.");
      gameState = GAME_OVER;
      game.setState(GAME_OVER);
      game.showLoseScreen();
    } else {
      // <<< MODIFIED: Add delay to read status >>>
      if (shotResult == MISS) {
        game.updateStatus("They MISSED!", TFT_BLUE);
      } else if (shotResult == HIT) {
        game.updateStatus("They HIT!", TFT_RED);
      } else {
        game.updateStatus("They SANK a ship!", TFT_RED);
      }
      delay(1000); // Wait for player to read
      
      myTurn = true;
      game.updateStatus("Your turn!", TFT_GREEN);
    }

  } else if (msg_type == MSG_TYPE_RESULT) {
    // --- This is the result of our shot ---
    memcpy(&myResult, incomingData, sizeof(myResult));
    Serial.printf("Result received for %d, %d: %d\n", myResult.x, myResult.y, myResult.result);
    
    // 1. Handle the hit/miss/sunk logic
    game.handleShotResult(myResult.x, myResult.y, myResult.result);
    
    // 2. Check if this result means we won
    if (myResult.isGameOver) {
      Serial.println("Game Over. I won!");
      gameState = GAME_OVER;
      game.setState(GAME_OVER);
      game.showWinScreen();
    } else {
      // Game continues, it's no longer our turn
      myTurn = false;
    }
  }
}

// =======================================================
// SETUP
// =======================================================
void setup() {
  Serial.begin(115200);
  Serial.println("Booting up...");

  // --- Init TFT ---
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(50); 
  tft.init();
  tft.setRotation(0); 

  // --- Init Buttons ---
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_FIRE, INPUT_PULLUP);

  // --- Init WiFi & ESP-NOW ---
  WiFi.mode(WIFI_STA);
  delay(100); 
  Serial.print("My MAC Address: ");
  Serial.println(WiFi.macAddress()); 
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    while(1); 
  }

  esp_now_register_send_cb(OnDataSent);
  
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
  }

  esp_now_register_recv_cb(OnDataRecv);

  // --- Init Game ---
  
  // TODO: Decide who goes first.
  // Player 1: set myTurn = true
  // Player 2: set myTurn = false
  myTurn = true; 
  
  amIPlayerOne = myTurn; // Store our starting role
  
  // <<< MODIFIED: Renamed begin() to initialize() >>>
  // This NO LONGER draws the game board.
  game.initialize(amIPlayerOne); 
  
  gameState = game.getState(); // Will be STANDBY
  
  // <<< MODIFIED: Show the new lobby screen first >>>
  game.showLobbyScreen();
  // We'll use the lobby text function for the ready prompt
  game.updateLobbyText("press fire to ready", TFT_WHITE);
}

// =======================================================
// LOOP
// =======================================================
void loop() {
  
  switch (gameState) {
    
    case STANDBY:
      // --- We are in the lobby, waiting to start ---
      // <<< MODIFIED: This is the new lobby logic >>>
      if (digitalRead(BTN_FIRE) == LOW && (millis() - lastButtonPress > debounceDelay)) {
        lastButtonPress = millis();
        
        if (!iAmReady) {
          Serial.println("I am now Ready!");
          iAmReady = true;
          esp_now_send(peerAddress, (uint8_t *) &myReadyMsg, sizeof(myReadyMsg));
          game.updateLobbyText("Wait for Opponent", TFT_CYAN);
        }

        // Check if we can start
        if (iAmReady && opponentIsReady) {
          Serial.println("Starting game.");
          game.startGame(); 
          gameState = PLAYING; 
          
          if (myTurn) {
            game.updateStatus("Your turn!", TFT_GREEN);
          } else {
            game.updateStatus("Game Start. Wait", TFT_YELLOW);
          }
        }
      }
      break;
      
    case PLAYING:
      // --- The game is in progress ---
      if (myTurn) {
        bool firedShot = game.handleInput(
          digitalRead(BTN_UP) == LOW,
          digitalRead(BTN_DOWN) == LOW,
          digitalRead(BTN_LEFT) == LOW,
          digitalRead(BTN_RIGHT) == LOW,
          digitalRead(BTN_FIRE) == LOW
        );

        if (firedShot) {
          myShot.x = game.getShotX();
          myShot.y = game.getShotY();
          esp_now_send(peerAddress, (uint8_t *) &myShot, sizeof(myShot));
          
          myTurn = false; 
          game.updateStatus("Firing... Waiting.", TFT_WHITE);
        }
      }
      break;
      
    case GAME_OVER:
      // --- The game is over, wait for reset ---
      if (digitalRead(BTN_FIRE) == LOW && (millis() - lastButtonPress > debounceDelay)) {
        lastButtonPress = millis();
        Serial.println("Resetting game...");
        
        game.resetGame(); 
        
        // Reset lobby state
        iAmReady = false;
        opponentIsReady = false;
        myTurn = amIPlayerOne; // Reset myTurn
        
        gameState = STANDBY;
        
        // <<< MODIFIED: Show lobby text >>>
        game.updateLobbyText("press fire to ready", TFT_WHITE);
      }
      break;
  }
}
