// FileOps.ino - SD card file operations and management

File logs;
String currentLogFile = "";
unsigned long logStartTime = 0;
int logEntryCount = 0;

// Forward declarations
void createDirectory(const char* path);
void startNewLogFile();
void listDirectory(const char* dirname, uint8_t levels);
void viewFiles(const char* path);
void fileOptions(const char* filename);
void viewFileContent(const char* filename);
void deleteFileConfirm(const char* filename);
void deleteFile(fs::FS &fs, const char* path);
void showCardInfo();

void initializeFileSystem() {
  if(!sdCardPresent) {
    Serial.println(F("[FILES] No SD card present"));
    return;
  }
  
  Serial.println(F("\n[FILES] ========== INITIALIZING FILE SYSTEM =========="));
  
  // Create directories if they don't exist
  createDirectory("/logs");
  createDirectory("/rx_data");
  createDirectory("/tx_data");
  createDirectory("/config");
  createDirectory("/subghz"); // drop Flipper Zero .sub files here
  
  // Start new log file
  startNewLogFile();
  
  // List files
  listDirectory("/", 0);
  
  Serial.println(F("[FILES] File system ready"));
}

void createDirectory(const char* path) {
  if(!SD_MMC.exists(path)) {
    if(SD_MMC.mkdir(path)) {
      Serial.printf("[FILES] Created directory: %s\n", path);
    } else {
      Serial.printf("[FILES] Failed to create directory: %s\n", path);
    }
  } else {
    Serial.printf("[FILES] Directory exists: %s\n", path);
  }
}

void startNewLogFile() {
  char filename[32];
  sprintf(filename, "/logs/log_%lu.txt", millis());
  currentLogFile = String(filename);
  logStartTime = millis();
  logEntryCount = 0;
  
  File logFile = SD_MMC.open(filename, FILE_WRITE);
  if(logFile) {
    logFile.println(F("=== TPP Badge Log ==="));
    logFile.printf("Start time: %lu\n", logStartTime);
    logFile.printf("Frequency: %.2f MHz\n", frequency);
    logFile.printf("Module: %c\n", activeModule == 0 ? 'A' : 'B');
    logFile.println(F("====================="));
    logFile.close();
    
    Serial.printf("[FILES] Started new log: %s\n", filename);
  } else {
    Serial.printf("[FILES] Failed to create log: %s\n", filename);
  }
}

// Remove the default parameter from the implementation
void logEvent(const char* event, const char* details) {
  if(!sdCardPresent || currentLogFile.length() == 0) return;
  
  File logFile = SD_MMC.open(currentLogFile, FILE_APPEND);
  if(logFile) {
    unsigned long timestamp = millis() - logStartTime;
    logFile.printf("[%lu] %s", timestamp, event);
    if(details && strlen(details) > 0) {
      logFile.printf(" - %s", details);
    }
    logFile.println();
    logFile.close();
    
    logEntryCount++;
    
    // Start new log file every 1000 entries
    if(logEntryCount >= 1000) {
      startNewLogFile();
    }
  }
}

void saveRXData(const char* filename) {
  if(!sdCardPresent || samplecount == 0) {
    Serial.println(F("[FILES] Cannot save RX data"));
    return;
  }
  
  Serial.printf("[FILES] Saving RX data to %s\n", filename);
  
  File dataFile = SD_MMC.open(filename, FILE_WRITE);
  if(!dataFile) {
    Serial.println(F("[FILES] Failed to create RX file"));
    debugPrint("Save failed!", true, true, 2000);
    return;
  }
  
  // Write header
  dataFile.println(F("# TPP Badge RX Data"));
  dataFile.printf("# Timestamp: %lu\n", millis());
  dataFile.printf("Frequency: %.2f\n", frequency);
  dataFile.printf("Modulation: %d\n", mod);
  dataFile.printf("Bandwidth: %.2f\n", setrxbw);
  dataFile.printf("DataRate: %d\n", datarate);
  dataFile.printf("RSSI: %d\n", ELECHOUSE_cc1101.getRssi());
  dataFile.printf("Count: %d\n", samplecount);
  dataFile.print("Data: ");
  
  // Write timing data
  for(int i = 0; i < samplecount; i++) {
    dataFile.print(sample[i]);
    if(i < samplecount - 1) dataFile.print(",");
  }
  dataFile.println();
  
  // Write binary interpretation
  dataFile.print("Binary: ");
  for(int i = 0; i < min(samplecount, 100); i++) {
    dataFile.print(sample[i] > 1000 ? "1" : "0");
  }
  dataFile.println();
  
  dataFile.close();
  
  Serial.printf("[FILES] Saved %d samples\n", samplecount);
  logEvent("RX_SAVE", filename);
  
  // Show success
  display.clearDisplay();
  display.setCursor(0, 20);
  display.println(F("RX Data Saved!"));
  display.println(filename);
  display.printf("%d samples\n", samplecount);
  display.display();
  delay(1500);
}

void listDirectory(const char* dirname, uint8_t levels) {
  Serial.printf("[FILES] Listing directory: %s\n", dirname);
  
  File root = SD_MMC.open(dirname);
  if(!root) {
    Serial.println(F("[FILES] Failed to open directory"));
    return;
  }
  
  if(!root.isDirectory()) {
    Serial.println(F("[FILES] Not a directory"));
    return;
  }
  
  File file = root.openNextFile();
  while(file) {
    if(file.isDirectory()) {
      Serial.print(F("  DIR : "));
      Serial.println(file.name());
      if(levels) {
        listDirectory(file.name(), levels - 1);
      }
    } else {
      Serial.print(F("  FILE: "));
      Serial.print(file.name());
      Serial.print(F("  SIZE: "));
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void displayFileMenu() {
  Serial.println(F("\n[FILES] ========== FILE MENU =========="));

  // Wait for the button that opened this menu to be released first - otherwise
  // it's still LOW on the very first check below and instantly selects option 0
  while(digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
    delay(10);
  }
  delay(150);

  bool inMenu = true;
  int menuOption = 0;
  const int numOptions = 5;
  
  while(inMenu) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ FILES ]="));
    display.println(F(""));
    
    const char* options[] = {
      "View RX Files",
      "View TX Files", 
      "View Logs",
      "Card Info",
      "Back"
    };
    
    for(int i = 0; i < numOptions; i++) {
      if(i == menuOption) {
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
    
    // Handle input
    if(millis() - lastButtonPress > buttonDebounce) {
      if(digitalRead(BTN_UP) == LOW) {
        lastButtonPress = millis();
        menuOption = (menuOption - 1 + numOptions) % numOptions;
      }
      else if(digitalRead(BTN_DOWN) == LOW) {
        lastButtonPress = millis();
        menuOption = (menuOption + 1) % numOptions;
      }
      else if(digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
        lastButtonPress = millis();
        
        switch(menuOption) {
          case 0:
            viewFiles("/rx_data");
            break;
          case 1:
            viewFiles("/tx_data");
            break;
          case 2:
            viewFiles("/logs");
            break;
          case 3:
            showCardInfo();
            break;
          case 4:
            inMenu = false;
            break;
        }
      }
      else if(digitalRead(BTN_LEFT) == LOW) {
        lastButtonPress = millis();
        inMenu = false;
      }
    }
    
    delay(10);
  }
}

void viewFiles(const char* path) {
  Serial.printf("[FILES] Viewing files in %s\n", path);
  
  File root = SD_MMC.open(path);
  if(!root || !root.isDirectory()) {
    debugPrint("Cannot open dir!", true, true, 2000);
    return;
  }
  
  // Count files
  int fileCount = 0;
  String fileNames[50];
  
  File file = root.openNextFile();
  while(file && fileCount < 50) {
    if(!file.isDirectory()) {
      fileNames[fileCount] = String(file.name());
      fileCount++;
    }
    file = root.openNextFile();
  }
  root.close();
  
  if(fileCount == 0) {
    debugPrint("No files found!", true, true, 2000);
    return;
  }
  
  // Display file list
  int selectedFile = 0;
  int fileOffset = 0;
  bool viewing = true;
  
  while(viewing) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.print(F("=[ "));
    display.print(path);
    display.println(F(" ]="));
    display.setCursor(90, 0);
    display.printf("%d/%d", selectedFile + 1, fileCount);
    display.println(F(""));
    
    // Show files
    int maxVisible = 5;
    for(int i = 0; i < maxVisible && (fileOffset + i) < fileCount; i++) {
      if(fileOffset + i == selectedFile) {
        display.fillRect(0, 16 + i*10, 128, 10, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
      } else {
        display.setTextColor(SH110X_WHITE);
      }
      
      display.setCursor(2, 17 + i*10);
      String shortName = fileNames[fileOffset + i];
      if(shortName.length() > 20) {
        shortName = shortName.substring(0, 17) + "...";
      }
      display.print(shortName);
    }
    
    display.setTextColor(SH110X_WHITE);
    display.display();
    
    // Handle input
    if(millis() - lastButtonPress > buttonDebounce) {
      if(digitalRead(BTN_UP) == LOW) {
        lastButtonPress = millis();
        if(selectedFile > 0) {
          selectedFile--;
          if(selectedFile < fileOffset) {
            fileOffset = selectedFile;
          }
        }
      }
      else if(digitalRead(BTN_DOWN) == LOW) {
        lastButtonPress = millis();
        if(selectedFile < fileCount - 1) {
          selectedFile++;
          if(selectedFile >= fileOffset + maxVisible) {
            fileOffset = selectedFile - maxVisible + 1;
          }
        }
      }
      else if(digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
        lastButtonPress = millis();
        String fullPath = String(path) + "/" + fileNames[selectedFile];
        fileOptions(fullPath.c_str());
      }
      else if(digitalRead(BTN_LEFT) == LOW) {
        lastButtonPress = millis();
        viewing = false;
      }
    }
    
    delay(10);
  }
}

void fileOptions(const char* filename) {
  Serial.printf("[FILES] File options for %s\n", filename);
  
  bool inOptions = true;
  int option = 0;
  const int numOptions = 4;
  
  while(inOptions) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ FILE OPS ]="));
    display.println(F(""));
    
    // Show filename (truncated)
    String shortName = String(filename);
    int lastSlash = shortName.lastIndexOf('/');
    if(lastSlash >= 0) {
      shortName = shortName.substring(lastSlash + 1);
    }
    display.println(shortName);
    display.println(F(""));
    
    const char* options[] = {
      "View",
      "Transmit",
      "Delete",
      "Back"
    };
    
    for(int i = 0; i < numOptions; i++) {
      if(i == option) {
        display.fillRect(0, 40 + i*10, 128, 10, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
      } else {
        display.setTextColor(SH110X_WHITE);
      }
      
      display.setCursor(2, 41 + i*10);
      display.print(options[i]);
    }
    
    display.setTextColor(SH110X_WHITE);
    display.display();
    
    // Handle input
    if(millis() - lastButtonPress > buttonDebounce) {
      if(digitalRead(BTN_UP) == LOW) {
        lastButtonPress = millis();
        option = (option - 1 + numOptions) % numOptions;
      }
      else if(digitalRead(BTN_DOWN) == LOW) {
        lastButtonPress = millis();
        option = (option + 1) % numOptions;
      }
      else if(digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
        lastButtonPress = millis();
        
        switch(option) {
          case 0:
            viewFileContent(filename);
            break;
          case 1:
            if(String(filename).indexOf("rx_") >= 0) {
              loadAndTransmitFile(filename);
            } else {
              debugPrint("Not RX file!", true, true, 2000);
            }
            break;
          case 2:
            deleteFileConfirm(filename);
            inOptions = false;
            break;
          case 3:
            inOptions = false;
            break;
        }
      }
      else if(digitalRead(BTN_LEFT) == LOW) {
        lastButtonPress = millis();
        inOptions = false;
      }
    }
    
    delay(10);
  }
}

void viewFileContent(const char* filename) {
  Serial.printf("[FILES] Viewing content of %s\n", filename);
  
  File file = SD_MMC.open(filename, FILE_READ);
  if(!file) {
    debugPrint("Cannot open file!", true, true, 2000);
    return;
  }
  
  // For small files, show content
  if(file.size() < 1024) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    
    int line = 0;
    while(file.available() && line < 8) {
      String textLine = file.readStringUntil('\n');
      if(textLine.length() > 21) {
        textLine = textLine.substring(0, 21);
      }
      display.println(textLine);
      line++;
    }
    
    display.display();
    file.close();
    
    // Wait for button
    while(digitalRead(BTN_LEFT) == HIGH && 
          digitalRead(BTN_SELECT) == HIGH) {
      delay(10);
    }
    delay(buttonDebounce);
  } else {
    // For large files, show info
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("=[ FILE INFO ]="));
    display.println(F(""));
    display.printf("Size: %d bytes\n", file.size());
    display.println(F(""));
    display.println(F("File too large"));
    display.println(F("to display"));
    display.display();
    file.close();
    delay(3000);
  }
}

void deleteFileConfirm(const char* filename) {
  Serial.printf("[FILES] Delete confirmation for %s\n", filename);
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ DELETE? ]="));
  display.println(F(""));
  
  String shortName = String(filename);
  int lastSlash = shortName.lastIndexOf('/');
  if(lastSlash >= 0) {
    shortName = shortName.substring(lastSlash + 1);
  }
  display.println(shortName);
  display.println(F(""));
  display.println(F("RIGHT = Delete"));
  display.println(F("LEFT = Cancel"));
  display.display();
  
  bool waiting = true;
  unsigned long startTime = millis();
  
  while(waiting && (millis() - startTime < 10000)) {
    if(digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
      deleteFile(SD_MMC, filename);
      waiting = false;
    }
    else if(digitalRead(BTN_LEFT) == LOW) {
      debugPrint("Cancelled", true, true, 1000);
      waiting = false;
    }
    delay(10);
  }
}

void deleteFile(fs::FS &fs, const char* path) {
  Serial.printf("[FILES] Deleting %s\n", path);
  
  if(fs.remove(path)) {
    Serial.println(F("[FILES] File deleted"));
    logEvent("FILE_DELETE", path);
    debugPrint("File deleted!", true, true, 1500);
  } else {
    Serial.println(F("[FILES] Delete failed"));
    debugPrint("Delete failed!", true, true, 2000);
  }
}

void showCardInfo() {
  if(!sdCardPresent) {
    debugPrint("No SD card!", true, true, 2000);
    return;
  }
  
  Serial.println(F("\n[FILES] ========== SD CARD INFO =========="));
  
  uint8_t cardType = SD_MMC.cardType();
  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  uint64_t usedBytes = SD_MMC.usedBytes() / (1024 * 1024);
  uint64_t totalBytes = SD_MMC.totalBytes() / (1024 * 1024);
  
  const char* typeStr = "Unknown";
  if(cardType == CARD_MMC) typeStr = "MMC";
  else if(cardType == CARD_SD) typeStr = "SDSC";
  else if(cardType == CARD_SDHC) typeStr = "SDHC";
  
  Serial.printf("Card Type: %s\n", typeStr);
  Serial.printf("Card Size: %lluMB\n", cardSize);
  Serial.printf("Used: %lluMB\n", usedBytes);
  Serial.printf("Total: %lluMB\n", totalBytes);
  Serial.printf("Free: %lluMB\n", totalBytes - usedBytes);
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("=[ SD INFO ]="));
  display.println(F(""));
  display.printf("Type: %s\n", typeStr);
  display.printf("Size: %lluMB\n", cardSize);
  display.printf("Used: %lluMB\n", usedBytes);
  display.printf("Free: %lluMB\n", totalBytes - usedBytes);
  display.println(F(""));
  display.println(F("Press any button"));
  display.display();
  
  // Wait for button
  while(digitalRead(BTN_UP) == HIGH && 
        digitalRead(BTN_DOWN) == HIGH &&
        digitalRead(BTN_LEFT) == HIGH && 
        digitalRead(BTN_RIGHT) == HIGH &&
        digitalRead(BTN_SELECT) == HIGH) {
    delay(10);
  }
  delay(buttonDebounce);
}

// Utility functions for main code
void appendFile(fs::FS &fs, const char* path, const char* message, String messagestring) {
  logs = fs.open(path, FILE_APPEND);
  if(!logs) {
    Serial.println(F("[FILES] Failed to open file for append"));
    return;
  }
  if(message) logs.print(message);
  if(messagestring.length() > 0) logs.print(messagestring);
  logs.close();
}

void appendFileLong(fs::FS &fs, const char* path, unsigned long messagechar) {
  logs = fs.open(path, FILE_APPEND);
  if(!logs) {
    Serial.println(F("[FILES] Failed to open file for append"));
    return;
  }
  logs.print(messagechar);
  logs.close();
}