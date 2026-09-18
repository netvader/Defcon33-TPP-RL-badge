// SubFile.ino - Load and transmit Flipper Zero SubGHz .sub files
//
// .sub files are plain-text "key: value" files, e.g.:
//   Filetype: Flipper SubGhz Key File
//   Version: 1
//   Frequency: 433920000
//   Preset: FuriHalSubGhzPresetOok650Async
//   Protocol: RAW
//   RAW_Data: 328 -1058 274 -220 ...
// or, for a fixed-code remote:
//   Protocol: Princeton
//   Bit: 24
//   Key: 00 00 00 00 00 F1 E1 E1
//   TE: 403
//
// RAW_Data pulses are signed - positive = mark (high), negative = space (low), value
// = microseconds. That maps directly onto sendRawData()'s existing alternating
// HIGH/LOW format, so RAW playback just needs parsing, no protocol decoding.
//
// "Princeton" (PT2262-style fixed-code remotes - garage doors, driveway sensors, etc.)
// is the one keyed protocol implemented here, since it's simple and widely documented:
// each bit is one TE-scaled short/long pulse pair, followed by a sync gap. Any other
// Protocol: value is reported as unsupported rather than guessed at.
//
// IMPORTANT: the Princeton bit/sync timing ratios below are from public protocol
// write-ups (e.g. rc-switch/rtl_433), not verified against a real remote - I had no
// hardware to test this against. RAW playback needs no such protocol knowledge, so
// it should be the more reliable path for anything captured directly off a Flipper.

#define SUBGHZ_DIR "/subghz"

bool sendPrincetonFromKey(String keyHex, int bitCount, int te) {
  if(bitCount <= 0 || bitCount > 32 || te <= 0) {
    Serial.println(F("[TX] Invalid Princeton parameters in .sub file"));
    return false;
  }

  uint32_t code = 0;
  int idx = 0;
  while(idx < (int)keyHex.length()) {
    while(idx < (int)keyHex.length() && keyHex[idx] == ' ') idx++;
    int start = idx;
    while(idx < (int)keyHex.length() && keyHex[idx] != ' ') idx++;
    if(idx > start) {
      code = (code << 8) | strtoul(keyHex.substring(start, idx).c_str(), NULL, 16);
    }
  }

  data_count = 0;
  for(int b = bitCount - 1; b >= 0 && data_count < 1996; b--) {
    bool bitVal = (code >> b) & 0x01;
    if(bitVal) {
      data_to_send[data_count++] = te * 3; // long high  = 1
      data_to_send[data_count++] = te;     // short low
    } else {
      data_to_send[data_count++] = te;     // short high = 0
      data_to_send[data_count++] = te * 3; // long low
    }
  }
  // Sync gap between repeats
  data_to_send[data_count++] = te;
  data_to_send[data_count++] = te * 31;

  Serial.printf("[TX] Princeton: %d bits, TE=%dus, code=0x%08lX\n", bitCount, te, (unsigned long)code);
  sendRawData(data_to_send, data_count, 5); // fixed-code remotes are usually repeated several times
  return true;
}

bool parseAndSendSubFile(String filename) {
  Serial.printf("[TX] Loading Flipper .sub file: %s\n", filename.c_str());

  File f = SD_MMC.open(filename, FILE_READ);
  if(!f) {
    debugPrint("File open failed!", true, true, 2000);
    return false;
  }

  String protocol = "";
  long fileFreqHz = 0;
  String preset = "";
  int bitCount = 0;
  String keyHex = "";
  int te = 0;

  data_count = 0;

  while(f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    if(line.startsWith("Frequency:")) {
      fileFreqHz = line.substring(10).toInt();
    } else if(line.startsWith("Preset:")) {
      preset = line.substring(7);
      preset.trim();
    } else if(line.startsWith("Protocol:")) {
      protocol = line.substring(9);
      protocol.trim();
    } else if(line.startsWith("Bit:")) {
      bitCount = line.substring(4).toInt();
    } else if(line.startsWith("Key:")) {
      keyHex = line.substring(4);
      keyHex.trim();
    } else if(line.startsWith("TE:")) {
      te = line.substring(3).toInt();
    } else if(line.startsWith("RAW_Data:")) {
      // A .sub file can have many RAW_Data lines - keep appending until the buffer is full
      String vals = line.substring(9);
      int idx = 0;
      while(idx < (int)vals.length() && data_count < 2000) {
        while(idx < (int)vals.length() && vals[idx] == ' ') idx++;
        int start = idx;
        while(idx < (int)vals.length() && vals[idx] != ' ') idx++;
        if(idx > start) {
          long v = vals.substring(start, idx).toInt();
          data_to_send[data_count++] = abs(v);
        }
      }
    }
  }
  f.close();

  if(fileFreqHz > 0) {
    frequency = fileFreqHz / 1000000.0;
  }
  if(preset.indexOf("Ook") >= 0) mod = 2;
  else if(preset.indexOf("GFSK") >= 0) mod = 1;
  else if(preset.indexOf("MSK") >= 0) mod = 4;
  else if(preset.indexOf("FSK") >= 0) mod = 0; // 2FSK presets, checked after the more specific FSK variants above

  Serial.printf("[TX] .sub file: protocol=%s, %.2f MHz, mod=%d\n", protocol.c_str(), frequency, mod);

  if(protocol == "RAW") {
    if(data_count == 0) {
      debugPrint("No RAW data!", true, true, 2000);
      return false;
    }
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("=[ FLIPPER RAW ]="));
    display.printf("%d pulses\n", data_count);
    display.printf("%.2f MHz\n", frequency);
    display.display();
    Serial.printf("[TX] Flipper .sub RAW: %d pulses\n", data_count);
    sendRawData(data_to_send, data_count, 3);
    return true;
  }

  if(protocol == "Princeton") {
    return sendPrincetonFromKey(keyHex, bitCount, te);
  }

  Serial.printf("[TX] Unsupported .sub protocol: %s\n", protocol.c_str());
  display.clearDisplay();
  display.setCursor(0, 20);
  display.println(F("Unsupported"));
  display.println(F("protocol:"));
  display.println(protocol);
  display.display();
  delay(2000);
  return false;
}

void loadFlipperSubFile() {
  if(TX_DEMO_MODE) {
    Serial.println(F("[TX] Demo mode - Flipper .sub"));
    runDemoTX("FLIPPER SUB");
    return;
  }

  if(!sdCardPresent) {
    debugPrint("No SD card!", true, true, 2000);
    return;
  }

  Serial.println(F("\n[TX] ===== FLIPPER .SUB TX ====="));

  if(!SD_MMC.exists(SUBGHZ_DIR)) {
    SD_MMC.mkdir(SUBGHZ_DIR);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("=[ FLIPPER .SUB ]="));
  display.println(F(""));

  File dir = SD_MMC.open(SUBGHZ_DIR);
  if(!dir) {
    display.println(F("No /subghz dir"));
    display.display();
    delay(2000);
    return;
  }

  File file = dir.openNextFile();
  int fileCount = 0;
  String fileNames[20];

  while(file && fileCount < 20) {
    String name = file.name();
    if(name.endsWith(".sub")) {
      fileNames[fileCount] = name;
      fileCount++;

      if(fileCount <= 5) {
        display.println(name);
      }
    }
    file = dir.openNextFile();
  }

  if(fileCount == 0) {
    display.println(F("No .sub files found"));
    display.println(F("Copy to /subghz"));
    display.display();
    delay(2000);
    return;
  }

  display.display();
  Serial.printf("[TX] Found %d .sub files\n", fileCount);

  // For now, just use the first file (same limitation as loadTXFromFile())
  // TODO: Add file selection menu
  String path = String(SUBGHZ_DIR) + "/" + fileNames[0];
  parseAndSendSubFile(path);
}
