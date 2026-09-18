// DC33 (DEF CON 33)
// The Pirates' Plunder / Rabbit Labs
// TPP Badge

// Badge designed by TehRabbitt 
// https://rabbit-labs.com/

// Firmware by RocketGod
// https://betaskynet.com
// https://discord.gg/thepirates

#include "Globals.h"

// Global object definitions
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Global variable definitions
MenuState currentMenu = MENU_MAIN;
MenuState previousMenu = MENU_MAIN;
int menuSelection = 0;
int menuOffset = 0;
const int maxMenuItems = 4;

IdlePattern idlePattern = PATTERN_BREATHE;
uint8_t ledBrightness = 50;
PixelMode pixelMode = PIXEL_IDLE;

// LED Pattern preview state
IdlePattern previewPattern = PATTERN_BREATHE;
IdlePattern savedPattern = PATTERN_BREATHE;
bool inPatternPreview = false;

String raw_rx = "0";
String jammer_tx = "0";
unsigned long sample[SAMPLE_SIZE];
int samplecount = 0;
float frequency = 433.92;
float teslaFrequency = 433.92;
int mod = 2;
float deviation = 47.60;
float setrxbw = 812.50;
int datarate = 99;
byte activeModule = 0;
bool sdCardPresent = false;
bool cc1101APresent = false;
bool cc1101BPresent = false;

unsigned long lastButtonPress = 0;
const unsigned long buttonDebounce = 200;

// Menu navigation
int frequencyIndex = 15; // Default to 433.92

// Tesla configuration  
int teslaTransmissions = 5;
int teslaDelay = 23;

// Jammer configuration
int jammerPower = 12;
bool jammerSweep = false;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Initialize hardware
  initializeHardware();
  
  // Start normal operation
  pixelMode = PIXEL_MENU;
  updateDisplay();
}

void loop() {
  handleButtons();
  updatePixels();
  
  if(raw_rx == "1") {
    if(currentMenu == MENU_WEATHER) {
      handleWeatherMode();
    } else {
      handleRXMode();
    }
  }
  
  if(jammer_tx == "1") {
    runJammer();
  }
}

void debugPrint(String message, bool serial, bool screen, int displayTime) {
  if (serial) {
    Serial.println(message);
  }
  if (screen && display.width() > 0) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("=[ DEBUG ]="));
    display.println(F(""));
    
    // Word wrap for long messages
    int lineLength = 20;
    for (int i = 0; i < message.length(); i += lineLength) {
      display.println(message.substring(i, min(i + lineLength, (int)message.length())));
    }
    
    display.display();
    delay(displayTime);
  }
}