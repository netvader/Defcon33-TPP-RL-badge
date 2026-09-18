// TX.ino - All TX mode functions

long data_to_send[2000];
int data_count = 0;
bool txActive = false;
bool TX_DEMO_MODE = false;  // SD card (4-bit SDIO) and RX capture/save are now implemented

// CC1101 Direct SPI functions
byte writeTXRegister(byte addr, byte value, int module) {
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

void strobeTX(byte cmd, int module) {
  int csPin = (module == 0) ? CC1101_CS_A : CC1101_CS_B;
  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, LOW);
  
  int misoPin = (module == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  while(digitalRead(misoPin));
  
  SPI.transfer(cmd);
  
  digitalWrite(csPin, HIGH);
}

void showComingSoonAnimation() {
  // Fun "coming soon" animation
  display.clearDisplay();
  
  // Pirate-themed animation
  for(int frame = 0; frame < 30; frame++) {
    display.clearDisplay();
    
    // Draw skull
    int skullY = 15 + sin(frame * 0.2) * 5;
    display.fillCircle(64, skullY, 8, SH110X_WHITE);
    display.fillCircle(61, skullY - 2, 2, SH110X_BLACK);
    display.fillCircle(67, skullY - 2, 2, SH110X_BLACK);
    
    // Text that fades in
    if(frame > 10) {
      display.setCursor(20, 35);
      display.println(F("COMING SOON"));
    }
    if(frame > 20) {
      display.setCursor(15, 45);
      display.println(F("Future Update!"));
    }
    
    display.display();
    
    // Rainbow effect on pixels
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      int hue = (frame * 10 + i * 20) % 360;
      pixels.setPixelColor(i, pixels.ColorHSV(hue * 182, 255, 100));
    }
    pixels.show();
    delay(50);
  }
  
  delay(1000);
  pixels.clear();
  pixels.show();
}

void runDemoTX(const char* modeName) {
  Serial.printf("[TX DEMO] Running demo for %s\n", modeName);
  
  // Simulate TX with fun animations
  display.clearDisplay();
  display.setCursor(0,0);
  display.print(F("=[ "));
  display.print(modeName);
  display.println(F(" ]="));
  display.drawRect(0, 0, 128, 10, SH110X_WHITE);
  display.setCursor(0,15);
  display.printf("Freq: %.2f MHz\n", frequency);
  display.printf("Module: %c\n", activeModule == 0 ? 'A' : 'B');
  display.println(F(""));
  display.println(F("Demo Mode Active"));
  display.display();
  delay(1000);
  
  display.fillRect(0, 35, 128, 20, SH110X_BLACK);
  display.setCursor(0, 35);
  display.println(F("Transmitting..."));
  display.display();
  
  // Fun pixel patterns during "transmission"
  unsigned long demoStart = millis();
  int pattern = 0;
  
  while(millis() - demoStart < 5000) {  // 5 second demo
    // Check for abort
    if(digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_SELECT) == LOW) {
      break;
    }
    
    // Update display with animated bars
    display.fillRect(0, 45, 128, 19, SH110X_BLACK);
    
    // Animated signal pattern
    for(int x = 0; x < 128; x += 4) {
      int height = 4 + sin((x + millis() / 50.0) * 0.1) * 4;
      display.drawLine(x, 64 - height, x, 64, SH110X_WHITE);
    }
    
    // Progress indicator
    int progress = map(millis() - demoStart, 0, 5000, 0, 100);
    display.fillRect(0, 52, 128, 10, SH110X_BLACK);
    display.setCursor(0, 52);
    display.printf("Progress: %d%%", progress);
    display.display();
    
    // Cycle through different LED patterns
    pattern = ((millis() - demoStart) / 500) % 4;
    
    switch(pattern) {
      case 0:  // Wave effect
        for(int i = 0; i < NEOPIXEL_COUNT; i++) {
          int brightness = 128 + 127 * sin((millis() / 100.0) + (i * 0.5));
          pixels.setPixelColor(i, pixels.Color(brightness, 0, 0));
        }
        break;
        
      case 1:  // Pulse effect
        {
          int pulse = (millis() / 10) % 255;
          for(int i = 0; i < NEOPIXEL_COUNT; i++) {
            pixels.setPixelColor(i, pixels.Color(0, pulse, 0));
          }
        }
        break;
        
      case 2:  // Chase effect
        pixels.clear();
        {
          int pos = (millis() / 50) % NEOPIXEL_COUNT;
          for(int i = 0; i < 3; i++) {
            int idx = (pos + i) % NEOPIXEL_COUNT;
            pixels.setPixelColor(idx, pixels.Color(0, 0, 255 - i * 80));
          }
        }
        break;
        
      case 3:  // Random sparkle
        for(int i = 0; i < NEOPIXEL_COUNT; i++) {
          if(random(10) < 3) {
            pixels.setPixelColor(i, pixels.Color(255, 255, 255));
          } else {
            pixels.setPixelColor(i, pixels.Color(0, 0, 0));
          }
        }
        break;
    }
    
    pixels.show();
    delay(20);
  }
  
  // Success animation
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15, 15);
  display.println(F("SENT!"));
  display.setTextSize(1);
  display.setCursor(20, 35);
  display.println(F("(Demo Mode)"));
  display.setCursor(10, 50);
  display.print(modeName);
  display.display();
  
  // Victory rainbow sweep on LEDs
  for(int sweep = 0; sweep < 50; sweep++) {
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      int hue = (sweep * 5 + i * 30) % 360;
      pixels.setPixelColor(i, pixels.ColorHSV(hue * 182, 255, 255));
    }
    pixels.show();
    delay(20);
  }
  
  pixels.clear();
  pixels.show();
  delay(1500);
}

void startTX() {
  if(TX_DEMO_MODE) {
    Serial.println(F("[TX] Demo mode active - Simple TX"));
    runDemoTX("TX MODE");
    return;
  }
  
  // Original TX code continues here...
  if(!cc1101APresent && !cc1101BPresent) {
    debugPrint("No CC1101 found!", true, true, 2000);
    return;
  }
  
  Serial.println(F("\n[TX] ========== STARTING TX MODE =========="));
  
  // Use whichever module is available
  if(activeModule == 0 && !cc1101APresent) {
    activeModule = 1;
    Serial.println(F("[TX] Module A not present, switching to B"));
  } else if(activeModule == 1 && !cc1101BPresent) {
    activeModule = 0;
    Serial.println(F("[TX] Module B not present, switching to A"));
  }
  
  Serial.printf("[TX] Configuration:\n");
  Serial.printf("     Module: %c\n", activeModule == 0 ? 'A' : 'B');
  Serial.printf("     Frequency: %.2f MHz\n", frequency);
  Serial.printf("     Modulation: %d (%s)\n", mod, getModulationName(mod));
  Serial.printf("     Deviation: %.2f kHz\n", deviation);
  Serial.printf("     Data Rate: %d kBaud\n", datarate);
  
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
  strobeTX(0x30, activeModule); // SRES
  delay(10);
  
  // Configure frequency
  byte freq2, freq1, freq0;
  // Calculate frequency registers (simplified for common frequencies)
  if(frequency >= 433 && frequency <= 435) {
    freq2 = 0x10; freq1 = 0xB0; freq0 = 0x7A; // ~433.92 MHz
  } else if(frequency >= 314 && frequency <= 316) {
    freq2 = 0x0C; freq1 = 0x4E; freq0 = 0xC4; // ~315 MHz
  } else if(frequency >= 867 && frequency <= 869) {
    freq2 = 0x21; freq1 = 0x65; freq0 = 0x6A; // ~868 MHz
  } else if(frequency >= 914 && frequency <= 916) {
    freq2 = 0x23; freq1 = 0x31; freq0 = 0x3B; // ~915 MHz
  } else {
    // Default to 433.92
    freq2 = 0x10; freq1 = 0xB0; freq0 = 0x7A;
  }
  
  writeTXRegister(0x0D, freq2, activeModule); // FREQ2
  writeTXRegister(0x0E, freq1, activeModule); // FREQ1  
  writeTXRegister(0x0F, freq0, activeModule); // FREQ0
  
  // Configure modulation
  byte mdmcfg2 = 0x00;
  switch(mod) {
    case 0: mdmcfg2 = 0x00; break; // 2-FSK
    case 1: mdmcfg2 = 0x10; break; // GFSK
    case 2: mdmcfg2 = 0x30; break; // ASK/OOK
    case 3: mdmcfg2 = 0x40; break; // 4-FSK
    case 4: mdmcfg2 = 0x70; break; // MSK
  }
  
  writeTXRegister(0x10, 0x00, activeModule); // MDMCFG4 - Max BW
  writeTXRegister(0x11, 0x22, activeModule); // MDMCFG3
  writeTXRegister(0x12, mdmcfg2, activeModule); // MDMCFG2 - Modulation
  writeTXRegister(0x07, 0x04, activeModule); // PKTCTRL1
  writeTXRegister(0x08, 0x00, activeModule); // PKTCTRL0
  writeTXRegister(0x1B, 0x40, activeModule); // AGCCTRL2
  writeTXRegister(0x1C, 0x00, activeModule); // AGCCTRL1
  writeTXRegister(0x1D, 0x91, activeModule); // AGCCTRL0
  
  // Set deviation if using FSK
  if(mod != 2) { // Not ASK/OOK
    writeTXRegister(0x15, 0x47, activeModule); // DEVIATN
  }
  
  // Set TX power to maximum
  writeTXRegister(0x3E, 0xC0, activeModule); // PATABLE - max power
  
  // Configure GDO0 for TX
  writeTXRegister(0x02, 0x0C, activeModule); // IOCFG0 - Serial TX data
  
  // Calibrate and enter TX
  strobeTX(0x33, activeModule); // SCAL
  delay(5);
  strobeTX(0x35, activeModule); // STX
  delay(5);
  
  txActive = true;
  pixelMode = PIXEL_TX;
  // The bit-bang transmit loops that follow this (sendRawData(), sendLastRX(), etc.)
  // are tight timing-critical delayMicroseconds() loops that never call
  // updatePixels() themselves, and by the time they return here to stopTX(),
  // pixelMode is already reverted - so without this, the TX NeoPixel effect never
  // actually got a chance to render even a single frame. One explicit render here
  // (bypassing updatePixels()'s own 20ms rate limit) lights it for the whole
  // transmission instead.
  lastPixelUpdate = 0;
  updatePixels();

  Serial.println(F("[TX] TX mode initialized"));
}

void stopTX() {
  Serial.println(F("\n[TX] ========== STOPPING TX MODE =========="));
  
  txActive = false;
  pixelMode = PIXEL_MENU;
  
  if(!TX_DEMO_MODE && (cc1101APresent || cc1101BPresent)) {
    strobeTX(0x36, activeModule); // SIDLE
    Serial.println(F("[TX] CC1101 set to idle"));
    
    SPI.endTransaction();
    SPI.end();
    
    // Set TX pin low
    int txPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
    digitalWrite(txPin, LOW);
  }
  
  debugPrint("TX Stopped", true, true, 1000);
}

void sendBinaryData() {
  if(TX_DEMO_MODE) {
    Serial.println(F("[TX] Demo mode - Binary TX"));
    runDemoTX("BINARY TX");
    return;
  }
  
  // Original code continues...
  if(!cc1101APresent && !cc1101BPresent) {
    debugPrint("No CC1101 found!", true, true, 2000);
    return;
  }
  
  if(!txActive) {
    startTX();
  }
  
  Serial.println(F("\n[TX] ===== BINARY TX MODE ====="));
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ BINARY TX ]="));
  display.println(F(""));
  display.println(F("Enter data via"));
  display.println(F("serial terminal"));
  display.println(F(""));
  display.println(F("Format: 0,1,0,1..."));
  display.println(F("Send 'x' to exit"));
  display.display();
  
  Serial.println(F("[TX] Binary TX mode - Enter binary data"));
  Serial.println(F("[TX] Format: 0,1,0,1,1,0,1,0..."));
  Serial.println(F("[TX] Send 'x' to exit"));
  Serial.println(F("[TX] Send 't' to transmit"));
  
  String binaryData = "";
  bool collecting = true;
  
  while(collecting) {
    // Check buttons
    if(digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_SELECT) == LOW) {
      Serial.println(F("[TX] Binary TX cancelled by button"));
      break;
    }
    
    if(Serial.available()) {
      char c = Serial.read();
      
      if(c == 'x' || c == 'X') {
        Serial.println(F("[TX] Exiting binary TX mode"));
        collecting = false;
      }
      else if(c == 't' || c == 'T') {
        if(binaryData.length() > 0) {
          Serial.println(F("[TX] Transmitting binary data..."));
          transmitBinaryString(binaryData);
          binaryData = "";
        } else {
          Serial.println(F("[TX] No data to transmit"));
        }
      }
      else if(c == '0' || c == '1') {
        binaryData += c;
        Serial.print(c);
        
        // Update display
        display.clearDisplay();
        display.setCursor(0,0);
        display.println(F("=[ BINARY TX ]="));
        display.println(F(""));
        display.print(F("Bits: "));
        display.println(binaryData.length());
        display.println(F(""));
        
        // Show last 20 bits
        int startIdx = max(0, (int)binaryData.length() - 20);
        display.println(binaryData.substring(startIdx));
        display.display();
      }
      else if(c == ',') {
        // Ignore commas
      }
      else if(c == '\n' || c == '\r') {
        Serial.println();
      }
    }
    
    delay(10);
  }
  
  stopTX();
}

void transmitBinaryString(String data) {
  if(TX_DEMO_MODE) {
    // Demo version
    Serial.printf("[TX DEMO] Would transmit %d bits\n", data.length());
    
    display.clearDisplay();
    display.setCursor(0,20);
    display.println(F("Demo TX..."));
    display.printf("%d bits\n", data.length());
    display.display();
    
    // Show bits on LEDs
    for(int i = 0; i < min(NEOPIXEL_COUNT, (int)data.length()); i++) {
      pixels.setPixelColor(i, data[i] == '1' ? 
        pixels.Color(255, 0, 0) : pixels.Color(0, 0, 255));
    }
    pixels.show();
    delay(2000);
    pixels.clear();
    pixels.show();
    
    display.clearDisplay();
    display.setCursor(0,20);
    display.println(F("Demo Complete!"));
    display.display();
    delay(1000);
    return;
  }
  
  // Original code
  int txPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
  
  Serial.printf("[TX] Transmitting %d bits\n", data.length());
  
  display.clearDisplay();
  display.setCursor(0,20);
  display.println(F("Transmitting..."));
  display.printf("%d bits\n", data.length());
  display.display();
  
  // Make sure we're in TX mode
  strobeTX(0x35, activeModule); // STX
  
  // Transmit each bit
  for(int i = 0; i < data.length(); i++) {
    if(data[i] == '1') {
      digitalWrite(txPin, HIGH);
    } else {
      digitalWrite(txPin, LOW);
    }
    delayMicroseconds(1000); // 1ms per bit
  }
  
  digitalWrite(txPin, LOW);
  
  Serial.println(F("[TX] Transmission complete"));
  
  display.clearDisplay();
  display.setCursor(0,20);
  display.println(F("TX Complete!"));
  display.display();
  delay(1000);
}

void sendLastRX() {
  if(TX_DEMO_MODE) {
    Serial.println(F("[TX] Demo mode - Last RX"));
    runDemoTX("TX LAST RX");
    return;
  }
  
  // Original code...
  if(samplecount == 0) {
    debugPrint("No RX data!", true, true, 2000);
    Serial.println(F("[TX] No received data to transmit"));
    return;
  }
  
  Serial.println(F("\n[TX] ===== TRANSMIT LAST RX ====="));
  Serial.printf("[TX] Transmitting %d samples\n", samplecount);
  
  if(!txActive) {
    startTX();
  }
  
  int txPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ TX LAST RX ]="));
  display.println(F(""));
  display.printf("Samples: %d\n", samplecount);
  display.printf("Freq: %.2f MHz\n", frequency);
  display.println(F(""));
  display.println(F("Transmitting..."));
  display.display();
  
  // Make sure we're in TX mode
  strobeTX(0x35, activeModule); // STX
  
  // Transmit the captured signal 3 times
  for(int repeat = 0; repeat < 3; repeat++) {
    Serial.printf("[TX] Transmission %d/3\n", repeat + 1);
    
    for(int i = 0; i < samplecount; i += 2) {
      digitalWrite(txPin, HIGH);
      delayMicroseconds(sample[i]);
      
      if(i + 1 < samplecount) {
        digitalWrite(txPin, LOW);
        delayMicroseconds(sample[i + 1]);
      }
    }
    
    digitalWrite(txPin, LOW);
    delay(500); // Delay between transmissions
  }
  
  Serial.println(F("[TX] Transmission complete"));
  
  display.clearDisplay();
  display.setCursor(0,20);
  display.println(F("TX Complete!"));
  display.println(F("3 transmissions"));
  display.display();
  delay(1500);
  
  stopTX();
}

void sendRawData(long *data, int count, int transmissions) {
  if(TX_DEMO_MODE) {
    Serial.printf("[TX DEMO] Would send %d data points, %d times\n", count, transmissions);
    runDemoTX("RAW DATA");
    return;
  }
  
  if(!cc1101APresent && !cc1101BPresent) {
    debugPrint("No CC1101 found!", true, true, 2000);
    return;
  }
  
  Serial.println(F("\n[TX] ===== RAW DATA TX ====="));
  Serial.printf("[TX] Data points: %d, Transmissions: %d\n", count, transmissions);
  
  if(!txActive) {
    startTX();
  }
  
  int txPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ RAW TX ]="));
  display.println(F(""));
  display.printf("Points: %d\n", count);
  display.printf("Repeat: %dx\n", transmissions);
  display.display();
  
  for (int r = 0; r < transmissions; r++) {
    Serial.printf("[TX] Transmission %d/%d\n", r + 1, transmissions);
    
    display.fillRect(0, 40, 128, 10, SH110X_BLACK); // clear the previous count first, or digits overlap
    display.setCursor(0, 40);
    display.printf("TX: %d/%d", r + 1, transmissions);
    display.display();

    for (int i = 0; i < count; i += 2) {
      digitalWrite(txPin, HIGH);
      delayMicroseconds(data[i]);
      digitalWrite(txPin, LOW);
      if(i + 1 < count) {
        delayMicroseconds(data[i + 1]);
      }
    }
    
    delay(2000); // Delay between retransmissions
  }
  
  Serial.println(F("[TX] Raw data transmission complete"));
  
  display.clearDisplay();
  display.setCursor(0,20);
  display.println(F("TX Complete!"));
  display.display();
  delay(1500);
  
  stopTX();
}

void loadTXFromFile() {
  if(TX_DEMO_MODE) {
    Serial.println(F("[TX] Demo mode - TX from file"));
    runDemoTX("TX FROM FILE");
    return;
  }
  
  // Original code...
  if(!sdCardPresent) {
    debugPrint("No SD card!", true, true, 2000);
    return;
  }
  
  Serial.println(F("\n[TX] ===== TX FROM FILE ====="));
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ TX FILES ]="));
  display.println(F(""));
  
  // List RX files
  File root = SD_MMC.open("/");
  File file = root.openNextFile();
  int fileCount = 0;
  String fileNames[20];
  
  while(file && fileCount < 20) {
    String name = file.name();
    if(name.startsWith("rx_")) {
      fileNames[fileCount] = name;
      fileCount++;
      
      if(fileCount <= 5) {
        display.println(name);
      }
    }
    file = root.openNextFile();
  }
  
  if(fileCount == 0) {
    display.println(F("No RX files found"));
    display.display();
    delay(2000);
    return;
  }
  
  display.display();
  Serial.printf("[TX] Found %d RX files\n", fileCount);
  
  // For now, just use the first file
  // TODO: Add file selection menu
  if(fileCount > 0) {
    loadAndTransmitFile("/" + fileNames[0]);
  }
}

void loadAndTransmitFile(String filename) {
  if(TX_DEMO_MODE) {
    Serial.printf("[TX DEMO] Would load file: %s\n", filename.c_str());
    runDemoTX("FILE TX");
    return;
  }
  
  Serial.printf("[TX] Loading file: %s\n", filename.c_str());
  
  File dataFile = SD_MMC.open(filename, FILE_READ);
  if(!dataFile) {
    debugPrint("File open failed!", true, true, 2000);
    return;
  }
  
  // Parse file
  float fileFreq = frequency;
  int fileMod = mod;
  int fileCount = 0;
  
  while(dataFile.available()) {
    String line = dataFile.readStringUntil('\n');
    
    if(line.startsWith("Frequency:")) {
      fileFreq = line.substring(11).toFloat();
      Serial.printf("[TX] File frequency: %.2f MHz\n", fileFreq);
    }
    else if(line.startsWith("Modulation:")) {
      fileMod = line.substring(12).toInt();
      Serial.printf("[TX] File modulation: %d\n", fileMod);
    }
    else if(line.startsWith("Count:")) {
      fileCount = line.substring(7).toInt();
      Serial.printf("[TX] File sample count: %d\n", fileCount);
    }
    else if(line.startsWith("Data:")) {
      // Parse timing data
      String dataStr = line.substring(6);
      int idx = 0;
      int dataIdx = 0;
      
      while(idx < dataStr.length() && dataIdx < 2000) {
        int commaPos = dataStr.indexOf(',', idx);
        if(commaPos == -1) commaPos = dataStr.length();
        
        String valStr = dataStr.substring(idx, commaPos);
        data_to_send[dataIdx++] = valStr.toInt();
        
        idx = commaPos + 1;
      }
      
      data_count = dataIdx;
      Serial.printf("[TX] Loaded %d data points\n", data_count);
    }
  }
  
  dataFile.close();
  
  // Set frequency and modulation from file
  frequency = fileFreq;
  mod = fileMod;
  
  // Transmit the data
  sendRawData(data_to_send, data_count, 3);
}

void drawTXBinaryMenu() {
  Serial.println(F("[Display] Drawing TX Binary menu"));
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ BINARY TX ]="));
  display.println(F(""));
  display.println(F("1. Manual Input"));
  display.println(F("2. Test Pattern"));
  display.println(F("3. From File"));
  display.println(F("4. Back"));
  
  display.setCursor(0, 55);
  display.print(F("Select option"));
  display.display();
}

void sendTestPattern() {
  if(TX_DEMO_MODE) {
    // Demo version with just animations
    Serial.println(F("[TX DEMO] Test pattern demo"));
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ TEST DEMO ]="));
    display.println(F(""));
    
    const char* patterns[] = {
      "10101010", 
      "11110000",
      "10011001",
      "11111111"
    };
    
    for(int p = 0; p < 4; p++) {
      display.fillRect(0, 20, 128, 44, SH110X_BLACK);
      display.setCursor(0, 20);
      display.printf("Pattern %d/4\n", p + 1);
      display.println(patterns[p]);
      
      // Draw pattern visually
      for(int i = 0; i < 8; i++) {
        int x = i * 16;
        if(patterns[p][i] == '1') {
          display.fillRect(x, 40, 14, 20, SH110X_WHITE);
        } else {
          display.drawRect(x, 40, 14, 20, SH110X_WHITE);
        }
      }
      display.display();
      
      // LED pattern
      for(int i = 0; i < NEOPIXEL_COUNT; i++) {
        if(patterns[p][i % 8] == '1') {
          pixels.setPixelColor(i, pixels.Color(255, 0, 0));
        } else {
          pixels.setPixelColor(i, pixels.Color(0, 0, 255));
        }
      }
      pixels.show();
      delay(1000);
    }
    
    pixels.clear();
    pixels.show();
    
    display.clearDisplay();
    display.setCursor(0,20);
    display.println(F("Demo Complete!"));
    display.display();
    delay(1500);
    return;
  }
  
  // Original code...
  Serial.println(F("\n[TX] ===== TEST PATTERN TX ====="));
  
  if(!cc1101APresent && !cc1101BPresent) {
    debugPrint("No CC1101 found!", true, true, 2000);
    return;
  }
  
  if(!txActive) {
    startTX();
  }
  
  int txPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
  
  // Common test patterns
  const char* patterns[] = {
    "10101010101010101010", // Alternating
    "11110000111100001111", // Square wave
    "10011001100110011001", // Pattern
    "11111111000000001111"  // Long pulse
  };
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ TEST PATTERN ]="));
  display.println(F(""));
  
  // Make sure we're in TX mode
  strobeTX(0x35, activeModule); // STX
  
  for(int p = 0; p < 4; p++) {
    display.setCursor(0, 20);
    display.printf("Pattern %d/4\n", p + 1);
    display.println(patterns[p]);
    display.display();
    
    Serial.printf("[TX] Sending pattern %d: %s\n", p + 1, patterns[p]);
    
    // Send pattern 5 times
    for(int r = 0; r < 5; r++) {
      for(int i = 0; patterns[p][i] != '\0'; i++) {
        digitalWrite(txPin, patterns[p][i] == '1' ? HIGH : LOW);
        delayMicroseconds(1000);
      }
      digitalWrite(txPin, LOW);
      delay(100);
    }
    
    delay(1000);
  }
  
  Serial.println(F("[TX] Test patterns complete"));
  
  display.clearDisplay();
  display.setCursor(0,20);
  display.println(F("Patterns sent!"));
  display.display();
  delay(1500);
  
  stopTX();
}