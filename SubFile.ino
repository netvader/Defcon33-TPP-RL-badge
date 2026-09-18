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
// Princeton, Holtek, Ansonic and Hormann are fixed-code PWM protocols (no rolling
// code) - each bit is a mark/space pair, so their encoders are ported directly from
// the real Flipper firmware source (lib/subghz/protocols/*.c on
// flipperdevices/flipperzero-firmware) rather than guessed at. Legrand is likewise
// ported, but reads its own "TE:" field from the file like Princeton does, instead
// of using a fixed built-in timing.
//
// NOT implemented:
// - Somfy (Telis/Keytis): impossible without protocol-specific hardware knowledge -
//   even Flipper's own firmware can't send it (.yield = NULL in its encoder), since
//   Somfy RTS uses a real encrypted rolling code tied to the physical remote's own
//   secret counter. RAW playback of a captured Somfy signal still works, just not
//   "encode from a Key".
// - Hollarm: its real encoder needs a button/channel value it doesn't visibly read
//   from the file in the source I read, and computes a checksum I couldn't verify
//   without a real remote - left out rather than guessing.
// - Every other named protocol (KeeLoq family, Nice, Came, Security+, etc.): each
//   is its own can of worms (rolling codes, per-manufacturer key derivation) - out
//   of scope here.
//
// Any Protocol: value not listed above is reported as unsupported rather than guessed at.

#define SUBGHZ_DIR "/subghz"

uint64_t parseHexKey(String keyHex) {
  uint64_t code = 0;
  int idx = 0;
  while(idx < (int)keyHex.length()) {
    while(idx < (int)keyHex.length() && keyHex[idx] == ' ') idx++;
    int start = idx;
    while(idx < (int)keyHex.length() && keyHex[idx] != ' ') idx++;
    if(idx > start) {
      code = (code << 8) | strtoul(keyHex.substring(start, idx).c_str(), NULL, 16);
    }
  }
  return code;
}

bool sendPrincetonFromKey(String keyHex, int bitCount, int te) {
  if(bitCount <= 0 || bitCount > 32 || te <= 0) {
    Serial.println(F("[TX] Invalid Princeton parameters in .sub file"));
    return false;
  }

  uint32_t code = parseHexKey(keyHex);

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

// Holtek HT12A/HT12E-family encoder chip (te_short=430us, te_long=870us, 40 bits)
bool sendHoltekFromKey(String keyHex, int bitCount) {
  const int te_short = 430, te_long = 870;
  if(bitCount <= 0 || bitCount > 40) {
    Serial.println(F("[TX] Invalid Holtek bit count in .sub file"));
    return false;
  }

  uint64_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te_short; // start bit (HIGH)
  for(int i = bitCount - 1; i >= 0 && data_count < 1996; i--) {
    bool bitVal = (code >> i) & 0x01;
    if(bitVal) {
      data_to_send[data_count++] = te_long;  // LOW
      data_to_send[data_count++] = te_short; // HIGH
    } else {
      data_to_send[data_count++] = te_short; // LOW
      data_to_send[data_count++] = te_long;  // HIGH
    }
  }

  Serial.printf("[TX] Holtek: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, 5);
  return true;
}

// Ansonic (te_short=555us, te_long=1111us, typically 12 bits)
bool sendAnsonicFromKey(String keyHex, int bitCount) {
  const int te_short = 555, te_long = 1111;
  if(bitCount <= 0 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Ansonic bit count in .sub file"));
    return false;
  }

  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te_short; // start bit (HIGH)
  for(int i = bitCount - 1; i >= 0 && data_count < 1996; i--) {
    bool bitVal = (code >> i) & 0x01;
    if(bitVal) {
      data_to_send[data_count++] = te_short; // LOW
      data_to_send[data_count++] = te_long;  // HIGH
    } else {
      data_to_send[data_count++] = te_long;  // LOW
      data_to_send[data_count++] = te_short; // HIGH
    }
  }

  Serial.printf("[TX] Ansonic: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, 5);
  return true;
}

// Hormann (te_short=500us, te_long=1000us) - garage door openers using this simple
// fixed-code scheme (older non-BiSecur units); the real remote's 12000us start mark
// is kept, but the "20 internal loops x 10 outer repeats" of the original firmware
// is reduced to a flat 5x via sendRawData()'s transmissions param
bool sendHormannFromKey(String keyHex, int bitCount) {
  const int te_short = 500, te_long = 1000;
  if(bitCount <= 0 || bitCount > 44) {
    Serial.println(F("[TX] Invalid Hormann bit count in .sub file"));
    return false;
  }

  uint64_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te_short * 24; // start bit (HIGH, long)
  data_to_send[data_count++] = te_short;      // LOW
  for(int i = bitCount - 1; i >= 0 && data_count < 1996; i--) {
    bool bitVal = (code >> i) & 0x01;
    if(bitVal) {
      data_to_send[data_count++] = te_long;  // HIGH
      data_to_send[data_count++] = te_short; // LOW
    } else {
      data_to_send[data_count++] = te_short; // HIGH
      data_to_send[data_count++] = te_long;  // LOW
    }
  }

  Serial.printf("[TX] Hormann: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, 5);
  return true;
}

// Legrand (In'O home automation) - te comes from the file's own "TE:" field, and the
// protocol repeats 5x internally with a sync gap before each repeat
bool sendLegrandFromKey(String keyHex, int bitCount, int te) {
  if(bitCount <= 0 || bitCount > 32 || te <= 0) {
    Serial.println(F("[TX] Invalid Legrand parameters in .sub file"));
    return false;
  }

  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int r = 0; r < 5 && data_count < 1900; r++) {
    if(r > 0) {
      data_to_send[data_count++] = te * 16; // sync gap (LOW) before repeats after the first
    }
    for(int i = bitCount - 1; i >= 0 && data_count < 1996; i--) {
      bool bitVal = (code >> i) & 0x01;
      if(i == bitCount - 1) {
        // first bit of this repeat is a lone mark (no preceding low component)
        data_to_send[data_count++] = bitVal ? (te * 3) : te;
      } else if(bitVal) {
        data_to_send[data_count++] = te;     // LOW
        data_to_send[data_count++] = te * 3; // HIGH
      } else {
        data_to_send[data_count++] = te * 3; // LOW
        data_to_send[data_count++] = te;     // HIGH
      }
    }
  }

  Serial.printf("[TX] Legrand: %d bits, TE=%dus, code=0x%08lX\n", bitCount, te, (unsigned long)code);
  sendRawData(data_to_send, data_count, 2); // repeats are already baked into the buffer above
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
  if(protocol == "Holtek") {
    return sendHoltekFromKey(keyHex, bitCount);
  }
  if(protocol == "Ansonic") {
    return sendAnsonicFromKey(keyHex, bitCount);
  }
  if(protocol == "Hormann") {
    return sendHormannFromKey(keyHex, bitCount);
  }
  if(protocol == "Legrand") {
    return sendLegrandFromKey(keyHex, bitCount, te);
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
