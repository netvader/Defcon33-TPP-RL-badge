// Hardware.ino - Hardware initialization, startup animation, about, credits

const float commonFrequencies[] = {
  300.00, 303.87, 304.25, 310.00, 315.00, 318.00,
  330.00, 336.00, 340.00, 350.00,
  387.00, 390.00, 418.00, 433.05, 433.42, 433.92,
  434.00, 434.39, 434.42, 434.78, 438.90,
  868.00, 868.30, 868.95, 869.85,
  902.00, 903.00, 904.00, 905.00, 906.00, 907.00,
  908.00, 909.00, 910.00, 911.00, 912.00, 913.00,
  914.00, 915.00, 916.00, 917.00, 918.00, 919.00,
  920.00, 921.00, 922.00, 923.00, 924.00, 925.00,
  926.00, 927.00
};
const int numFrequencies = sizeof(commonFrequencies) / sizeof(commonFrequencies[0]);

// Forward declarations
void initializeButtons();
void initializeSDCard(bool displayOK);
void initializeCC1101(bool displayOK);
void showHardwareSummary(bool displayOK);
void loadSettings();
void testCC1101SPI();

void initializeHardware() {
  // Initialize NeoPixels first for visual feedback
  pixels.begin();
  pixels.setBrightness(50);
  pixels.clear();
  pixels.show();
  
  Serial.println(F("\n[TPP Badge] Booting..."));
  Serial.println(F("[+] Starting hardware initialization"));
  
  // Initialize I2C for display
  Serial.println(F("[+] Initializing I2C..."));
  Wire.begin(8, 6); // SDA, SCL
  delay(100);
  
  // Initialize display
  Serial.println(F("[+] Initializing SH1106 OLED..."));
  bool displayOK = false;
  if(!display.begin(SCREEN_ADDRESS, true)) {
    Serial.println(F("[!] SH1106 at 0x3C failed, trying 0x3D"));
    if(!display.begin(0x3D, true)) {
      Serial.println(F("[!] Display initialization failed"));
    } else {
      displayOK = true;
    }
  } else {
    displayOK = true;
  }
  
  if(displayOK) {
    Serial.println(F("[+] Display initialized successfully"));
    showStartupAnimation();
  }
  
  // Now show boot progress
  debugPrint("Initializing...", true, displayOK, 500);
  
  // Initialize buttons
  initializeButtons();
  
  // Initialize EEPROM
  debugPrint("[+] Init EEPROM...", true, displayOK, 500);
  EEPROM.begin(EEPROM_SIZE);
  loadSettings();
  
  // Initialize SD Card
  initializeSDCard(displayOK);
  initializeFileSystem(); // creates /rx_data, /tx_data, /logs, /config, /subghz - was never called before, so saving RX data always failed
  
  // Test CC1101 SPI before full initialization
  testCC1101SPI();
  
  // Initialize CC1101 modules
  initializeCC1101(displayOK);
  
  // Show hardware summary
  showHardwareSummary(displayOK);
}

void initializeButtons() {
  debugPrint("[+] Init Buttons...", true, true, 500);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
}

void initializeSDCard(bool displayOK) {
  debugPrint("[+] Init SD Card...", true, displayOK, 500);

  if(!SD_MMC.setPins(SD_CLK, SD_CMD, SD_DAT0, SD_DAT1, SD_DAT2, SD_DAT3)) {
    Serial.println(F("[!] Pin change failed!"));
    return;
  }

  if(!SD_MMC.begin("/sdcard")) { // 4-bit mode
    Serial.println(F("[!] SD Card Mount Failed"));
    if(!SD_MMC.begin()) {
      Serial.println(F("[!] SD Card initialization failed"));
      debugPrint("No SD Card", true, displayOK, 1000);
      sdCardPresent = false;
    } else {
      Serial.println(F("[+] SD Card initialized (4-bit mode)"));
      debugPrint("SD Card OK (4-Bit)!", true, displayOK, 500);
      sdCardPresent = true;
    }
  } else {
    Serial.println(F("[+] SD Card initialized (4-bit mode)"));
    debugPrint("SD Card OK!", true, displayOK, 500);
    sdCardPresent = true;
  }

  if(sdCardPresent) {
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE) {
      Serial.println(F("[!] No SD card attached"));
      sdCardPresent = false;
    } else {
      Serial.print(F("SD Card Type: "));
      if(cardType == CARD_MMC) Serial.println(F("MMC"));
      else if(cardType == CARD_SD) Serial.println(F("SDSC"));
      else if(cardType == CARD_SDHC) Serial.println(F("SDHC"));
      else Serial.println(F("UNKNOWN"));
      
      uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
      Serial.printf("SD Card Size: %lluMB\n", cardSize);
    }
  }
}

void testCC1101SPI() {
  Serial.println(F("[CC1101] Testing SPI communication..."));
  debugPrint("[+] Test CC1101 SPI...", true, true, 500);
  
  // Configure SPI pins for both modules
  ELECHOUSE_cc1101.addSpiPin(CC1101_CLK_A, CC1101_MISO_A, CC1101_MOSI_A, CC1101_CS_A, 0);
  ELECHOUSE_cc1101.addSpiPin(CC1101_CLK_B, CC1101_MISO_B, CC1101_MOSI_B, CC1101_CS_B, 1);
  
  // Test module A
  Serial.println(F("[CC1101] Testing Module A..."));
  ELECHOUSE_cc1101.setModul(0);
  
  // Try to read the VERSION register (address 0x31 in status registers)
  // CC1101 should return 0x14 (version) or 0x04 (partnum)
  pinMode(CC1101_CS_A, OUTPUT);
  digitalWrite(CC1101_CS_A, HIGH);
  delay(10);
  
  SPI.begin(CC1101_CLK_A, CC1101_MISO_A, CC1101_MOSI_A, CC1101_CS_A);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0)); // 1MHz for testing
  
  // Reset sequence
  digitalWrite(CC1101_CS_A, LOW);
  delay(1);
  digitalWrite(CC1101_CS_A, HIGH);
  delay(1);
  digitalWrite(CC1101_CS_A, LOW);
  
  // Wait for MISO to go low (chip ready)
  unsigned long timeout = millis() + 100;
  while(digitalRead(CC1101_MISO_A) && millis() < timeout);
  
  if(millis() >= timeout) {
    Serial.println(F("[CC1101] Module A timeout waiting for MISO"));
    cc1101APresent = false;
  } else {
    // Send SRES command (reset)
    SPI.transfer(0x30);
    delay(1);
    
    // Read VERSION register (0x31 | 0xC0 for status register burst read)
    SPI.transfer(0x31 | 0xC0);
    byte version = SPI.transfer(0x00);
    
    digitalWrite(CC1101_CS_A, HIGH);
    SPI.endTransaction();
    SPI.end();
    
    Serial.printf("[CC1101] Module A VERSION register: 0x%02X\n", version);
    
    if(version == 0x14 || version == 0x04 || (version > 0 && version < 0xFF)) {
      Serial.println(F("[CC1101] Module A detected!"));
      cc1101APresent = true;
    } else {
      Serial.println(F("[CC1101] Module A not detected"));
      cc1101APresent = false;
    }
  }
  
  // Test module B
  Serial.println(F("[CC1101] Testing Module B..."));
  ELECHOUSE_cc1101.setModul(1);
  
  pinMode(CC1101_CS_B, OUTPUT);
  digitalWrite(CC1101_CS_B, HIGH);
  delay(10);
  
  SPI.begin(CC1101_CLK_B, CC1101_MISO_B, CC1101_MOSI_B, CC1101_CS_B);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  
  // Reset sequence
  digitalWrite(CC1101_CS_B, LOW);
  delay(1);
  digitalWrite(CC1101_CS_B, HIGH);
  delay(1);
  digitalWrite(CC1101_CS_B, LOW);
  
  // Wait for MISO to go low
  timeout = millis() + 100;
  while(digitalRead(CC1101_MISO_B) && millis() < timeout);
  
  if(millis() >= timeout) {
    Serial.println(F("[CC1101] Module B timeout waiting for MISO"));
    cc1101BPresent = false;
  } else {
    // Send SRES command
    SPI.transfer(0x30);
    delay(1);
    
    // Read VERSION register
    SPI.transfer(0x31 | 0xC0);
    byte version = SPI.transfer(0x00);
    
    digitalWrite(CC1101_CS_B, HIGH);
    SPI.endTransaction();
    SPI.end();
    
    Serial.printf("[CC1101] Module B VERSION register: 0x%02X\n", version);
    
    if(version == 0x14 || version == 0x04 || (version > 0 && version < 0xFF)) {
      Serial.println(F("[CC1101] Module B detected!"));
      cc1101BPresent = true;
    } else {
      Serial.println(F("[CC1101] Module B not detected"));
      cc1101BPresent = false;
    }
  }
  
  Serial.printf("[CC1101] Test complete. A=%d, B=%d\n", cc1101APresent, cc1101BPresent);
}

void initializeCC1101(bool displayOK) {
  debugPrint("[+] Init CC1101s...", true, displayOK, 500);
  
  Serial.println(F("[CC1101] Starting initialization"));
  
  // Configure GDO0 pins for both modules
  ELECHOUSE_cc1101.addGDO0(CC1101_GDO0_A, 0);
  ELECHOUSE_cc1101.addGDO0(CC1101_GDO0_B, 1);
  
  // Only mark modules as ready if they passed the SPI test
  // We DON'T call Init() here - it will be called when actually using the module
  if(cc1101APresent) {
    debugPrint("CC1101-A Ready", true, displayOK, 500);
    Serial.println(F("[CC1101] Module A ready (will init on use)"));
  }
  
  if(cc1101BPresent) {
    debugPrint("CC1101-B Ready", true, displayOK, 500);
    Serial.println(F("[CC1101] Module B ready (will init on use)"));
  }
  
  // Set default module
  if(cc1101APresent) {
    activeModule = 0;
    ELECHOUSE_cc1101.setModul(0);
    Serial.println(F("[CC1101] Using Module A as default"));
  } else if(cc1101BPresent) {
    activeModule = 1;
    ELECHOUSE_cc1101.setModul(1);
    Serial.println(F("[CC1101] Using Module B as default"));
  } else {
    Serial.println(F("[CC1101] WARNING: No CC1101 modules detected!"));
    debugPrint("No CC1101 Found", true, displayOK, 1000);
  }
  
  Serial.printf("[CC1101] Initialization complete. A=%d, B=%d\n", cc1101APresent, cc1101BPresent);
}

void showHardwareSummary(bool displayOK) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ HARDWARE ]="));
  display.println(F(""));
  display.print(F("Display: "));
  display.println(displayOK ? F("OK") : F("FAIL"));
  display.print(F("SD Card: "));
  display.println(sdCardPresent ? F("OK") : F("NO"));
  display.print(F("CC1101-A: "));
  display.println(cc1101APresent ? F("OK") : F("NO"));
  display.print(F("CC1101-B: "));
  display.println(cc1101BPresent ? F("OK") : F("NO"));
  display.println(F(""));
  display.println(F("Ready!"));
  display.display();
  
  Serial.println(F("[+] TPP Badge Ready!"));
  
  // Quick flash to indicate ready
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 255, 0));
  }
  pixels.show();
  
  delay(2000);
  
  pixels.clear();
  pixels.show();
  
  display.clearDisplay();
  display.display();
  
  currentMenu = MENU_MAIN;
  previousMenu = MENU_MAIN;
  menuSelection = 0;
  menuOffset = 0;
  
  lastButtonPress = millis();
}

void showStartupAnimation() {
  // ========== PHASE 1: ANIMATION ==========
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(25, 10);
  display.println(F("TPP"));
  display.setTextSize(1);
  display.setCursor(20, 35);
  display.println(F("DEFCON 33"));
  display.setCursor(15, 50);
  display.println(F("Pirates Plunder"));
  display.display();
  
  // Red flashing effect on LEDs
  for(int i = 0; i < 3; i++) {
    for(int j = 0; j < NEOPIXEL_COUNT; j++) {
      pixels.setPixelColor(j, pixels.Color(255, 0, 0));
    }
    pixels.show();
    delay(200);
    pixels.clear();
    pixels.show();
    delay(800);
  }
   
  // ========== PHASE 2: SKULL AND CROSSBONES ON WAVES ==========
  for(int frame = 0; frame < 30; frame++) {
    display.clearDisplay();
    
    // Draw skull (matching original exactly)
    int skullY = 20 + sin(frame * 0.2) * 3;
    
    // Draw crossbones first (behind skull)
    display.drawLine(25, skullY + 10, 103, skullY + 10, SH110X_WHITE);
    display.drawLine(25, skullY + 11, 103, skullY + 11, SH110X_WHITE);
    display.drawLine(25, skullY + 15, 103, skullY + 5, SH110X_WHITE);
    display.drawLine(25, skullY + 16, 103, skullY + 6, SH110X_WHITE);
    
    // Bone ends
    display.fillCircle(25, skullY + 10, 3, SH110X_WHITE);
    display.fillCircle(103, skullY + 10, 3, SH110X_WHITE);
    display.fillCircle(25, skullY + 15, 3, SH110X_WHITE);
    display.fillCircle(103, skullY + 5, 3, SH110X_WHITE);
    
    // Skull head
    display.fillCircle(64, skullY, 20, SH110X_WHITE);
    
    // Eye sockets (larger, matching original)
    display.fillCircle(56, skullY - 5, 5, SH110X_BLACK);
    display.fillCircle(72, skullY - 5, 5, SH110X_BLACK);
    
    // Evil eye dots
    display.fillCircle(56, skullY - 5, 2, SH110X_WHITE);
    display.fillCircle(72, skullY - 5, 2, SH110X_WHITE);
    
    // Nose (triangular)
    display.fillTriangle(64, skullY + 2, 61, skullY + 6, 67, skullY + 6, SH110X_BLACK);
    
    // Evil grin
    display.drawLine(56, skullY + 10, 72, skullY + 10, SH110X_BLACK);
    display.drawLine(55, skullY + 9, 57, skullY + 10, SH110X_BLACK);
    display.drawLine(71, skullY + 10, 73, skullY + 9, SH110X_BLACK);
    
    // Teeth
    for(int i = 0; i < 5; i++) {
      display.drawLine(58 + i*3, skullY + 10, 58 + i*3, skullY + 13, SH110X_BLACK);
    }
    
    // Draw waves at bottom
    for(int x = 0; x < 128; x += 10) {
      int waveY = 48 + sin((x + frame * 5) * 0.1) * 3;
      display.drawLine(x, waveY, x + 10, waveY, SH110X_WHITE);
    }
    
    // Text
    display.setTextSize(1);
    display.setCursor(40, 55);
    display.print(F("TPP BADGE"));
    
    display.display();
    
    // Animate LEDs - pirate ship sailing
    pixels.clear();
    int shipPos = (frame * 2) % NEOPIXEL_COUNT;
    
    // Ship hull (red)
    pixels.setPixelColor(shipPos, pixels.Color(255, 0, 0));
    if(shipPos > 0) pixels.setPixelColor(shipPos - 1, pixels.Color(128, 0, 0));
    if(shipPos < NEOPIXEL_COUNT - 1) pixels.setPixelColor(shipPos + 1, pixels.Color(128, 0, 0));
    
    // Mast (white)
    int mastPos = (shipPos + 2) % NEOPIXEL_COUNT;
    pixels.setPixelColor(mastPos, pixels.Color(255, 255, 255));
    
    // Wake (blue)
    for(int j = 3; j < 8; j++) {
      int wakePos = (shipPos - j + NEOPIXEL_COUNT) % NEOPIXEL_COUNT;
      int brightness = 255 - (j * 30);
      if(brightness > 0) {
        pixels.setPixelColor(wakePos, pixels.Color(0, 0, brightness));
      }
    }
    
    pixels.show();
    delay(50);
  }
  
  // ========== PHASE 3: MATRIX RAIN TRANSITION (not working?) ==========
  uint8_t drops[16]; // 16 columns for matrix effect
  uint8_t dropSpeed[16];
  for(int i = 0; i < 16; i++) {
    drops[i] = random(64);
    dropSpeed[i] = random(1, 4);
  }
  
  int skullY = 20; // Final skull position
  
  for(int frame = 0; frame < 60; frame++) {
    display.clearDisplay();
    
    // Matrix rain effect
    for(int i = 0; i < 16; i++) {
      for(int j = 0; j < 8; j++) {
        int y = drops[i] - j * 8;
        if(y >= 0 && y < 64) {
          display.setCursor(i * 8, y);
          display.print(random(2));
        }
      }
      
      if(frame % dropSpeed[i] == 0) {
        drops[i]++;
        if(drops[i] > 70) drops[i] = 0;
      }
    }
    
    // Skull starts to fall after frame 20
    if(frame > 20) {
      skullY += 2;
      if(skullY > 48) skullY = 48;
      
      // Draw falling skull (smaller)
      display.fillCircle(64, skullY, 10, SH110X_WHITE);
      display.fillCircle(60, skullY - 2, 3, SH110X_BLACK);
      display.fillCircle(68, skullY - 2, 3, SH110X_BLACK);
      display.drawPixel(60, skullY - 2, SH110X_WHITE);
      display.drawPixel(68, skullY - 2, SH110X_WHITE);
    }
    
    display.display();
    
    // Matrix LEDs
    pixels.clear();
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      if(random(10) < 3) {
        pixels.setPixelColor(i, pixels.Color(0, random(50, 255), 0));
      }
    }
    pixels.show();
    delay(50);
  }
  
  // ========== PHASE 4: SKULL DROPS INTO WATER ==========
  for(int frame = 0; frame < 20; frame++) {
    display.clearDisplay();
    
    // Water splash effect
    if(frame < 10) {
      int splashRadius = frame * 3;
      display.drawCircle(64, 48, splashRadius, SH110X_WHITE);
      
      // Splash droplets
      for(int i = 0; i < 8; i++) {
        float angle = (i * 45) * PI / 180;
        int x = 64 + cos(angle) * (splashRadius + 5);
        int y = 48 + sin(angle) * (splashRadius + 5) - frame;
        display.fillCircle(x, y, 2, SH110X_WHITE);
      }
    }
    
    // Ripples
    for(int r = 0; r < 3; r++) {
      int rippleRadius = (frame * 2) + (r * 10);
      if(rippleRadius < 64) {
        display.drawCircle(64, 48, rippleRadius, SH110X_WHITE);
      }
    }
    
    display.display();
    
    // Water splash LEDs
    pixels.clear();
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      int brightness = random(0, 255 - (frame * 10));
      if(brightness > 0) {
        pixels.setPixelColor(i, pixels.Color(0, 0, brightness));
      }
    }
    pixels.show();
    delay(50);
  }
  
  // ========== FINAL: RETURN TO ORIGINAL LOGO ==========
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(25, 10);
  display.println(F("TPP"));
  display.setTextSize(1);
  display.setCursor(20, 35);
  display.println(F("DEFCON 33"));
  display.setCursor(15, 50);
  display.println(F("Pirates Plunder"));
  display.display();
  
  // Final LED effect - pirate red wave
  for(int wave = 0; wave < 2; wave++) {
    for(int i = 0; i < NEOPIXEL_COUNT + 10; i++) {
      pixels.clear();
      for(int j = 0; j < 10; j++) {
        int pos = i - j;
        if(pos >= 0 && pos < NEOPIXEL_COUNT) {
          int brightness = 255 - (j * 25);
          pixels.setPixelColor(pos, pixels.Color(brightness, 0, 0));
        }
      }
      pixels.show();
      delay(30);
    }
  }
  
  pixels.clear();
  pixels.show();
  
  delay(1000);
}

// Credits handling
unsigned long lastCreditScroll = 0;
int creditScrollPos = 0;
const char* creditLines[] = {
  "=[ CREDITS ]=",
  "",
  "Hardware Design:",
  "  TehRabbitt",
  "  rabbit-labs.com",
  "",
  "Firmware Dev:",
  "  RocketGod",
  "  betaskynet.com",
  "",
  "Discord:",
  "  discord.gg/",
  "  thepirates",
  "",
  "Special Thanks:",
  "  TPP Crew",
  "  Argh mateys!",
  "",
  "Fork v2 by:",
  "  netvader",
  "  w/ Claude Code",
  "  (vibecoded -",
  "   use at own risk,",
  "   no warranty)",
  "",
  "DEF CON 33",
  "2025",
  "",
  "Now fuck off!",
  "",
  ""
};
const int numCreditLines = sizeof(creditLines) / sizeof(creditLines[0]);

void drawAbout() {
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ ABOUT ]="));
  display.println(F(""));
  display.println(F("The Pirates'"));
  display.println(F("Plunder Badge"));
  display.println(F("DEF CON 33"));
  display.println(F(""));
  display.println(F("Fork v2 (netvader)"));
  display.println(F("See Credits"));
}

void drawCredits() {
  if (millis() - lastCreditScroll > 500) {
    lastCreditScroll = millis();
    creditScrollPos++;
    if (creditScrollPos >= numCreditLines - 6) {
      creditScrollPos = 0;
    }
  }
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  for (int i = 0; i < 8 && (creditScrollPos + i) < numCreditLines; i++) {
    display.setCursor(0, i * 8);
    display.println(creditLines[creditScrollPos + i]);
  }
}

const char* getModulationName(int modulation) {
  switch(modulation) {
    case 0: return "2-FSK";
    case 1: return "GFSK";
    case 2: return "ASK/OOK";
    case 3: return "4-FSK";
    case 4: return "MSK";
    default: return "Unknown";
  }
}