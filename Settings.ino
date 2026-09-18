// Settings.ino - EEPROM settings management and persistence

// EEPROM memory map
#define EEPROM_MAGIC 0xDEF33     // Magic number to verify valid data
#define EEPROM_VERSION 1          // Settings version

// EEPROM addresses
#define ADDR_MAGIC 0              // 4 bytes
#define ADDR_VERSION 4            // 1 byte
#define ADDR_MODULE 5             // 1 byte
#define ADDR_MOD 6                // 1 byte
#define ADDR_PATTERN 7            // 1 byte
#define ADDR_BRIGHTNESS 8         // 1 byte
#define ADDR_FREQUENCY 10         // 4 bytes (float)
#define ADDR_DEVIATION 14         // 4 bytes (float)
#define ADDR_RXBW 18              // 4 bytes (float)
#define ADDR_DATARATE 22          // 4 bytes (int)
#define ADDR_TESLA_FREQ 26        // 4 bytes (float)
#define ADDR_JAMMER_POWER 30      // 1 byte
#define ADDR_JAMMER_SWEEP 31      // 1 byte
#define ADDR_TESLA_COUNT 32       // 1 byte
#define ADDR_TESLA_DELAY 33       // 1 byte
#define ADDR_CHECKSUM 100         // 2 bytes

// Forward declarations
void setDefaultSettings();
void validateSettings();
uint16_t calculateChecksum();

void saveSettings() {
  Serial.println(F("\n[SETTINGS] ========== SAVING SETTINGS =========="));
  
  // Show saving message
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(15, 25);
  display.println(F("Saving settings..."));
  display.display();
  
  // Calculate checksum
  uint16_t checksum = calculateChecksum();
  
  // Write magic number and version
  EEPROM.put(ADDR_MAGIC, (uint32_t)EEPROM_MAGIC);
  EEPROM.write(ADDR_VERSION, EEPROM_VERSION);
  
  // Write settings
  EEPROM.write(ADDR_MODULE, activeModule);
  EEPROM.write(ADDR_MOD, mod);
  EEPROM.write(ADDR_PATTERN, idlePattern);
  EEPROM.write(ADDR_BRIGHTNESS, ledBrightness);
  EEPROM.put(ADDR_FREQUENCY, frequency);
  EEPROM.put(ADDR_DEVIATION, deviation);
  EEPROM.put(ADDR_RXBW, setrxbw);
  EEPROM.put(ADDR_DATARATE, datarate);
  EEPROM.put(ADDR_TESLA_FREQ, teslaFrequency);
  EEPROM.write(ADDR_JAMMER_POWER, jammerPower);
  EEPROM.write(ADDR_JAMMER_SWEEP, jammerSweep ? 1 : 0);
  EEPROM.write(ADDR_TESLA_COUNT, teslaTransmissions);
  EEPROM.write(ADDR_TESLA_DELAY, teslaDelay);
  
  // Write checksum
  EEPROM.put(ADDR_CHECKSUM, checksum);
  
  // Commit changes
  EEPROM.commit();
  
  // Log saved values
  Serial.println(F("[SETTINGS] Saved values:"));
  Serial.printf("  Module: %d\n", activeModule);
  Serial.printf("  Modulation: %d\n", mod);
  Serial.printf("  LED Pattern: %d\n", idlePattern);
  Serial.printf("  LED Brightness: %d\n", ledBrightness);
  Serial.printf("  Frequency: %.2f MHz\n", frequency);
  Serial.printf("  Deviation: %.2f kHz\n", deviation);
  Serial.printf("  RX BW: %.2f kHz\n", setrxbw);
  Serial.printf("  Data Rate: %d kBaud\n", datarate);
  Serial.printf("  Tesla Freq: %.2f MHz\n", teslaFrequency);
  Serial.printf("  Jammer Power: %d dBm\n", jammerPower);
  Serial.printf("  Jammer Sweep: %s\n", jammerSweep ? "ON" : "OFF");
  Serial.printf("  Tesla Count: %d\n", teslaTransmissions);
  Serial.printf("  Tesla Delay: %d ms\n", teslaDelay);
  Serial.printf("  Checksum: 0x%04X\n", checksum);
  
  // Success animation
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(25, 20);
  display.println(F("SAVED!"));
  display.setTextSize(1);
  display.setCursor(15, 45);
  display.printf("Checksum: %04X", checksum);
  display.display();
  
  // Flash LEDs green
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 255, 0));
  }
  pixels.show();
  delay(500);
  pixels.clear();
  pixels.show();
  
  delay(500);
}

void loadSettings() {
  Serial.println(F("\n[SETTINGS] ========== LOADING SETTINGS =========="));
  
  // Check magic number
  uint32_t magic;
  EEPROM.get(ADDR_MAGIC, magic);
  
  if(magic != EEPROM_MAGIC) {
    Serial.println(F("[SETTINGS] No valid settings found, using defaults"));
    setDefaultSettings();
    saveSettings();
    return;
  }
  
  // Check version
  uint8_t version = EEPROM.read(ADDR_VERSION);
  if(version != EEPROM_VERSION) {
    Serial.printf("[SETTINGS] Version mismatch (found %d, expected %d)\n", 
                  version, EEPROM_VERSION);
    setDefaultSettings();
    saveSettings();
    return;
  }
  
  // Load settings
  activeModule = EEPROM.read(ADDR_MODULE);
  mod = EEPROM.read(ADDR_MOD);
  idlePattern = (IdlePattern)EEPROM.read(ADDR_PATTERN);
  ledBrightness = EEPROM.read(ADDR_BRIGHTNESS);
  
  if (ledBrightness == 0 || ledBrightness > 255) {
    ledBrightness = 50;
  }
  
  EEPROM.get(ADDR_FREQUENCY, frequency);
  EEPROM.get(ADDR_DEVIATION, deviation);
  EEPROM.get(ADDR_RXBW, setrxbw);
  EEPROM.get(ADDR_DATARATE, datarate);
  EEPROM.get(ADDR_TESLA_FREQ, teslaFrequency);
  jammerPower = EEPROM.read(ADDR_JAMMER_POWER);
  jammerSweep = EEPROM.read(ADDR_JAMMER_SWEEP) == 1;
  teslaTransmissions = EEPROM.read(ADDR_TESLA_COUNT);
  teslaDelay = EEPROM.read(ADDR_TESLA_DELAY);
  
  // Verify checksum
  uint16_t storedChecksum;
  EEPROM.get(ADDR_CHECKSUM, storedChecksum);
  uint16_t calculatedChecksum = calculateChecksum();
  
  if(storedChecksum != calculatedChecksum) {
    Serial.printf("[SETTINGS] Checksum mismatch (stored: 0x%04X, calc: 0x%04X)\n", 
                  storedChecksum, calculatedChecksum);
    setDefaultSettings();
    saveSettings();
    return;
  }
  
  // Validate loaded values
  validateSettings();
  
  // Apply settings
  pixels.setBrightness(ledBrightness);
  
  Serial.println(F("[SETTINGS] Settings loaded successfully:"));
  Serial.printf("  Module: %d\n", activeModule);
  Serial.printf("  Frequency: %.2f MHz\n", frequency);
  Serial.printf("  Modulation: %d\n", mod);
  Serial.printf("  LED Pattern: %d\n", idlePattern);
  Serial.printf("  LED Brightness: %d\n", ledBrightness);
  Serial.printf("  Tesla Freq: %.2f MHz\n", teslaFrequency);
  Serial.printf("  Jammer Power: %d dBm\n", jammerPower);
  Serial.printf("  Checksum: 0x%04X\n", storedChecksum);
}

void setDefaultSettings() {
  Serial.println(F("[SETTINGS] Setting defaults"));
  
  activeModule = 0;
  mod = 2; // ASK/OOK
  idlePattern = PATTERN_BREATHE;
  ledBrightness = 50;
  frequency = 433.92;
  deviation = 47.60;
  setrxbw = 812.50;
  datarate = 99;
  teslaFrequency = 433.92;
  jammerPower = 12;
  jammerSweep = false;
  teslaTransmissions = 5;
  teslaDelay = 23;
}

void validateSettings() {
  bool needsSave = false;
  
  // Validate frequency
  if (frequency < 300 || frequency > 928) {
    Serial.printf("[SETTINGS] Invalid frequency: %.2f, resetting to 433.92\n", frequency);
    frequency = 433.92;
    needsSave = true;
  }
  
  // Validate Tesla frequency
  if (teslaFrequency != 315.00 && teslaFrequency != 433.92) {
    Serial.printf("[SETTINGS] Invalid Tesla freq: %.2f, resetting to 433.92\n", teslaFrequency);
    teslaFrequency = 433.92;
    needsSave = true;
  }
  
  // Validate deviation
  if (deviation < 0 || deviation > 500) {
    Serial.printf("[SETTINGS] Invalid deviation: %.2f, resetting to 47.60\n", deviation);
    deviation = 47.60;
    needsSave = true;
  }
  
  // Validate RX bandwidth
  if (setrxbw < 50 || setrxbw > 1000) {
    Serial.printf("[SETTINGS] Invalid RX BW: %.2f, resetting to 812.50\n", setrxbw);
    setrxbw = 812.50;
    needsSave = true;
  }
  
  // Validate data rate
  if (datarate < 0 || datarate > 2000) {
    Serial.printf("[SETTINGS] Invalid data rate: %d, resetting to 99\n", datarate);
    datarate = 99;
    needsSave = true;
  }
  
  // Validate module
  if (activeModule > 1) {
    Serial.printf("[SETTINGS] Invalid module: %d, resetting to 0\n", activeModule);
    activeModule = 0;
    needsSave = true;
  }
  
  // Validate modulation
  if (mod > 4) {
    Serial.printf("[SETTINGS] Invalid modulation: %d, resetting to 2\n", mod);
    mod = 2;
    needsSave = true;
  }
  
  // Validate LED pattern
  if (idlePattern > PATTERN_OFF) {
    Serial.printf("[SETTINGS] Invalid LED pattern: %d, resetting to BREATHE\n", idlePattern);
    idlePattern = PATTERN_BREATHE;
    needsSave = true;
  }
  
  // Validate jammer power
  if (jammerPower < -30 || jammerPower > 12) {
    Serial.printf("[SETTINGS] Invalid jammer power: %d, resetting to 12\n", jammerPower);
    jammerPower = 12;
    needsSave = true;
  }
  
  // Validate Tesla settings
  if (teslaTransmissions < 1 || teslaTransmissions > 10) {
    Serial.printf("[SETTINGS] Invalid Tesla count: %d, resetting to 5\n", teslaTransmissions);
    teslaTransmissions = 5;
    needsSave = true;
  }
  
  if (teslaDelay < 10 || teslaDelay > 100) {
    Serial.printf("[SETTINGS] Invalid Tesla delay: %d, resetting to 23\n", teslaDelay);
    teslaDelay = 23;
    needsSave = true;
  }
  
  if (needsSave) {
    Serial.println(F("[SETTINGS] Validation failed, will save corrected values"));
  }
}

uint16_t calculateChecksum() {
  uint16_t sum = 0;
  
  // Sum all setting values
  sum += activeModule;
  sum += mod;
  sum += idlePattern;
  sum += ledBrightness;
  sum += (uint16_t)(frequency * 100);
  sum += (uint16_t)(deviation * 100);
  sum += (uint16_t)(setrxbw * 10);
  sum += datarate;
  sum += (uint16_t)(teslaFrequency * 100);
  sum += jammerPower;
  sum += jammerSweep ? 1 : 0;
  sum += teslaTransmissions;
  sum += teslaDelay;
  
  // Simple checksum
  return sum ^ 0xDEF3;
}

void resetToFactory() {
  Serial.println(F("\n[SETTINGS] ========== FACTORY RESET =========="));
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(F("Factory Reset?"));
  display.println(F(""));
  display.println(F("RIGHT = Confirm"));
  display.println(F("LEFT = Cancel"));
  display.display();
  
  // Wait for RIGHT/SELECT/LEFT to be released first - only reachable via the
  // serial "reset" command today (not a physical menu item), but a held button
  // at that moment would otherwise wipe the EEPROM immediately with no real
  // confirmation, so guard it the same way as the other confirm screens anyway
  while(digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_LEFT) == LOW) {
    delay(10);
  }
  delay(150);

  bool waiting = true;
  unsigned long startTime = millis();

  while(waiting && (millis() - startTime < 10000)) { // 10 second timeout
    if(digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
      // Confirmed
      Serial.println(F("[SETTINGS] Factory reset confirmed"));
      
      display.clearDisplay();
      display.setCursor(10, 25);
      display.println(F("Resetting..."));
      display.display();
      
      // Clear EEPROM
      for(int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0xFF);
      }
      EEPROM.commit();
      
      // Set defaults
      setDefaultSettings();
      saveSettings();
      
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(15, 25);
      display.println(F("RESET!"));
      display.display();
      
      // Red flash
      for(int i = 0; i < NEOPIXEL_COUNT; i++) {
        pixels.setPixelColor(i, pixels.Color(255, 0, 0));
      }
      pixels.show();
      delay(1000);
      pixels.clear();
      pixels.show();
      
      // Restart
      Serial.println(F("[SETTINGS] Restarting..."));
      delay(1000);
      ESP.restart();
    }
    else if(digitalRead(BTN_LEFT) == LOW) {
      // Cancelled
      Serial.println(F("[SETTINGS] Factory reset cancelled"));
      waiting = false;
    }
    
    delay(10);
  }
  
  if(millis() - startTime >= 10000) {
    Serial.println(F("[SETTINGS] Factory reset timeout"));
  }
}

void exportSettings() {
  if(!sdCardPresent) {
    debugPrint("No SD card!", true, true, 2000);
    return;
  }
  
  Serial.println(F("\n[SETTINGS] ========== EXPORT SETTINGS =========="));
  
  char filename[32];
  sprintf(filename, "/settings_%lu.cfg", millis());
  
  File configFile = SD_MMC.open(filename, FILE_WRITE);
  if(!configFile) {
    debugPrint("Export failed!", true, true, 2000);
    return;
  }
  
  // Write settings in readable format
  configFile.println(F("# TPP Badge Configuration"));
  configFile.printf("# Exported: %lu\n", millis());
  configFile.println(F("# Version: 1.0"));
  configFile.println();
  
  configFile.printf("module=%d\n", activeModule);
  configFile.printf("frequency=%.2f\n", frequency);
  configFile.printf("modulation=%d\n", mod);
  configFile.printf("deviation=%.2f\n", deviation);
  configFile.printf("bandwidth=%.2f\n", setrxbw);
  configFile.printf("datarate=%d\n", datarate);
  configFile.printf("tesla_freq=%.2f\n", teslaFrequency);
  configFile.printf("tesla_count=%d\n", teslaTransmissions);
  configFile.printf("tesla_delay=%d\n", teslaDelay);
  configFile.printf("jammer_power=%d\n", jammerPower);
  configFile.printf("jammer_sweep=%d\n", jammerSweep ? 1 : 0);
  configFile.printf("led_pattern=%d\n", idlePattern);
  configFile.printf("led_brightness=%d\n", ledBrightness);
  
  configFile.close();
  
  Serial.printf("[SETTINGS] Exported to %s\n", filename);
  
  display.clearDisplay();
  display.setCursor(0, 20);
  display.println(F("Settings exported!"));
  display.println(filename);
  display.display();
  delay(2000);
}