// Menu.ino - Menu system and navigation with serial command support

// Serial command buffer
String serialCommand = "";

void handleButtons() {
  // Handle serial commands
  handleSerialCommands();
  
  if (millis() - lastButtonPress < buttonDebounce) return;
  
  // Special handling for LEFT button during RX mode to change visualization
  if (raw_rx == "1" && digitalRead(BTN_LEFT) == LOW) {
    lastButtonPress = millis();
    cycleRXVisualization();
    return;
  }
  
  // If in RX (or Weather Station) mode and any other button is pressed, stop and go back to menu
  if (raw_rx == "1") {
    if (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW ||
        digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
      lastButtonPress = millis();
      if(currentMenu == MENU_WEATHER) {
        stopWeatherStation();
      } else {
        stopRX();
      }
      return;
    }
  }
  
  // Special handling for TX Binary menu
  if (currentMenu == MENU_TX_BINARY) {
    handleTXBinaryMenu();
    return;
  }
  
  // Normal menu navigation when not in RX mode
  if (digitalRead(BTN_UP) == LOW) {
    lastButtonPress = millis();
    handleMenuUp();
  }
  else if (digitalRead(BTN_DOWN) == LOW) {
    lastButtonPress = millis();
    handleMenuDown();
  }
  else if (digitalRead(BTN_LEFT) == LOW) {
    lastButtonPress = millis();
    handleMenuBack();
  }
  else if (digitalRead(BTN_RIGHT) == LOW) {
    lastButtonPress = millis();
    handleMenuSelect();
  }
  else if (digitalRead(BTN_SELECT) == LOW) {
    lastButtonPress = millis();
    handleMenuSelect();
  }
}

void handleSerialCommands() {
  while(Serial.available()) {
    char c = Serial.read();
    
    if(c == '\n' || c == '\r') {
      // Process command when Enter is pressed
      if(serialCommand.length() > 0) {
        processSerialCommand(serialCommand);
        serialCommand = "";
      }
    } else if(c >= 32 && c <= 126) { // Printable ASCII
      serialCommand += c;
      if(serialCommand.length() > 100) {
        serialCommand = ""; // Prevent overflow
      }
    }
  }
}

void processSerialCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  
  Serial.print(F("[SERIAL] Command: "));
  Serial.println(cmd);
  
  // Navigation commands
  if(cmd == "up") {
    Serial.println(F("[SERIAL] UP"));
    lastButtonPress = millis();
    handleMenuUp();
  }
  else if(cmd == "down") {
    Serial.println(F("[SERIAL] DOWN"));
    lastButtonPress = millis();
    handleMenuDown();
  }
  else if(cmd == "left") {
    Serial.println(F("[SERIAL] LEFT"));
    lastButtonPress = millis();
    if (raw_rx == "1") {
      cycleRXVisualization();
    } else {
      handleMenuBack();
    }
  }
  else if(cmd == "right") {
    Serial.println(F("[SERIAL] RIGHT"));
    lastButtonPress = millis();
    handleMenuSelect();
  }
  else if(cmd == "select" || cmd == "enter") {
    Serial.println(F("[SERIAL] SELECT"));
    lastButtonPress = millis();
    handleMenuSelect();
  }
  else if(cmd == "back") {
    Serial.println(F("[SERIAL] BACK"));
    lastButtonPress = millis();
    handleMenuBack();
  }
  // System commands
  else if(cmd == "help" || cmd == "?") {
    printSerialHelp();
  }
  else if(cmd == "reboot" || cmd == "restart") {
    Serial.println(F("[SERIAL] Rebooting..."));
    delay(100);
    ESP.restart();
  }
  else if(cmd == "reset") {
    Serial.println(F("[SERIAL] Factory reset..."));
    resetToFactory();
    delay(100);
    ESP.restart();
  }
  else if(cmd == "status") {
    printStatus();
  }
  else if(cmd == "info") {
    printSystemInfo();
  }
  // Operation commands
  else if(cmd == "rx") {
    Serial.println(F("[SERIAL] Starting RX mode"));
    if(cc1101APresent || cc1101BPresent) {
      startRX();
    } else {
      Serial.println(F("[SERIAL] No CC1101 detected!"));
    }
  }
  else if(cmd == "stop") {
    Serial.println(F("[SERIAL] Stopping current operation"));
    if(raw_rx == "1") stopRX();
    if(jammer_tx == "1") stopJammer();
    currentMenu = MENU_MAIN;
    updateDisplay();
  }
  else if(cmd == "menu" || cmd == "home") {
    currentMenu = MENU_MAIN;
    menuSelection = 0;
    menuOffset = 0;
    updateDisplay();
    Serial.println(F("[SERIAL] Returned to main menu"));
  }
  else if(cmd == "tesla") {
    if(cc1101APresent || cc1101BPresent) {
      Serial.println(F("[SERIAL] Sending Tesla signal"));
      sendTeslaSignal();
    } else {
      Serial.println(F("[SERIAL] No CC1101 detected!"));
    }
  }
  else if(cmd == "jammer") {
    if(cc1101APresent || cc1101BPresent) {
      Serial.println(F("[SERIAL] Starting jammer"));
      startJammer();
    } else {
      Serial.println(F("[SERIAL] No CC1101 detected!"));
    }
  }
  // Configuration commands with parameters
  else if(cmd.startsWith("freq ")) {
    float newFreq = cmd.substring(5).toFloat();
    if(newFreq >= 300 && newFreq <= 928) {
      frequency = newFreq;
      Serial.printf("[SERIAL] Frequency set to %.2f MHz\n", frequency);
      saveSettings();
    } else {
      Serial.println(F("[SERIAL] Invalid frequency (300-928 MHz)"));
    }
  }
  else if(cmd.startsWith("mod ")) {
    int newMod = cmd.substring(4).toInt();
    if(newMod >= 0 && newMod <= 4) {
      mod = newMod;
      Serial.printf("[SERIAL] Modulation set to %s\n", getModulationName(mod));
      saveSettings();
    } else {
      Serial.println(F("[SERIAL] Invalid modulation (0-4)"));
    }
  }
  else if(cmd.startsWith("led ")) {
    String ledCmd = cmd.substring(4);
    if(ledCmd == "off") {
      pixelMode = PIXEL_OFF;
      pixels.clear();
      pixels.show();
      Serial.println(F("[SERIAL] LEDs turned off"));
    }
    else if(ledCmd == "on") {
      pixelMode = PIXEL_IDLE;
      Serial.println(F("[SERIAL] LEDs turned on"));
    }
    else if(ledCmd.startsWith("bright ")) {
      int brightness = ledCmd.substring(7).toInt();
      if(brightness >= 0 && brightness <= 255) {
        ledBrightness = brightness;
        pixels.setBrightness(ledBrightness);
        Serial.printf("[SERIAL] LED brightness set to %d\n", ledBrightness);
        saveSettings();
      } else {
        Serial.println(F("[SERIAL] Invalid brightness (0-255)"));
      }
    }
  }
  else if(cmd == "") {
    // Empty command, ignore
  }
  else {
    Serial.print(F("[SERIAL] Unknown command: '"));
    Serial.print(cmd);
    Serial.println(F("'. Type 'help' for commands"));
  }
}

void printSerialHelp() {
  Serial.println(F("\n================================="));
  Serial.println(F("   TPP Badge Serial Commands"));
  Serial.println(F("================================="));
  Serial.println(F("Navigation:"));
  Serial.println(F("  up        - Navigate up"));
  Serial.println(F("  down      - Navigate down"));
  Serial.println(F("  left      - Go back (or cycle RX view)"));
  Serial.println(F("  right     - Go forward/select"));
  Serial.println(F("  select    - Select current item"));
  Serial.println(F("  back      - Go back"));
  Serial.println(F(""));
  Serial.println(F("Commands:"));
  Serial.println(F("  help      - Show this help"));
  Serial.println(F("  status    - Show current status"));
  Serial.println(F("  info      - Show system info"));
  Serial.println(F("  reboot    - Restart the badge"));
  Serial.println(F("  reset     - Factory reset"));
  Serial.println(F("  menu      - Return to main menu"));
  Serial.println(F("  rx        - Start RX mode"));
  Serial.println(F("  tesla     - Send Tesla signal"));
  Serial.println(F("  jammer    - Start jammer"));
  Serial.println(F("  stop      - Stop current operation"));
  Serial.println(F(""));
  Serial.println(F("Configuration:"));
  Serial.println(F("  freq <MHz>    - Set frequency"));
  Serial.println(F("                  Example: freq 433.92"));
  Serial.println(F("  mod <0-4>     - Set modulation"));
  Serial.println(F("                  0=2-FSK, 1=GFSK"));
  Serial.println(F("                  2=ASK/OOK, 3=4-FSK, 4=MSK"));
  Serial.println(F("  led off       - Turn LEDs off"));
  Serial.println(F("  led on        - Turn LEDs on"));
  Serial.println(F("  led bright <0-255> - Set brightness"));
  Serial.println(F("                  Example: led bright 128"));
  Serial.println(F("=================================\n"));
}

void printStatus() {
  Serial.println(F("\n=== Badge Status ==="));
  Serial.printf("Frequency: %.2f MHz\n", frequency);
  Serial.printf("Modulation: %s\n", getModulationName(mod));
  Serial.printf("Module: %c\n", activeModule == 0 ? 'A' : 'B');
  Serial.printf("RX Active: %s\n", raw_rx == "1" ? "Yes" : "No");
  Serial.printf("Jammer Active: %s\n", jammer_tx == "1" ? "Yes" : "No");
  Serial.printf("LED Mode: ");
  switch(pixelMode) {
    case PIXEL_OFF: Serial.println("OFF"); break;
    case PIXEL_IDLE: Serial.println("IDLE"); break;
    case PIXEL_RX: Serial.println("RX"); break;
    case PIXEL_TX: Serial.println("TX"); break;
    case PIXEL_JAMMER: Serial.println("JAMMER"); break;
    case PIXEL_TESLA: Serial.println("TESLA"); break;
    case PIXEL_MENU: Serial.println("MENU"); break;
    default: Serial.println("UNKNOWN"); break;
  }
  Serial.printf("LED Brightness: %d\n", ledBrightness);
  Serial.printf("Current Menu: ");
  switch(currentMenu) {
    case MENU_MAIN: Serial.println("MAIN"); break;
    case MENU_RX: Serial.println("RX"); break;
    case MENU_TX: Serial.println("TX"); break;
    case MENU_JAMMER: Serial.println("JAMMER"); break;
    case MENU_TESLA: Serial.println("TESLA"); break;
    case MENU_SETTINGS: Serial.println("SETTINGS"); break;
    case MENU_ABOUT: Serial.println("ABOUT"); break;
    case MENU_CREDITS: Serial.println("CREDITS"); break;
    case MENU_TX_BINARY: Serial.println("TX BINARY"); break;
    case MENU_WEATHER: Serial.println("WEATHER"); break;
    default: Serial.printf("%d\n", currentMenu); break;
  }
  Serial.println(F("==================\n"));
}

void printSystemInfo() {
  Serial.println(F("\n=== System Info ==="));
  Serial.printf("ESP32 Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Size: %d KB\n", ESP.getFlashChipSize() / 1024);
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Min Free Heap: %d bytes\n", ESP.getMinFreeHeap());
  Serial.printf("Sketch Size: %d bytes\n", ESP.getSketchSize());
  Serial.printf("Free Sketch Space: %d bytes\n", ESP.getFreeSketchSpace());
  
  Serial.println(F("\nHardware Status:"));
  Serial.printf("Display: %s\n", display.width() > 0 ? "OK" : "FAIL");
  Serial.printf("SD Card: %s\n", sdCardPresent ? "OK" : "NO");
  Serial.printf("CC1101-A: %s\n", cc1101APresent ? "OK" : "NO");
  Serial.printf("CC1101-B: %s\n", cc1101BPresent ? "OK" : "NO");
  Serial.printf("NeoPixels: %d\n", NEOPIXEL_COUNT);
  
  Serial.println(F("\nBoot Info:"));
  Serial.printf("Reset Reason: ");
  switch(esp_reset_reason()) {
    case ESP_RST_POWERON: Serial.println("Power-on"); break;
    case ESP_RST_SW: Serial.println("Software"); break;
    case ESP_RST_PANIC: Serial.println("Panic"); break;
    case ESP_RST_WDT: Serial.println("Watchdog"); break;
    case ESP_RST_BROWNOUT: Serial.println("Brownout"); break;
    default: Serial.println("Unknown"); break;
  }
  
  uint64_t chipid = ESP.getEfuseMac();
  Serial.printf("Chip ID: %04X%08X\n", (uint16_t)(chipid>>32), (uint32_t)chipid);
  Serial.println(F("==================\n"));
}

void handleMenuUp() {
  debugPrint("Menu Up", true, false, 0);
  
  if (currentMenu == MENU_LED_BRIGHTNESS) {
    if (ledBrightness < 255) {
      ledBrightness += 5;
      if (ledBrightness > 255) ledBrightness = 255;
      pixels.setBrightness(ledBrightness);
      saveSettings();
      Serial.printf("[Menu] LED Brightness: %d\n", ledBrightness);
    }
  } else if (currentMenu == MENU_FREQUENCY_SELECT) {
    if (frequencyIndex > 0) {
      frequencyIndex--;
      frequency = commonFrequencies[frequencyIndex];
      Serial.printf("[Menu] Frequency: %.2f MHz\n", frequency);
    }
  } else {
    if (menuSelection > 0) {
      menuSelection--;
      if (menuSelection < menuOffset) {
        menuOffset = menuSelection;
      }
      Serial.printf("[Menu] Selection: %d\n", menuSelection);
    }
  }
  updateDisplay();
}

void handleMenuDown() {
  debugPrint("Menu Down", true, false, 0);
  
  if (currentMenu == MENU_LED_BRIGHTNESS) {
    if (ledBrightness > 5) {
      ledBrightness -= 5;
      if (ledBrightness < 5) ledBrightness = 5;
      pixels.setBrightness(ledBrightness);
      saveSettings();
      Serial.printf("[Menu] LED Brightness: %d\n", ledBrightness);
    }
  } else if (currentMenu == MENU_FREQUENCY_SELECT) {
    if (frequencyIndex < numFrequencies - 1) {
      frequencyIndex++;
      frequency = commonFrequencies[frequencyIndex];
      Serial.printf("[Menu] Frequency: %.2f MHz\n", frequency);
    }
  } else if (currentMenu == MENU_CREDITS) {
    // Credits auto-scroll, no manual control
  } else {
    int maxItems = getMaxMenuItems();
    if (menuSelection < maxItems - 1) {
      menuSelection++;
      if (menuSelection >= menuOffset + maxMenuItems) {
        menuOffset = menuSelection - maxMenuItems + 1;
      }
      Serial.printf("[Menu] Selection: %d\n", menuSelection);
    }
  }
  updateDisplay();
}

void handleMenuBack() {
  Serial.println(F("[Menu] Back button pressed"));
  
  switch(currentMenu) {
    case MENU_MAIN:
      Serial.println(F("[Menu] Already at main menu"));
      break;
    case MENU_RX:
    case MENU_TX:
    case MENU_JAMMER:
    case MENU_TESLA:
    case MENU_SETTINGS:
    case MENU_ABOUT:
    case MENU_CREDITS:
      currentMenu = MENU_MAIN;
      menuSelection = 0;
      menuOffset = 0;
      creditScrollPos = 0;
      Serial.println(F("[Menu] Returning to main menu"));
      break;
    case MENU_RX_CONFIG:
      currentMenu = MENU_RX;
      menuSelection = 0;
      menuOffset = 0;
      Serial.println(F("[Menu] Returning to RX menu"));
      break;
    case MENU_TX_CONFIG:
    case MENU_TX_BINARY:
      currentMenu = MENU_TX;
      menuSelection = 0;
      menuOffset = 0;
      Serial.println(F("[Menu] Returning to TX menu"));
      break;
    case MENU_DISPLAY_SETTINGS:
      currentMenu = MENU_SETTINGS;
      menuSelection = 0;
      menuOffset = 0;
      Serial.println(F("[Menu] Returning to settings menu"));
      break;
    case MENU_LED_PATTERN:
    case MENU_LED_BRIGHTNESS:
      if (currentMenu == MENU_LED_PATTERN) {
        exitPatternMenu(false);  // Don't save, restore original
      }
      currentMenu = MENU_DISPLAY_SETTINGS;
      menuSelection = 0;
      menuOffset = 0;
      break;
    case MENU_FREQUENCY_SELECT:
      currentMenu = previousMenu;
      saveSettings();
      menuSelection = 0;
      menuOffset = 0;
      Serial.printf("[Menu] Frequency saved: %.2f MHz, returning to %d\n", frequency, previousMenu);
      break;
    case MENU_TESLA_CONFIG:
      currentMenu = MENU_TESLA;
      menuSelection = 0;
      menuOffset = 0;
      Serial.println(F("[Menu] Returning to Tesla menu"));
      break;
    default:
      currentMenu = MENU_MAIN;
      menuSelection = 0;
      menuOffset = 0;
      Serial.println(F("[Menu] Unknown menu, returning to main"));
      break;
  }
  pixelMode = PIXEL_MENU;
  updateDisplay();
}

void handleMenuSelect() {
  Serial.printf("[Menu] Select pressed in menu %d, selection %d\n", currentMenu, menuSelection);
  
  switch(currentMenu) {
    case MENU_MAIN:
      switch(menuSelection) {
        case 0: 
          currentMenu = MENU_RX;
          Serial.println(F("[Menu] Opening RX menu"));
          break;
        case 1: 
          currentMenu = MENU_TX;
          Serial.println(F("[Menu] Opening TX menu"));
          break;
        case 2: 
          currentMenu = MENU_JAMMER;
          Serial.println(F("[Menu] Opening Jammer menu"));
          break;
        case 3: 
          currentMenu = MENU_TESLA;
          Serial.println(F("[Menu] Opening Tesla menu"));
          break;
        case 4: 
          currentMenu = MENU_SETTINGS;
          Serial.println(F("[Menu] Opening Settings menu"));
          break;
        case 5: 
          currentMenu = MENU_ABOUT;
          Serial.println(F("[Menu] Opening About"));
          break;
        case 6:
          currentMenu = MENU_CREDITS;
          Serial.println(F("[Menu] Opening Credits"));
          break;
        case 7:
          Serial.println(F("[Menu] Opening Weather Station"));
          startWeatherStation();
          break;
      }
      menuSelection = 0;
      menuOffset = 0;
      break;
      
    case MENU_RX:
      switch(menuSelection) {
        case 0:  
          if(cc1101APresent || cc1101BPresent) {
            Serial.println(F("[Menu] Starting RX mode"));
            startRX();
            return; 
          } else {
            debugPrint("No CC1101!", true, true, 2000);
          }
          break;
        case 1: 
          currentMenu = MENU_RX_CONFIG;
          Serial.println(F("[Menu] Opening RX config"));
          break;
        case 2:  
          Serial.println(F("[Menu] Stopping RX"));
          stopRX();
          break;
      }
      break;
      
    case MENU_TX:
      switch(menuSelection) {
        case 0:
          Serial.println(F("[Menu] Starting simple TX"));
          startTX();  // This will run demo mode if TX_DEMO_MODE is true
          break;
        case 1:
          currentMenu = MENU_TX_BINARY;
          Serial.println(F("[Menu] Opening Binary TX"));
          // Don't call handleTXBinaryMenu here, it will be called in handleButtons()
          break;
        case 2:
          Serial.println(F("[Menu] TX Last RX"));
          sendLastRX();  // This will run demo mode if TX_DEMO_MODE is true
          break;
        case 3:
          Serial.println(F("[Menu] TX from file"));
          loadTXFromFile();  // This will run demo mode if TX_DEMO_MODE is true
          break;
        case 4:
          Serial.println(F("[Menu] TX from Flipper .sub file"));
          loadFlipperSubFile();
          break;
      }
      break;

    case MENU_JAMMER:
      switch(menuSelection) {
        case 0:
          if(cc1101APresent || cc1101BPresent) {
            Serial.println(F("[Menu] Starting Jammer"));
            startJammer();
            return;
          } else {
            debugPrint("No CC1101!", true, true, 2000);
          }
          break;
        case 1:
          Serial.println(F("[Menu] Stopping Jammer"));
          stopJammer();
          break;
      }
      break;
      
    case MENU_TESLA:
      switch(menuSelection) {
        case 0:
          if(cc1101APresent || cc1101BPresent) {
            Serial.println(F("[Menu] Sending Tesla signal"));
            sendTeslaSignal();
          } else {
            debugPrint("No CC1101!", true, true, 2000);
          }
          break;
        case 1:
          currentMenu = MENU_TESLA_CONFIG;
          Serial.println(F("[Menu] Opening Tesla config"));
          break;
      }
      break;
      
    case MENU_TESLA_CONFIG:
      // Toggle frequency
      if (teslaFrequency == 433.92) {
        teslaFrequency = 315.00;
      } else {
        teslaFrequency = 433.92;
      }
      saveSettings();
      Serial.printf("[Menu] Tesla frequency toggled to %.2f MHz\n", teslaFrequency);
      break;
      
    case MENU_SETTINGS:
      switch(menuSelection) {
        case 0: // Module selection
          if(cc1101APresent && cc1101BPresent) {
            activeModule = (activeModule == 0) ? 1 : 0;
            ELECHOUSE_cc1101.setModul(activeModule);
            saveSettings();
            Serial.printf("[Menu] Active module changed to %c\n", activeModule == 0 ? 'A' : 'B');
          } else {
            debugPrint("Only 1 module!", true, true, 2000);
          }
          break;
        case 1: // Frequency
          previousMenu = MENU_SETTINGS;
          currentMenu = MENU_FREQUENCY_SELECT;
          for(int i = 0; i < numFrequencies; i++) {
            if(abs(commonFrequencies[i] - frequency) < 0.01) {
              frequencyIndex = i;
              break;
            }
          }
          menuSelection = 0;
          menuOffset = 0;
          Serial.println(F("[Menu] Opening frequency selector"));
          break;
        case 2: // Modulation
          mod = (mod + 1) % 5;
          saveSettings();
          Serial.printf("[Menu] Modulation changed to %d\n", mod);
          break;
        case 3: // Display Settings
          currentMenu = MENU_DISPLAY_SETTINGS;
          menuSelection = 0;
          menuOffset = 0;
          Serial.println(F("[Menu] Opening display settings"));
          break;
        case 4: // Save Config
          saveSettings();
          Serial.println(F("[Menu] Settings saved"));
          break;
      }
      break;
      
    case MENU_RX_CONFIG:
      switch(menuSelection) {
        case 0: // Frequency
          previousMenu = MENU_RX_CONFIG;
          currentMenu = MENU_FREQUENCY_SELECT;
          for(int i = 0; i < numFrequencies; i++) {
            if(abs(commonFrequencies[i] - frequency) < 0.01) {
              frequencyIndex = i;
              break;
            }
          }
          menuSelection = 0;
          menuOffset = 0;
          Serial.println(F("[Menu] Opening frequency selector from RX config"));
          break;
      }
      break;
      
    case MENU_TX_CONFIG:
      switch(menuSelection) {
        case 0: // Frequency
          previousMenu = MENU_TX_CONFIG;
          currentMenu = MENU_FREQUENCY_SELECT;
          for(int i = 0; i < numFrequencies; i++) {
            if(abs(commonFrequencies[i] - frequency) < 0.01) {
              frequencyIndex = i;
              break;
            }
          }
          menuSelection = 0;
          menuOffset = 0;
          Serial.println(F("[Menu] Opening frequency selector from TX config"));
          break;
      }
      break;
      
    case MENU_DISPLAY_SETTINGS:
      switch(menuSelection) {
        case 0:
          currentMenu = MENU_LED_PATTERN;
          Serial.println(F("[Menu] Opening LED pattern selector"));
          break;
        case 1:
          currentMenu = MENU_LED_BRIGHTNESS;
          Serial.println(F("[Menu] Opening LED brightness control"));
          break;
      }
      menuSelection = 0;
      menuOffset = 0;
      break;
      
    case MENU_LED_PATTERN:
      exitPatternMenu(true);  // Save the previewed pattern
      saveSettings();
      currentMenu = MENU_DISPLAY_SETTINGS;
      menuSelection = 0;
      menuOffset = 0;
      break;
      
    case MENU_FREQUENCY_SELECT:
      frequency = commonFrequencies[frequencyIndex];
      saveSettings();
      currentMenu = previousMenu;
      menuSelection = 0;
      menuOffset = 0;
      Serial.printf("[Menu] Frequency selected: %.2f MHz\n", frequency);
      break;
  }
  
  updateDisplay();
}

int getMaxMenuItems() {
  switch(currentMenu) {
    case MENU_MAIN: return 8;
    case MENU_RX: return 4;
    case MENU_TX: return 5;  // Simple TX, Binary TX, TX Last RX, TX from File, Flipper .sub
    case MENU_JAMMER: return 3;
    case MENU_TESLA: return 2;
    case MENU_SETTINGS: return 5;
    case MENU_DISPLAY_SETTINGS: return 3;
    case MENU_LED_PATTERN: return 35;
    case MENU_RX_CONFIG: return 4;
    case MENU_TX_CONFIG: return 4;
    case MENU_TX_BINARY: return 4;  // Added for binary TX submenu
    default: return 1;
  }
}

// Handler for TX Binary submenu
void handleTXBinaryMenu() {
  static int binaryOption = 0;
  static unsigned long lastBinaryDraw = 0;
  
  // Only redraw if needed
  if(millis() - lastBinaryDraw > 100) {
    lastBinaryDraw = millis();
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0,0);
    display.println(F("=[ BINARY TX ]="));
    display.println(F(""));
    
    const char* options[] = {
      "Manual Input",
      "Test Pattern",
      "From File",
      "Back"
    };
    
    for(int i = 0; i < 4; i++) {
      if(i == binaryOption) {
        display.fillRect(0, 16 + i*10, 128, 10, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
      } else {
        display.setTextColor(SH110X_WHITE);
      }
      
      display.setCursor(2, 17 + i*10);
      display.print(options[i]);
    }
    
    display.setTextColor(SH110X_WHITE);
    display.display();
  }
  
  // Handle input
  if(millis() - lastButtonPress > buttonDebounce) {
    if(digitalRead(BTN_UP) == LOW) {
      lastButtonPress = millis();
      binaryOption = (binaryOption - 1 + 4) % 4;
      Serial.printf("[TX Binary] Option: %d\n", binaryOption);
    }
    else if(digitalRead(BTN_DOWN) == LOW) {
      lastButtonPress = millis();
      binaryOption = (binaryOption + 1) % 4;
      Serial.printf("[TX Binary] Option: %d\n", binaryOption);
    }
    else if(digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
      lastButtonPress = millis();
      
      switch(binaryOption) {
        case 0: // Manual Input
          Serial.println(F("[TX Binary] Manual Input selected"));
          sendBinaryData();
          currentMenu = MENU_TX;  // Return to TX menu after
          binaryOption = 0;  // Reset selection
          break;
          
        case 1: // Test Pattern
          Serial.println(F("[TX Binary] Test Pattern selected"));
          sendTestPattern();
          currentMenu = MENU_TX;  // Return to TX menu after
          binaryOption = 0;  // Reset selection
          break;
          
        case 2: // From File
          Serial.println(F("[TX Binary] From File selected"));
          loadTXFromFile();
          currentMenu = MENU_TX;  // Return to TX menu after
          binaryOption = 0;  // Reset selection
          break;
          
        case 3: // Back
          Serial.println(F("[TX Binary] Back selected"));
          currentMenu = MENU_TX;
          binaryOption = 0;  // Reset selection
          break;
      }
      
      menuSelection = 0;
      menuOffset = 0;
      updateDisplay();
    }
    else if(digitalRead(BTN_LEFT) == LOW) {
      lastButtonPress = millis();
      Serial.println(F("[TX Binary] Left/Back pressed"));
      currentMenu = MENU_TX;
      binaryOption = 0;  // Reset selection
      menuSelection = 0;
      menuOffset = 0;
      updateDisplay();
    }
  }
}