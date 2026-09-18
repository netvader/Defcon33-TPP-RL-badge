// Display.ino - All display drawing and UI functions

void updateDisplay() {
  display.clearDisplay();
  
  Serial.printf("[Display] Updating display for menu %d\n", currentMenu);
  
  switch(currentMenu) {
    case MENU_MAIN:
      drawMainMenu();
      break;
    case MENU_RX:
      drawRXMenu();
      break;
    case MENU_TX:
      drawTXMenu();
      break;
    case MENU_JAMMER:
      drawJammerMenu();
      break;
    case MENU_TESLA:
      drawTeslaMenu();
      break;
    case MENU_SETTINGS:
      drawSettingsMenu();
      break;
    case MENU_ABOUT:
      drawAbout();
      break;
    case MENU_CREDITS:
      drawCredits();
      break;
    case MENU_WEATHER:
      drawWeatherMenu();
      break;
    case MENU_FULLDUPLEX:
      drawFullDuplexMenu();
      break;
    case MENU_RX_CONFIG:
      drawRXConfig();
      break;
    case MENU_TX_CONFIG:
      drawTXConfig();
      break;
    case MENU_DISPLAY_SETTINGS:
      drawDisplaySettings();
      break;
    case MENU_LED_PATTERN:
      drawLEDPatternMenu();
      break;
    case MENU_LED_BRIGHTNESS:
      drawBrightnessMenu();
      break;
    case MENU_FREQUENCY_SELECT:
      drawFrequencySelect();
      break;
    case MENU_TESLA_CONFIG:
      drawTeslaConfig();
      break;
  }
  
  display.display();
}

void drawMainMenu() {
  Serial.println(F("[Display] Drawing main menu"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ TPP BADGE ]="));
  
  // Show hardware status on main menu
  display.setCursor(90, 0);
  display.print(F("HW:"));
  if(cc1101APresent || cc1101BPresent) display.print(F("R"));
  if(sdCardPresent) display.print(F("S"));
  
  display.setCursor(0, 8);
  display.println(F(""));
  
  const char* menuItems[] = {
    "RX Mode",
    "TX Mode",
    "Jammer",
    "Tesla",
    "Settings",
    "Weather",
    "Full Duplex",
    "About",
    "Credits"
  };

  int itemCount = 9;
  drawMenuItems(menuItems, itemCount);
}

void drawRXMenu() {
  Serial.println(F("[Display] Drawing RX menu"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ RX MODE ]="));
  display.println(F(""));
  
  const char* menuItems[] = {
    "Start RX",     
    "Configure",    
    "Stop RX",
    "View Logs"
  };
  
  int itemCount = 4;
  drawMenuItems(menuItems, itemCount);
  
  if(raw_rx == "1") {
    display.setCursor(80, 0);
    display.print(F("[RX]"));
  }
}

void drawTXMenu() {
  Serial.println(F("[Display] Drawing TX menu"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ TX MODE ]="));
  display.println(F(""));
  
  const char* menuItems[] = {
    "Configure",
    "Binary TX",
    "TX from File",
    "Last RX",
    "Flipper .sub"
  };

  int itemCount = 5;
  drawMenuItems(menuItems, itemCount);
}

void drawJammerMenu() {
  Serial.println(F("[Display] Drawing Jammer menu"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ JAMMER ]="));
  display.println(F(""));
  
  const char* menuItems[] = {
    "Start Jammer",
    "Stop Jammer",
    "Configure"
  };
  
  int itemCount = 3;
  drawMenuItems(menuItems, itemCount);
  
  if(jammer_tx == "1") {
    display.setCursor(80, 0);
    display.print(F("[JAM]"));
  }
}

void drawTeslaMenu() {
  Serial.println(F("[Display] Drawing Tesla menu"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ TESLA ]="));
  display.println(F(""));
  
  char freqStr[20];
  sprintf(freqStr, "Freq: %.2f MHz", teslaFrequency);
  
  const char* menuItems[] = {
    "Send Signal",
    freqStr
  };
  
  int itemCount = 2;
  drawMenuItems(menuItems, itemCount);
  
  display.setCursor(0, 55);
  display.print(F("SELECT toggles freq"));
}

void drawTeslaConfig() {
  Serial.println(F("[Display] Drawing Tesla config"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ TESLA FREQ ]="));
  display.println(F(""));
  
  display.setCursor(10, 20);
  display.print(F("Current: "));
  display.print(teslaFrequency, 2);
  display.println(F(" MHz"));
  
  display.setCursor(10, 35);
  display.println(F("Press SELECT to"));
  display.setCursor(10, 45);
  display.println(F("toggle 315/433.92"));
}

void drawSettingsMenu() {
  Serial.println(F("[Display] Drawing Settings menu"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ SETTINGS ]="));
  display.println(F(""));
  
  char moduleStr[20];
  if(!cc1101APresent && !cc1101BPresent) {
    sprintf(moduleStr, "Module: NONE");
  } else if(cc1101APresent && !cc1101BPresent) {
    sprintf(moduleStr, "Module: A only");
  } else if(!cc1101APresent && cc1101BPresent) {
    sprintf(moduleStr, "Module: B only");
  } else {
    sprintf(moduleStr, "Module: %c", activeModule == 0 ? 'A' : 'B');
  }
  
  char freqStr[20];
  sprintf(freqStr, "Freq: %.2f", frequency);
  
  const char* modStr;
  switch(mod) {
    case 0: modStr = "Mod: 2-FSK"; break;
    case 1: modStr = "Mod: GFSK"; break;
    case 2: modStr = "Mod: ASK/OOK"; break;
    case 3: modStr = "Mod: 4-FSK"; break;
    case 4: modStr = "Mod: MSK"; break;
    default: modStr = "Mod: Unknown"; break;
  }
  
  const char* menuItems[] = {
    moduleStr,
    freqStr,
    modStr,
    "Display Settings",
    "Save Config"
  };
  
  int itemCount = 5;
  drawMenuItems(menuItems, itemCount);
}

void drawDisplaySettings() {
  Serial.println(F("[Display] Drawing Display Settings"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ DISPLAY ]="));
  display.println(F(""));
  
  const char* menuItems[] = {
    "LED Pattern",
    "LED Brightness",
    "Back"
  };
  
  int itemCount = 3;
  drawMenuItems(menuItems, itemCount);
}

void drawLEDPatternMenu() {
  Serial.printf("[Display] Drawing LED Pattern menu, selection: %d\n", menuSelection);
  
  if (!inPatternPreview) {
    savedPattern = idlePattern;
    inPatternPreview = true;
  }
  
  previewPattern = (IdlePattern)menuSelection;
  idlePattern = previewPattern;
  pixelMode = PIXEL_IDLE;
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ LED PATTERN ]="));
  display.println(F(""));
  
  const char* menuItems[] = {
    "Breathe",
    "Rainbow",
    "Scanner",
    "Sparkle",
    "Fire",
    "Ocean", 
    "Matrix",
    "SuperGay",
    "Red",
    "Green",
    "Blue",
    "White",
    "Magenta",
    "Cyan",
    "Yellow",
    "Orange",
    "Neon Pulse",
    "Plasma",
    "Meteor Rain",
    "Twinkle",
    "Vaporwave",
    "Police",
    "Pirate Ship",
    "Theater Chase",
    "Running Lights",
    "Bouncing Balls",
    "Heartbeat",
    "Strobe",
    "Cylon",
    "Snow Sparkle",
    "Pacifica",
    "Lava Lamp",
    "Lightning",
    "Confetti",
    "Off"
  };
  
  int itemCount = 35;
  drawMenuItems(menuItems, itemCount);
  
  display.setCursor(0, 56);
  display.setTextSize(1);
  display.print(F("Now: "));
  if(menuSelection < itemCount) {
    display.print(menuItems[menuSelection]);
  }
}

// Add this function to handle exiting the pattern menu
void exitPatternMenu(bool save) {
  if (inPatternPreview) {
    if (save) {
      // Keep the previewed pattern
      Serial.printf("[Display] Saving pattern: %d\n", previewPattern);
    } else {
      // Restore the original pattern
      idlePattern = savedPattern;
      Serial.printf("[Display] Restoring pattern: %d\n", savedPattern);
    }
    inPatternPreview = false;
  }
}

void drawBrightnessMenu() {
  Serial.printf("[Display] Drawing brightness menu, current: %d\n", ledBrightness);
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ BRIGHTNESS ]="));
  display.println(F(""));
  
  display.setCursor(10, 20);
  display.print(F("Level: "));
  display.print(ledBrightness);
  display.println(F("/255"));
  
  // Draw brightness bar
  int barWidth = map(ledBrightness, 0, 255, 0, 100);
  display.drawRect(10, 35, 102, 10, SH110X_WHITE);
  display.fillRect(11, 36, barWidth, 8, SH110X_WHITE);
  
  display.setCursor(10, 50);
  display.print(F("UP/DOWN to adjust"));
}

void drawFrequencySelect() {
  Serial.printf("[Display] Drawing frequency select, current: %.2f MHz, index: %d\n", 
                frequency, frequencyIndex);
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ FREQUENCY ]="));
  display.println(F(""));
  
  // Show current frequency
  display.setCursor(0, 16);
  display.print(F("Current: "));
  display.print(frequency, 2);
  display.println(F(" MHz"));
  
  // Show selected frequency
  display.setCursor(0, 28);
  display.print(F("> "));
  display.print(commonFrequencies[frequencyIndex], 2);
  display.println(F(" MHz"));
  
  // Show band info
  display.setCursor(0, 45);
  if(commonFrequencies[frequencyIndex] < 350) {
    display.print(F("Band: 300 MHz"));
  } else if(commonFrequencies[frequencyIndex] < 500) {
    display.print(F("Band: 433 MHz"));
  } else if(commonFrequencies[frequencyIndex] < 900) {
    display.print(F("Band: 868 MHz"));
  } else {
    display.print(F("Band: 915 MHz"));
  }
  
  display.setCursor(0, 55);
  display.print(F("UP/DOWN to change"));
}

void drawRXConfig() {
  Serial.println(F("[Display] Drawing RX config"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ RX CONFIG ]="));
  display.println(F(""));
  
  char buffer[20];
  sprintf(buffer, "Freq: %.2f MHz", frequency);
  
  const char* menuItems[] = {
    buffer,
    "BW: 812.50 kHz",
    "Rate: 99.97 kBaud",
    "Back"
  };
  
  int itemCount = 4;
  drawMenuItems(menuItems, itemCount);
}

void drawTXConfig() {
  Serial.println(F("[Display] Drawing TX config"));
  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("=[ TX CONFIG ]="));
  display.println(F(""));
  
  char buffer[20];
  sprintf(buffer, "Freq: %.2f MHz", frequency);
  
  const char* menuItems[] = {
    buffer,
    "Dev: 47.60 kHz",
    "Rate: 99.97 kBaud",
    "Back"
  };
  
  int itemCount = 4;
  drawMenuItems(menuItems, itemCount);
}

void drawMenuItems(const char* items[], int itemCount) {
  int startItem = menuOffset;
  int endItem = min(startItem + maxMenuItems, itemCount);
  
  Serial.printf("[Display] Drawing menu items %d to %d, selection=%d\n", 
                startItem, endItem-1, menuSelection);
  
  for (int i = startItem; i < endItem; i++) {
    int yPos = 16 + ((i - startItem) * 10);
    
    if (i == menuSelection) {
      display.fillRect(0, yPos - 1, 128, 10, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }
    
    display.setCursor(2, yPos);
    display.print(items[i]);
  }
  
  display.setTextColor(SH110X_WHITE);
  
  // Show scroll indicators if needed
  if (menuOffset > 0) {
    display.setCursor(120, 16);
    display.print(F("^"));
  }
  if (endItem < itemCount) {
    display.setCursor(120, 54);
    display.print(F("v"));
  }
}