#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

SoftwareSerial BT(8, 7); // RX = D8 (from TX of Bluetooth), TX = D7 (to RX of Bluetooth)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define SCREEN_I2C_ADDRESS 0x3c
#define OLED_RESET_PIN -1

Adafruit_SSD1306 screen(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);


// Input pins
const int muxPins[] = {3, 4, 5, 6};
const int CAPS_PIN = 13;
bool Caps= false;
bool prevCapsKeyState = false; // To track "caps" key previous state
bool debuggingMode = true; // To define if the display shows debugging information
bool startKB = false; // To start the KB 


// Define row and column pins

const int numRows = 5;
const int numCols = 12;
// Maping between the rows / cols number and the corresponding pin on the multiplexer
const int rowMuxPins[numRows] = {0, 1, 2, 3, 4};
const int colMuxPins[numCols-1] = {5, 6, 7, 8, 13, 14, 15, 12, 11, 10, 9};
// The first collumn is not on the multiplex and connected directly to the Arduino via pin 9
const int COL_0_PIN = 9;
// The pin of the mux signal
const int MUX_SIG_PIN = 10;

bool rowPressed[numRows] = {false, false, false, false, false};
bool colPressed[numCols] = {false, false, false, false, false, false, false, false, false, false, false, false};

// Key mapping based on (row, col)
const char* keyMap[5][12] = {
  {"esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "backspace"},
  {"tab", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\\"},
  {"caps", "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "enter"},
  {"shift", "z", "x", "c", "v", "b", "n", "m", ",", ".", "Up", "enter"},
  {"ctrl", "fn", "win", "alt", "space", "space", "space", "'", "/", "Left", "Down", "Right"}
};
void setup() {
  BT.begin(9600); // Bluetooth module

  // The pins connected to the multiplexer (sellection pins)
  for (int i = 0; i < 4; i++) {
    pinMode(muxPins[i], OUTPUT);
  }

  pinMode(MUX_SIG_PIN, INPUT);
  // // Set row and column pins as input
  // for (int i = 0; i < 3; i++) {
  //   pinMode(rowPins[i], INPUT);
  //   pinMode(colPins[i], INPUT);
  // }
  pinMode(CAPS_PIN, OUTPUT);
  digitalWrite(CAPS_PIN, LOW);

  // Initialize OLED
  if (!screen.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }

  screen.clearDisplay();
  screen.setTextSize(2);
  screen.setTextColor(SSD1306_WHITE);
  screen.setCursor(0, 10);
  screen.print("Ready");
  screen.display();
  delay(1000);
  screen.setTextSize(2);
}


// Set 4-bit multiplexer address (0–15)
void selectMuxChannel(uint8_t channel) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(muxPins[i], (channel >> i) & 0x01);
  }
}

void loop() {
  // Keys for commands
  bool FnPressed = false;
  bool altPressed = false;
  bool dPressed = false;
  bool nPressed = false;

  screen.clearDisplay();
  screen.setCursor(0, 0);
  // Read the rows value
  for(int row_iter = 0; row_iter< numRows; row_iter++){
    int channel = rowMuxPins[row_iter];
    selectMuxChannel(channel);
    // delay(1);
    int rowState = digitalRead(MUX_SIG_PIN);
    rowPressed[row_iter] = (rowState == HIGH);
  }

  // === Read COLUMN 0 (direct connection) ===
  colPressed[0] = (digitalRead(COL_0_PIN) == HIGH);

  // Read the cols value
  for(int col_iter = 0; col_iter< numCols; col_iter++){
    int channel = colMuxPins[col_iter];
    selectMuxChannel(channel);
    // delay(1);
    int colState = digitalRead(MUX_SIG_PIN);
    colPressed[col_iter + 1] = (colState == HIGH);  // +1 because col 0 is already read
  }

  // === Format string ===
  // In the case of debugging show the rows and cols detected
  if(debuggingMode)
    screen.print("R:");
  String msg = "R: ";
  for (int i = 0; i < numRows; i++) {
    if (rowPressed[i]) {
      msg += String(i) + " ";
      if(debuggingMode){
        screen.print(i);
        screen.print(",");
      }
    }
  }

  if(debuggingMode)
    screen.print("C:");
  msg += ", C: ";
  for (int i = 0; i < numCols; i++) {
    if (colPressed[i]) {
      msg += String(i) + " ";
      if(debuggingMode){
        screen.print(i);
        screen.print(",");
      }
    }
  }
  
  // In the case of debugging print the key pressed on the second line
  if(debuggingMode)
    screen.setCursor(0, 16);

  // === Send message via Bluetooth if the startKB is true ===
  if(startKB){
    BT.println(msg);
  }

  bool keyShown = false;
  bool currentCapsKeyState = false;
  for (int row = 0; row < numRows; row++) {
    for (int col = 0; col < numCols; col++) {
      if (rowPressed[row] && colPressed[col]) {
        const char* key = keyMap[numRows - row - 1][numCols - col - 1];
        if (strcmp(key, "fn") == 0) FnPressed = true;
        if (strcmp(key, "alt") == 0) altPressed = true;
        if (strcmp(key, "d") == 0) dPressed = true;
        if (strcmp(key, "n") == 0) nPressed = true;
        screen.println(key);
        keyShown = true;

        if (strcmp(key, "caps") == 0) {
          currentCapsKeyState = true;  // caps key is pressed now
        }
      }
    }
  }


  screen.display();
  
  // === Edge detection for caps key ===
  if (currentCapsKeyState && !prevCapsKeyState) {
    Caps = !Caps;
    digitalWrite(CAPS_PIN, Caps ? HIGH : LOW);
  }

  prevCapsKeyState = currentCapsKeyState;
  
  // === Checking for commands ===

  // Tuggeling debugging mode
  // Ctrl+Alt+D 
  if (FnPressed && altPressed && dPressed) {
    debuggingMode = !debuggingMode;
    screen.clearDisplay();
    screen.setCursor(0, 0);
    if(debuggingMode)
      screen.println("Debugging on");
    else
      screen.println("Debugging off");
    screen.display();
    delay(1000);
  }
    
  // Start/Stop sending via Bluetooth
  // Ctrl+Alt+S
  if (FnPressed && altPressed && nPressed) {
    startKB = !startKB;
    screen.clearDisplay();
    screen.setCursor(0, 0);
    if(startKB)
      screen.println("Start Sending");
    else
      screen.println("Stop sending");
    screen.display();
    delay(1000);
  }

}

