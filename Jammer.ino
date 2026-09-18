// Jammer.ino - RF Jammer functions

unsigned long jammerStartTime = 0;
float jammerStartFreq = 433.0;
float jammerEndFreq = 434.0;
float jammerCurrentFreq = 433.0;
unsigned long lastJammerUpdate = 0;

// CC1101 Direct SPI functions for Jammer
byte writeJammerRegister(byte addr, byte value, int module) {
  int csPin = (module == 0) ? CC1101_CS_A : CC1101_CS_B;
  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, LOW);
  
  int misoPin = (module == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  while(digitalRead(misoPin));
  
  SPI.transfer(addr);
  SPI.transfer(value);
  
  digitalWrite(csPin, HIGH);
  return value;
}

void strobeJammer(byte cmd, int module) {
  int csPin = (module == 0) ? CC1101_CS_A : CC1101_CS_B;
  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, LOW);
  
  int misoPin = (module == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  while(digitalRead(misoPin));
  
  SPI.transfer(cmd);
  
  digitalWrite(csPin, HIGH);
}

void setJammerFrequency(float freq, int module) {
  // Calculate frequency registers
  byte freq2, freq1, freq0;
  
  // Simplified frequency calculation for common bands
  if(freq >= 299 && freq <= 301) {
    freq2 = 0x0B; freq1 = 0x89; freq0 = 0x9A; // ~300 MHz
  } else if(freq >= 314 && freq <= 316) {
    freq2 = 0x0C; freq1 = 0x4E; freq0 = 0xC4; // ~315 MHz
  } else if(freq >= 432 && freq <= 435) {
    float offset = (freq - 433.92) * 255.0;
    freq2 = 0x10; 
    freq1 = 0xB0 + (int)(offset / 256);
    freq0 = 0x7A + (int)offset % 256;
  } else if(freq >= 867 && freq <= 869) {
    freq2 = 0x21; freq1 = 0x65; freq0 = 0x6A; // ~868 MHz
  } else if(freq >= 914 && freq <= 916) {
    freq2 = 0x23; freq1 = 0x31; freq0 = 0x3B; // ~915 MHz
  } else {
    // Default to requested frequency (rough approximation)
    long freqCalc = (long)(freq * 65536.0 / 26.0);
    freq2 = (freqCalc >> 16) & 0xFF;
    freq1 = (freqCalc >> 8) & 0xFF;
    freq0 = freqCalc & 0xFF;
  }
  
  writeJammerRegister(0x0D, freq2, module);
  writeJammerRegister(0x0E, freq1, module);
  writeJammerRegister(0x0F, freq0, module);
}

void startJammer() {
  if(!cc1101APresent && !cc1101BPresent) {
    debugPrint("No CC1101 found!", true, true, 2000);
    return;
  }
  
  Serial.println(F("\n[JAMMER] ========== STARTING JAMMER =========="));
  Serial.println(F("[JAMMER] WARNING: Check local regulations!"));
  
  debugPrint("Starting Jammer!", true, true, 1000);
  
  jammer_tx = "1";
  pixelMode = PIXEL_JAMMER;
  jammerStartTime = millis();
  
  // Use whichever module is available
  if(activeModule == 0 && !cc1101APresent) {
    activeModule = 1;
    Serial.println(F("[JAMMER] Module A not present, switching to B"));
  } else if(activeModule == 1 && !cc1101BPresent) {
    activeModule = 0;
    Serial.println(F("[JAMMER] Module B not present, switching to A"));
  }
  
  Serial.printf("[JAMMER] Configuration:\n");
  Serial.printf("         Module: %c\n", activeModule == 0 ? 'A' : 'B');
  Serial.printf("         Frequency: %.2f MHz\n", frequency);
  Serial.printf("         Power: %d dBm\n", jammerPower);
  Serial.printf("         Mode: %s\n", jammerSweep ? "Sweep" : "Fixed");
  
  // Set TX pin mode
  int txPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
  pinMode(txPin, OUTPUT);
  digitalWrite(txPin, LOW);
  
  // Initialize SPI for the selected module
  int sckPin = (activeModule == 0) ? CC1101_CLK_A : CC1101_CLK_B;
  int misoPin = (activeModule == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  int mosiPin = (activeModule == 0) ? CC1101_MOSI_A : CC1101_MOSI_B;
  int csPin = (activeModule == 0) ? CC1101_CS_A : CC1101_CS_B;
  
  SPI.begin(sckPin, misoPin, mosiPin, csPin);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  
  // Reset CC1101
  strobeJammer(0x30, activeModule); // SRES
  delay(10);
  
  // Set initial frequency
  setJammerFrequency(frequency, activeModule);
  
  // Configure for ASK/OOK modulation (best for jamming)
  writeJammerRegister(0x10, 0x00, activeModule); // MDMCFG4 - Max BW
  writeJammerRegister(0x11, 0x22, activeModule); // MDMCFG3
  writeJammerRegister(0x12, 0x30, activeModule); // MDMCFG2 - ASK/OOK, no sync
  writeJammerRegister(0x07, 0x04, activeModule); // PKTCTRL1
  writeJammerRegister(0x08, 0x00, activeModule); // PKTCTRL0
  writeJammerRegister(0x1B, 0x40, activeModule); // AGCCTRL2
  writeJammerRegister(0x1C, 0x00, activeModule); // AGCCTRL1
  writeJammerRegister(0x1D, 0x91, activeModule); // AGCCTRL0
  
  // Set TX power based on jammerPower setting
  byte patable = 0xC0; // Default max
  if(jammerPower <= -30) patable = 0x12;
  else if(jammerPower <= -20) patable = 0x0E;
  else if(jammerPower <= -15) patable = 0x1D;
  else if(jammerPower <= -10) patable = 0x34;
  else if(jammerPower <= 0) patable = 0x60;
  else if(jammerPower <= 5) patable = 0x84;
  else if(jammerPower <= 7) patable = 0xC8;
  else if(jammerPower <= 10) patable = 0xC0;
  else patable = 0xC0; // Max power for anything above 10
  
  writeJammerRegister(0x3E, patable, activeModule); // PATABLE
  
  // Configure GDO0 for TX
  writeJammerRegister(0x02, 0x0C, activeModule); // IOCFG0 - Serial TX data
  
  // Calibrate and enter TX
  strobeJammer(0x33, activeModule); // SCAL
  delay(5);
  strobeJammer(0x35, activeModule); // STX
  delay(5);
  
  // Set up sweep parameters if enabled
  if(jammerSweep) {
    jammerStartFreq = frequency - 1.0;
    jammerEndFreq = frequency + 1.0;
    jammerCurrentFreq = jammerStartFreq;
    
    // Ensure frequencies are in valid range
    if(jammerStartFreq < 300.0) jammerStartFreq = 300.0;
    if(jammerEndFreq > 928.0) jammerEndFreq = 928.0;
  }
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("!!! JAMMER !!!"));
  display.drawRect(0, 0, 128, 10, SH110X_WHITE);
  display.setCursor(0,15);
  display.printf("Freq: %.2f MHz\n", frequency);
  display.printf("Power: %d dBm\n", jammerPower);
  display.printf("Module: %c\n", activeModule == 0 ? 'A' : 'B');
  display.setCursor(0,50);
  display.println(F("Press any button"));
  display.println(F("to stop"));
  display.display();
  
  Serial.println(F("[JAMMER] Jammer active - Press any button to stop"));
}

void stopJammer() {
  Serial.println(F("\n[JAMMER] ========== STOPPING JAMMER =========="));
  
  debugPrint("Stopping Jammer", true, true, 1000);
  
  jammer_tx = "0";
  pixelMode = PIXEL_MENU;
  
  if(cc1101APresent || cc1101BPresent) {
    strobeJammer(0x36, activeModule); // SIDLE
    Serial.println(F("[JAMMER] CC1101 set to idle"));
    
    SPI.endTransaction();
    SPI.end();
  }
  
  // Set TX pin low
  int txPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
  digitalWrite(txPin, LOW);
  
  unsigned long jamTime = (millis() - jammerStartTime) / 1000;
  Serial.printf("[JAMMER] Total jam time: %lu seconds\n", jamTime);
  
  display.clearDisplay();
  display.setCursor(0,20);
  display.println(F("Jammer Stopped!"));
  display.printf("Duration: %lu sec\n", jamTime);
  display.display();
  delay(1500);
  
  // Return to jammer menu
  currentMenu = MENU_JAMMER;
  updateDisplay();
}

void runJammer() {
  int txPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
  
  // Check for button press to stop
  if(digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW || 
     digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW || 
     digitalRead(BTN_SELECT) == LOW) {
    if(millis() - lastButtonPress > buttonDebounce) {
      lastButtonPress = millis();
      stopJammer();
      return;
    }
  }
  
  // Different jamming patterns
  static int pattern = 0;
  static unsigned long patternTime = 0;
  
  if(millis() - patternTime > 100) {
    patternTime = millis();
    pattern = (pattern + 1) % 4;
  }
  
  switch(pattern) {
    case 0:
      // Fast pulse jamming
      for(int i = 0; i < 10; i++) {
        digitalWrite(txPin, HIGH);
        delayMicroseconds(100);
        digitalWrite(txPin, LOW);
        delayMicroseconds(100);
      }
      break;
      
    case 1:
      // Random noise
      for(int i = 0; i < 20; i++) {
        digitalWrite(txPin, random(2));
        delayMicroseconds(random(50, 200));
      }
      break;
      
    case 2:
      // Continuous carrier
      digitalWrite(txPin, HIGH);
      delay(5);
      digitalWrite(txPin, LOW);
      break;
      
    case 3:
      // Sweep pattern
      for(int i = 0; i < 50; i++) {
        digitalWrite(txPin, HIGH);
        delayMicroseconds(i * 10);
        digitalWrite(txPin, LOW);
        delayMicroseconds(i * 10);
      }
      break;
  }
  
  // Update display periodically
  if(millis() - lastJammerUpdate > 1000) {
    lastJammerUpdate = millis();
    updateJammerDisplay();
    
    // Frequency sweep mode
    if(jammerSweep) {
      jammerCurrentFreq += 0.1;
      if(jammerCurrentFreq > jammerEndFreq) {
        jammerCurrentFreq = jammerStartFreq;
      }
      
      // Update frequency
      strobeJammer(0x36, activeModule); // SIDLE
      setJammerFrequency(jammerCurrentFreq, activeModule);
      strobeJammer(0x33, activeModule); // SCAL
      delay(1);
      strobeJammer(0x35, activeModule); // STX
      
      Serial.printf("[JAMMER] Sweep frequency: %.2f MHz\n", jammerCurrentFreq);
    }
  }
}

void updateJammerDisplay() {
  unsigned long runtime = (millis() - jammerStartTime) / 1000;
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("!!! JAMMING !!!"));
  display.drawRect(0, 0, 128, 10, SH110X_WHITE);
  
  // Flashing warning
  if((millis() / 500) % 2) {
    display.fillRect(1, 1, 126, 8, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setCursor(35, 1);
    display.print(F("JAMMING"));
    display.setTextColor(SH110X_WHITE);
  }
  
  display.setCursor(0,15);
  if(jammerSweep) {
    display.printf("Freq: %.2f MHz\n", jammerCurrentFreq);
    display.printf("Range: %.1f-%.1f\n", jammerStartFreq, jammerEndFreq);
  } else {
    display.printf("Freq: %.2f MHz\n", frequency);
  }
  display.printf("Power: %d dBm\n", jammerPower);
  display.printf("Time: %02lu:%02lu\n", runtime / 60, runtime % 60);
  
  // Activity indicator
  display.setCursor(0,45);
  for(int i = 0; i < 16; i++) {
    display.print((millis() / 100 + i) % 2 ? "=" : "-");
  }
  
  display.setCursor(0,55);
  display.println(F("Any button = stop"));
  display.display();
}

void configureJammer() {
  Serial.println(F("\n[JAMMER] ===== JAMMER CONFIG ====="));

  // Wait for the button that opened this menu to be released first - otherwise
  // it's still LOW on the very first check below and instantly selects option 0
  while(digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
    delay(10);
  }
  delay(150);

  bool configuring = true;
  int configOption = 0;
  const int numOptions = 4;
  
  while(configuring) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ JAM CONFIG ]="));
    display.println(F(""));
    
    // Menu options
    const char* options[] = {
      "Frequency",
      "Power",
      "Mode",
      "Back"
    };
    
    // Draw menu
    for(int i = 0; i < numOptions; i++) {
      if(i == configOption) {
        display.fillRect(0, 16 + i*10, 128, 10, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
      } else {
        display.setTextColor(SH110X_WHITE);
      }
      
      display.setCursor(2, 17 + i*10);
      display.print(options[i]);
      
      // Show current values
      display.setCursor(70, 17 + i*10);
      switch(i) {
        case 0:
          display.printf("%.2f", frequency);
          break;
        case 1:
          display.printf("%d dBm", jammerPower);
          break;
        case 2:
          display.print(jammerSweep ? "Sweep" : "Fixed");
          break;
      }
    }
    
    display.setTextColor(SH110X_WHITE);
    display.display();
    
    // Handle input
    if(millis() - lastButtonPress > buttonDebounce) {
      if(digitalRead(BTN_UP) == LOW) {
        lastButtonPress = millis();
        configOption = (configOption - 1 + numOptions) % numOptions;
        Serial.printf("[JAMMER] Config option: %d\n", configOption);
      }
      else if(digitalRead(BTN_DOWN) == LOW) {
        lastButtonPress = millis();
        configOption = (configOption + 1) % numOptions;
        Serial.printf("[JAMMER] Config option: %d\n", configOption);
      }
      else if(digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
        lastButtonPress = millis();
        
        switch(configOption) {
          case 0: // Frequency
            adjustJammerFrequency();
            break;
          case 1: // Power
            adjustJammerPower();
            break;
          case 2: // Mode
            jammerSweep = !jammerSweep;
            if(jammerSweep) {
              setupJammerSweep();
            }
            Serial.printf("[JAMMER] Mode: %s\n", jammerSweep ? "Sweep" : "Fixed");
            break;
          case 3: // Back
            configuring = false;
            break;
        }
      }
      else if(digitalRead(BTN_LEFT) == LOW) {
        lastButtonPress = millis();
        configuring = false;
      }
    }
    
    delay(10);
  }
  
  saveSettings();
  Serial.println(F("[JAMMER] Configuration saved"));
}

void adjustJammerFrequency() {
  Serial.println(F("[JAMMER] Adjusting frequency"));

  // Wait for the button that opened this screen to be released first - it's
  // checked again below as the exit condition, so a still-held SELECT/RIGHT would
  // otherwise exit again immediately, before any adjustment could be made
  while(digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
    delay(10);
  }
  delay(150);

  bool adjusting = true;
  
  while(adjusting) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ FREQUENCY ]="));
    display.println(F(""));
    display.setCursor(10,20);
    display.setTextSize(2);
    display.printf("%.2f", frequency);
    display.setTextSize(1);
    display.println(F(" MHz"));
    display.setCursor(0,45);
    display.println(F("UP/DOWN: +/- 0.25"));
    display.println(F("LEFT: Back"));
    display.display();
    
    if(millis() - lastButtonPress > buttonDebounce) {
      if(digitalRead(BTN_UP) == LOW) {
        lastButtonPress = millis();
        frequency += 0.25;
        if(frequency > 928.0) frequency = 300.0;
        Serial.printf("[JAMMER] Frequency: %.2f MHz\n", frequency);
      }
      else if(digitalRead(BTN_DOWN) == LOW) {
        lastButtonPress = millis();
        frequency -= 0.25;
        if(frequency < 300.0) frequency = 928.0;
        Serial.printf("[JAMMER] Frequency: %.2f MHz\n", frequency);
      }
      else if(digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_SELECT) == LOW) {
        lastButtonPress = millis();
        adjusting = false;
      }
    }
    
    delay(10);
  }
}

void adjustJammerPower() {
  Serial.println(F("[JAMMER] Adjusting power"));

  // Wait for the button that opened this screen to be released first - same
  // reasoning as adjustJammerFrequency()
  while(digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
    delay(10);
  }
  delay(150);

  bool adjusting = true;

  while(adjusting) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ TX POWER ]="));
    display.println(F(""));
    display.setCursor(20,20);
    display.setTextSize(2);
    display.printf("%d dBm", jammerPower);
    display.setTextSize(1);
    display.setCursor(0,45);
    display.println(F("UP/DOWN: +/- 1dBm"));
    display.println(F("LEFT: Back"));
    display.display();
    
    if(millis() - lastButtonPress > buttonDebounce) {
      if(digitalRead(BTN_UP) == LOW) {
        lastButtonPress = millis();
        jammerPower++;
        if(jammerPower > 12) jammerPower = 12;
        Serial.printf("[JAMMER] Power: %d dBm\n", jammerPower);
      }
      else if(digitalRead(BTN_DOWN) == LOW) {
        lastButtonPress = millis();
        jammerPower--;
        if(jammerPower < -30) jammerPower = -30;
        Serial.printf("[JAMMER] Power: %d dBm\n", jammerPower);
      }
      else if(digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_SELECT) == LOW) {
        lastButtonPress = millis();
        adjusting = false;
      }
    }
    
    delay(10);
  }
}

void setupJammerSweep() {
  Serial.println(F("[JAMMER] Setting up frequency sweep"));
  
  jammerStartFreq = frequency - 1.0;
  jammerEndFreq = frequency + 1.0;
  jammerCurrentFreq = jammerStartFreq;
  
  // Ensure frequencies are in valid range
  if(jammerStartFreq < 300.0) jammerStartFreq = 300.0;
  if(jammerEndFreq > 928.0) jammerEndFreq = 928.0;
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ SWEEP MODE ]="));
  display.println(F(""));
  display.printf("Start: %.2f MHz\n", jammerStartFreq);
  display.printf("End: %.2f MHz\n", jammerEndFreq);
  display.println(F(""));
  display.println(F("Sweep enabled!"));
  display.display();
  delay(2000);
  
  Serial.printf("[JAMMER] Sweep range: %.2f - %.2f MHz\n", jammerStartFreq, jammerEndFreq);
}