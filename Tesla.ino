// Tesla.ino - Tesla charging port signal functions

// Tesla signal parameters
const uint16_t pulseWidth = 400;
const uint16_t messageDistance = 23;
const uint8_t transmtesla = 5;
const uint8_t messageLength = 43;
const uint8_t sequence[messageLength] = { 
  0x02,0xAA,0xAA,0xAA,
  0x2B,
  0x2C,0xCB,0x33,0x33,0x2D,0x34,0xB5,0x2B,0x4D,0x32,0xAD,0x2C,0x56,0x59,0x96,0x66,
  0x66,0x5A,0x69,0x6A,0x56,0x9A,0x65,0x5A,0x58,0xAC,0xB3,0x2C,0xCC,0xCC,0xB4,0xD2,
  0xD4,0xAD,0x34,0xCA,0xB4,0xA0
};

// Additional Tesla configurations
bool teslaRepeat = false;
unsigned long teslaStartTime = 0;

// CC1101 Direct SPI functions (matching RX.ino style)
byte writeCC1101Register(byte addr, byte value, int module) {
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

void strobeCC1101TX(byte cmd, int module) {
  int csPin = (module == 0) ? CC1101_CS_A : CC1101_CS_B;
  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, LOW);
  
  int misoPin = (module == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  while(digitalRead(misoPin));
  
  SPI.transfer(cmd);
  
  digitalWrite(csPin, HIGH);
}

void sendTeslaSignal() {
  if(!cc1101APresent && !cc1101BPresent) {
    debugPrint("No CC1101 found!", true, true, 2000);
    return;
  }
  
  Serial.println(F("\n[TESLA] ========== SENDING TESLA SIGNAL =========="));
  
  debugPrint("Sending Tesla!", true, true, 500);
  
  pixelMode = PIXEL_TESLA;
  teslaStartTime = millis();
  // animateTeslaDelay() below only renders between repeats, never for a single
  // transmission (or after the last one) - without an explicit render here, a
  // 1-shot send (or a short teslaDelay) meant the LED effect never visibly showed
  lastPixelUpdate = 0;
  updatePixels();
  
  // Use whichever module is available
  if(activeModule == 0 && !cc1101APresent) {
    activeModule = 1;
    Serial.println(F("[TESLA] Module A not present, switching to B"));
  } else if(activeModule == 1 && !cc1101BPresent) {
    activeModule = 0;
    Serial.println(F("[TESLA] Module B not present, switching to A"));
  }
  
  int txPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
  pinMode(txPin, OUTPUT);
  digitalWrite(txPin, LOW);
  
  Serial.printf("[TESLA] Configuration:\n");
  Serial.printf("        Module: %c\n", activeModule == 0 ? 'A' : 'B');
  Serial.printf("        Frequency: %.2f MHz\n", teslaFrequency);
  Serial.printf("        Transmissions: %d\n", teslaTransmissions);
  Serial.printf("        Pulse Width: %d us\n", pulseWidth);
  Serial.printf("        Message Distance: %d ms\n", teslaDelay);
  Serial.printf("        Signal Length: %d bytes\n", messageLength);
  
  // Initialize SPI for the selected module
  int sckPin = (activeModule == 0) ? CC1101_CLK_A : CC1101_CLK_B;
  int misoPin = (activeModule == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  int mosiPin = (activeModule == 0) ? CC1101_MOSI_A : CC1101_MOSI_B;
  int csPin = (activeModule == 0) ? CC1101_CS_A : CC1101_CS_B;
  
  SPI.begin(sckPin, misoPin, mosiPin, csPin);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  
  // Reset CC1101
  strobeCC1101TX(0x30, activeModule); // SRES
  delay(10);
  
  // Configure for Tesla TX
  // Set frequency for Tesla (315 or 433.92 MHz)
  if(teslaFrequency > 400) {
    // 433.92 MHz
    writeCC1101Register(0x0D, 0x10, activeModule); // FREQ2
    writeCC1101Register(0x0E, 0xB0, activeModule); // FREQ1  
    writeCC1101Register(0x0F, 0x7A, activeModule); // FREQ0
  } else {
    // 315 MHz
    writeCC1101Register(0x0D, 0x0C, activeModule); // FREQ2
    writeCC1101Register(0x0E, 0x4E, activeModule); // FREQ1  
    writeCC1101Register(0x0F, 0xC4, activeModule); // FREQ0
  }
  
  // Configure for ASK/OOK modulation
  writeCC1101Register(0x10, 0x00, activeModule); // MDMCFG4 - Max BW
  writeCC1101Register(0x11, 0x22, activeModule); // MDMCFG3
  writeCC1101Register(0x12, 0x30, activeModule); // MDMCFG2 - ASK/OOK, no sync
  writeCC1101Register(0x07, 0x04, activeModule); // PKTCTRL1
  writeCC1101Register(0x08, 0x00, activeModule); // PKTCTRL0
  writeCC1101Register(0x1B, 0x40, activeModule); // AGCCTRL2
  writeCC1101Register(0x1C, 0x00, activeModule); // AGCCTRL1
  writeCC1101Register(0x1D, 0x91, activeModule); // AGCCTRL0
  
  // Set TX power to maximum
  writeCC1101Register(0x3E, 0xC0, activeModule); // PATABLE - max power
  
  // Configure GDO0 for TX
  writeCC1101Register(0x02, 0x0C, activeModule); // IOCFG0 - Serial TX data
  
  // Calibrate and enter TX
  strobeCC1101TX(0x33, activeModule); // SCAL
  delay(5);
  strobeCC1101TX(0x35, activeModule); // STX
  delay(5);
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ TESLA TX ]="));
  display.drawRect(0, 0, 128, 10, SH110X_WHITE);
  display.setCursor(0,15);
  display.print(F("Freq: "));
  display.print(teslaFrequency, 2);
  display.println(F(" MHz"));
  display.println(F(""));
  display.println(F("Sending signal..."));
  display.display();
  
  Serial.println(F("[TESLA] Signal sequence:"));
  for(int i = 0; i < messageLength; i++) {
    Serial.printf("0x%02X ", sequence[i]);
    if((i + 1) % 16 == 0) Serial.println();
  }
  Serial.println();
  
  // Send the signal
  for (uint8_t t = 0; t < teslaTransmissions; t++) {
    Serial.printf("[TESLA] Transmission %d/%d\n", t + 1, teslaTransmissions);
    
    // Update display with progress
    display.fillRect(0, 35, 128, 10, SH110X_BLACK);
    display.setCursor(0, 35);
    display.print(F("TX: "));
    display.print(t + 1);
    display.print(F("/"));
    display.print(teslaTransmissions);
    
    // Progress bar
    int progress = map(t, 0, teslaTransmissions - 1, 0, 100);
    display.drawRect(10, 48, 102, 8, SH110X_WHITE);
    display.fillRect(11, 49, progress, 6, SH110X_WHITE);
    display.display();
    
    // Send each byte using direct pin control
    for (uint8_t i = 0; i < messageLength; i++) {
      sendByte(sequence[i], txPin);
    }
    
    digitalWrite(txPin, LOW);
    
    // Animation during delay
    if(t < teslaTransmissions - 1) {
      animateTeslaDelay(teslaDelay);
    }
  }
  
  // Return to idle
  strobeCC1101TX(0x36, activeModule); // SIDLE
  
  SPI.endTransaction();
  SPI.end();
  
  pixelMode = PIXEL_MENU;
  
  unsigned long txTime = millis() - teslaStartTime;
  Serial.printf("[TESLA] Transmission complete in %lu ms\n", txTime);
  
  // Success display
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15, 20);
  display.println(F("SENT!"));
  display.setTextSize(1);
  display.setCursor(20, 45);
  display.printf("Time: %lu ms", txTime);
  display.display();
  
  // Success animation on LEDs (function is defined in Pixels.ino)
  teslaSuccessAnimation();
  
  delay(1500);
  updateDisplay();
}

void sendByte(uint8_t dataByte, int txPin) {
  for (int8_t bit = 7; bit >= 0; bit--) {
    digitalWrite(txPin, (dataByte & (1 << bit)) != 0 ? HIGH : LOW);
    delayMicroseconds(pulseWidth);
  }
}

void animateTeslaDelay(int delayTime) {
  // Animate during the delay between transmissions
  unsigned long startDelay = millis();
  
  while(millis() - startDelay < delayTime) {
    // Check for abort
    if(digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_SELECT) == LOW) {
      Serial.println(F("[TESLA] Transmission aborted by user"));
      return;
    }
    
    // Update LED animation
    if(millis() - lastPixelUpdate > 20) {
      lastPixelUpdate = millis();
      lightningAnimation();
      pixels.show();
    }
    
    delay(1);
  }
}

void configureTesla() {
  Serial.println(F("\n[TESLA] ===== TESLA CONFIG ====="));
  
  bool configuring = true;
  int configOption = 0;
  const int numOptions = 5;
  
  while(configuring) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ TESLA CFG ]="));
    display.println(F(""));
    
    // Menu options with current values
    display.setCursor(0, 16);
    display.print(configOption == 0 ? ">" : " ");
    display.print(F("Freq: "));
    display.print(teslaFrequency, 2);
    display.println(F(" MHz"));
    
    display.print(configOption == 1 ? ">" : " ");
    display.print(F("Count: "));
    display.println(teslaTransmissions);
    
    display.print(configOption == 2 ? ">" : " ");
    display.print(F("Delay: "));
    display.print(teslaDelay);
    display.println(F(" ms"));
    
    display.print(configOption == 3 ? ">" : " ");
    display.print(F("Repeat: "));
    display.println(teslaRepeat ? "ON" : "OFF");
    
    display.print(configOption == 4 ? ">" : " ");
    display.println(F("Back"));
    
    display.display();
    
    // Handle input
    if(millis() - lastButtonPress > buttonDebounce) {
      if(digitalRead(BTN_UP) == LOW) {
        lastButtonPress = millis();
        configOption = (configOption - 1 + numOptions) % numOptions;
        Serial.printf("[TESLA] Config option: %d\n", configOption);
      }
      else if(digitalRead(BTN_DOWN) == LOW) {
        lastButtonPress = millis();
        configOption = (configOption + 1) % numOptions;
        Serial.printf("[TESLA] Config option: %d\n", configOption);
      }
      else if(digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
        lastButtonPress = millis();
        
        switch(configOption) {
          case 0: // Frequency toggle
            if (teslaFrequency == 433.92) {
              teslaFrequency = 315.00;
            } else {
              teslaFrequency = 433.92;
            }
            Serial.printf("[TESLA] Frequency: %.2f MHz\n", teslaFrequency);
            break;
            
          case 1: // Transmission count
            teslaTransmissions++;
            if(teslaTransmissions > 10) teslaTransmissions = 1;
            Serial.printf("[TESLA] Transmissions: %d\n", teslaTransmissions);
            break;
            
          case 2: // Delay
            teslaDelay += 5;
            if(teslaDelay > 100) teslaDelay = 10;
            Serial.printf("[TESLA] Delay: %d ms\n", teslaDelay);
            break;
            
          case 3: // Repeat mode
            teslaRepeat = !teslaRepeat;
            Serial.printf("[TESLA] Repeat: %s\n", teslaRepeat ? "ON" : "OFF");
            break;
            
          case 4: // Back
            configuring = false;
            break;
        }
        
        if(configOption < 4) {
          saveSettings();
        }
      }
      else if(digitalRead(BTN_LEFT) == LOW) {
        lastButtonPress = millis();
        configuring = false;
      }
    }
    
    delay(10);
  }
  
  Serial.println(F("[TESLA] Configuration complete"));
}

void sendTeslaRepeat() {
  if(!teslaRepeat) {
    sendTeslaSignal();
    return;
  }
  
  Serial.println(F("\n[TESLA] ===== REPEAT MODE ====="));
  
  bool repeating = true;
  int repeatCount = 0;
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ TESLA RPT ]="));
  display.println(F(""));
  display.println(F("Repeat mode ON"));
  display.println(F(""));
  display.println(F("Any button stops"));
  display.display();
  delay(1000);
  
  while(repeating) {
    // Check for stop
    if(digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW || 
       digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW || 
       digitalRead(BTN_SELECT) == LOW) {
      if(millis() - lastButtonPress > buttonDebounce) {
        lastButtonPress = millis();
        repeating = false;
        break;
      }
    }
    
    repeatCount++;
    Serial.printf("[TESLA] Repeat transmission %d\n", repeatCount);
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ TESLA RPT ]="));
    display.println(F(""));
    display.printf("Count: %d\n", repeatCount);
    display.printf("Freq: %.2f MHz\n", teslaFrequency);
    display.println(F(""));
    display.println(F("Sending..."));
    display.display();
    
    // Send the signal
    sendTeslaSignal();
    
    // Delay between repeats
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ TESLA RPT ]="));
    display.println(F(""));
    display.printf("Count: %d\n", repeatCount);
    display.println(F(""));
    display.println(F("Waiting..."));
    display.println(F(""));
    display.println(F("Any button stops"));
    display.display();
    
    // Wait with ability to cancel
    unsigned long waitStart = millis();
    while(millis() - waitStart < 5000) { // 5 second delay between repeats
      if(digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW || 
         digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW || 
         digitalRead(BTN_SELECT) == LOW) {
        repeating = false;
        break;
      }
      delay(10);
    }
  }
  
  Serial.printf("[TESLA] Repeat mode stopped after %d transmissions\n", repeatCount);
  
  display.clearDisplay();
  display.setCursor(0,20);
  display.println(F("Repeat stopped"));
  display.printf("Total: %d\n", repeatCount);
  display.display();
  delay(1500);
}

void analyzeTeslaSignal() {
  Serial.println(F("\n[TESLA] ===== SIGNAL ANALYSIS ====="));
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ TESLA INFO ]="));
  display.println(F(""));
  display.printf("Length: %d bytes\n", messageLength);
  display.printf("Pulse: %d us\n", pulseWidth);
  display.println(F(""));
  display.println(F("Header: 02AAAAAA"));
  display.println(F("Type: ASK/OOK"));
  display.display();
  
  Serial.println(F("[TESLA] Signal structure:"));
  Serial.printf("  Header: 0x%02X 0x%02X 0x%02X 0x%02X\n", 
                sequence[0], sequence[1], sequence[2], sequence[3]);
  Serial.printf("  Key byte: 0x%02X\n", sequence[4]);
  Serial.println(F("  Payload:"));
  
  for(int i = 5; i < messageLength; i++) {
    Serial.printf("  [%02d] 0x%02X - ", i, sequence[i]);
    for(int bit = 7; bit >= 0; bit--) {
      Serial.print((sequence[i] >> bit) & 1);
    }
    Serial.println();
  }
  
  delay(5000);
}