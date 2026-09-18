// RX.ino - RX mode with waterfall display and reactive NeoPixels

// RX state variables
bool rxActive = false;
unsigned long rxStartTime = 0;
unsigned long lastSignalTime = 0;
int signalCount = 0;
unsigned long lastRXRefresh = 0;  // Periodic refresh

// Waterfall display
#define WATERFALL_WIDTH 128
#define WATERFALL_HEIGHT 50
uint8_t waterfallBuffer[WATERFALL_HEIGHT][WATERFALL_WIDTH / 8];

// Signal processing
int currentRSSI = -100;
int instantRSSI = -100;  // Immediate signal detection
int baselineRSSI = -100;
bool baselineSet = false;
unsigned long baselineStartTime = 0;
int baselineSamples = 0;
long baselineSum = 0;

// Global RSSI variables (used elsewhere in code)
float avgRSSI = -100;
int peakRSSI = -120;

// NeoPixel animation for RX
uint8_t rxPixelHue = 0;
unsigned long lastRXPixelUpdate = 0;
int signalPixelBrightness = 0;

// Raw pulse-timing capture (feeds sample[]/samplecount for "TX Last RX" and file save)
volatile unsigned long isrLastEdge = 0;
volatile int isrSampleIndex = 0;
volatile bool isrCapturing = false;
int captureGdoPin = -1;
bool rxCaptureActive = false;

// CC1101 Direct SPI functions
byte readCC1101Register(byte addr) {
  int csPin = (activeModule == 0) ? CC1101_CS_A : CC1101_CS_B;
  int misoPin = (activeModule == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  
  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, LOW);
  while(digitalRead(misoPin));
  
  SPI.transfer(addr | 0x80);
  byte value = SPI.transfer(0x00);
  
  digitalWrite(csPin, HIGH);
  return value;
}

void writeCC1101Register(byte addr, byte value) {
  int csPin = (activeModule == 0) ? CC1101_CS_A : CC1101_CS_B;
  int misoPin = (activeModule == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  
  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, LOW);
  while(digitalRead(misoPin));
  
  SPI.transfer(addr);
  SPI.transfer(value);
  
  digitalWrite(csPin, HIGH);
}

void strobeCC1101(byte cmd) {
  int csPin = (activeModule == 0) ? CC1101_CS_A : CC1101_CS_B;
  int misoPin = (activeModule == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  
  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, LOW);
  while(digitalRead(misoPin));
  
  SPI.transfer(cmd);
  
  digitalWrite(csPin, HIGH);
}

byte readCC1101Status(byte addr) {
  int csPin = (activeModule == 0) ? CC1101_CS_A : CC1101_CS_B;
  int misoPin = (activeModule == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  
  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, LOW);
  while(digitalRead(misoPin));
  
  SPI.transfer(addr | 0xC0);  // Status register burst read
  byte value = SPI.transfer(0x00);
  
  digitalWrite(csPin, HIGH);
  return value;
}

// Fires on every edge of the CC1101's demodulated data line (GDO0, configured
// as async serial data output) and records the time since the previous edge.
// This produces the same alternating HIGH/LOW pulse-width format that TX.ino's
// sendLastRX()/sendRawData() replay - so whatever we capture here can be
// transmitted back out or saved to SD as-is.
void IRAM_ATTR rxEdgeISR() {
  unsigned long now = micros();
  unsigned long delta = now - isrLastEdge;
  isrLastEdge = now;

  if(!isrCapturing) return;
  if(isrSampleIndex < SAMPLE_SIZE) {
    sample[isrSampleIndex++] = delta;
  }
}

void beginRawCapture() {
  isrSampleIndex = 0;
  isrLastEdge = micros();
  isrCapturing = true;
  // Only attach the interrupt for the duration of an actual capture window - left
  // attached for a whole RX session, it fires continuously on GDO0's demodulator
  // noise (no signal present most of the time) at a rate high enough to starve the
  // watchdog and reset the badge
  attachInterrupt(digitalPinToInterrupt(captureGdoPin), rxEdgeISR, CHANGE);
}

void endRawCapture() {
  detachInterrupt(digitalPinToInterrupt(captureGdoPin));
  noInterrupts();
  isrCapturing = false;
  samplecount = isrSampleIndex;
  interrupts();
}

void refreshRXState() {
  // Check current state
  byte marcState = readCC1101Status(0x35);  // MARCSTATE register
  
  // If not in RX state (0x0D), put it back
  if((marcState & 0x1F) != 0x0D) {
    Serial.printf("[RX] CC1101 not in RX (state: 0x%02X), resetting...\n", marcState);
    
    // Flush RX FIFO
    strobeCC1101(0x3A); // SFRX
    
    // Go back to RX
    strobeCC1101(0x34); // SRX
    delay(1);
  }
}

void startRX() {
  Serial.println(F("[RX] Starting RX mode"));
  
  // Check which module to use
  if(!cc1101APresent && !cc1101BPresent) {
    debugPrint("No CC1101 found!", true, true, 2000);
    return;
  }
  
  // Use whichever module is available
  if(activeModule == 0 && !cc1101APresent) {
    activeModule = 1;
    Serial.println(F("[RX] Module A not present, switching to B"));
  } else if(activeModule == 1 && !cc1101BPresent) {
    activeModule = 0;
    Serial.println(F("[RX] Module B not present, switching to A"));
  }
  
  Serial.printf("[RX] Using module: %c\n", activeModule == 0 ? 'A' : 'B');
  
  // Initialize variables
  raw_rx = "1";
  rxActive = true;
  rxStartTime = millis();
  lastRXRefresh = millis();
  signalCount = 0;
  currentRSSI = -100;
  instantRSSI = -100;
  baselineRSSI = -100;
  baselineSet = false;
  baselineStartTime = millis();
  baselineSamples = 0;
  baselineSum = 0;
  avgRSSI = -100;
  peakRSSI = -120;
  
  // Clear waterfall buffer
  memset(waterfallBuffer, 0, sizeof(waterfallBuffer));
  
  // Show initializing message
  display.clearDisplay();
  display.setCursor(0, 20);
  display.println(F("Calibrating..."));
  display.println(F("Please wait"));
  display.printf("Module: %c", activeModule == 0 ? 'A' : 'B');
  display.display();
  
  // Initialize SPI for the selected module
  int sckPin = (activeModule == 0) ? CC1101_CLK_A : CC1101_CLK_B;
  int misoPin = (activeModule == 0) ? CC1101_MISO_A : CC1101_MISO_B;
  int mosiPin = (activeModule == 0) ? CC1101_MOSI_A : CC1101_MOSI_B;
  int csPin = (activeModule == 0) ? CC1101_CS_A : CC1101_CS_B;
  
  SPI.begin(sckPin, misoPin, mosiPin, csPin);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  
  // Reset CC1101
  strobeCC1101(0x30); // SRES
  delay(10);
  
  // Configure frequency based on setting
  byte freq2, freq1, freq0;
  if(frequency >= 433 && frequency <= 435) {
    freq2 = 0x10; freq1 = 0xB0; freq0 = 0x7A; // ~433.92 MHz
  } else if(frequency >= 314 && frequency <= 316) {
    freq2 = 0x0C; freq1 = 0x4E; freq0 = 0xC4; // ~315 MHz
  } else if(frequency >= 867 && frequency <= 869) {
    freq2 = 0x21; freq1 = 0x65; freq0 = 0x6A; // ~868 MHz
  } else if(frequency >= 914 && frequency <= 916) {
    freq2 = 0x23; freq1 = 0x31; freq0 = 0x3B; // ~915 MHz
  } else {
    freq2 = 0x10; freq1 = 0xB0; freq0 = 0x7A; // Default to 433.92
  }
  
  writeCC1101Register(0x0D, freq2); // FREQ2
  writeCC1101Register(0x0E, freq1); // FREQ1  
  writeCC1101Register(0x0F, freq0); // FREQ0
  
  // Configure for basic RX with selected modulation
  writeCC1101Register(0x10, 0x00); // MDMCFG4 - Max BW
  writeCC1101Register(0x11, 0x22); // MDMCFG3
  
  // Set modulation based on mod variable
  byte mdmcfg2 = 0x00;
  switch(mod) {
    case 0: mdmcfg2 = 0x00; break; // 2-FSK
    case 1: mdmcfg2 = 0x10; break; // GFSK
    case 2: mdmcfg2 = 0x30; break; // ASK/OOK
    case 3: mdmcfg2 = 0x40; break; // 4-FSK
    case 4: mdmcfg2 = 0x70; break; // MSK
  }
  writeCC1101Register(0x12, mdmcfg2); // MDMCFG2 - Modulation
  
  writeCC1101Register(0x07, 0x04); // PKTCTRL1
  writeCC1101Register(0x08, 0x00); // PKTCTRL0

  // AGC settings
  writeCC1101Register(0x1B, 0x40); // AGCCTRL2
  writeCC1101Register(0x1C, 0x00); // AGCCTRL1
  writeCC1101Register(0x1D, 0x91); // AGCCTRL0

  // GDO0 outputs the demodulated bitstream (async serial data output) so we
  // can capture raw pulse timings on it independently of the RSSI polling below
  writeCC1101Register(0x02, 0x0D); // IOCFG0 - Serial Data Output, asynchronous

  // Flush RX FIFO
  strobeCC1101(0x3A); // SFRX

  // Calibrate and enter RX
  strobeCC1101(0x33); // SCAL
  delay(5);
  strobeCC1101(0x34); // SRX
  delay(5);

  Serial.println(F("[RX] CC1101 configured, calibrating baseline..."));
  Serial.printf("[RX] Frequency: %.2f MHz, Modulation: %s\n", frequency, getModulationName(mod));

  // Prepare (but don't yet arm) raw pulse capture on the GDO0 data line - the
  // interrupt itself is only attached for the duration of an actual capture, see
  // beginRawCapture()/endRawCapture()
  captureGdoPin = (activeModule == 0) ? CC1101_GDO0_A : CC1101_GDO0_B;
  pinMode(captureGdoPin, INPUT);
  rxCaptureActive = false;
  isrSampleIndex = 0;
  isrCapturing = false;

  // Set pixel mode
  pixelMode = PIXEL_RX;
  pixels.clear();
  pixels.show();
}

void stopRX() {
  Serial.println(F("[RX] Stopping RX mode"));

  raw_rx = "0";
  rxActive = false;
  pixelMode = PIXEL_MENU;

  if(rxCaptureActive) {
    endRawCapture(); // also detaches the interrupt
    rxCaptureActive = false;
  }
  captureGdoPin = -1;

  strobeCC1101(0x36); // SIDLE
  
  SPI.endTransaction();
  SPI.end();
  
  pixels.clear();
  pixels.show();
  
  // Show summary
  unsigned long rxDuration = (millis() - rxStartTime) / 1000;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("=[ RX STOPPED ]="));
  display.println();
  display.printf("Duration: %lu sec\n", rxDuration);
  display.printf("Signals: %d\n", signalCount);
  display.printf("Baseline: %d dBm\n", baselineRSSI);
  display.printf("Avg: %d dBm\n", (int)avgRSSI);
  display.printf("Peak: %d dBm\n", peakRSSI);
  display.display();
  delay(2000);
  
  currentMenu = MENU_RX;
  updateDisplay();
}

void handleRXMode() {
  if(!rxActive) return;
  
  // Check for stop condition
  if(digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW || 
     digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW || 
     digitalRead(BTN_SELECT) == LOW) {
    if(millis() - lastButtonPress > buttonDebounce) {
      lastButtonPress = millis();
      stopRX();
      return;
    }
  }
  
  // Periodically refresh RX state (every 5 seconds)
  if(millis() - lastRXRefresh > 5000) {
    lastRXRefresh = millis();
    refreshRXState();
  }
  
  // Update displays
  updateWaterfall();
  if(baselineSet) {
    drawWaterfall();
  }
  updateRXPixels();
}

void updateWaterfall() {
  static unsigned long lastUpdate = 0;
  static int rssiSamples = 0;
  static long rssiSum = 0;
  
  // Update rate: 20 times per second
  if(millis() - lastUpdate < 50) return;
  lastUpdate = millis();
  
  // Read RSSI
  byte rawRSSI = readCC1101Register(0x34 | 0x40);
  
  // Convert to dBm and store as INSTANT RSSI
  if (rawRSSI >= 128) {
    instantRSSI = ((int)(rawRSSI - 256) / 2) - 74;
  } else {
    instantRSSI = (rawRSSI / 2) - 74;
  }
  
  // Keep currentRSSI for display (smoothed)
  currentRSSI = instantRSSI;
  
  // Update running average for display only
  rssiSum += instantRSSI;
  rssiSamples++;
  if(rssiSamples >= 20) {
    avgRSSI = (float)rssiSum / rssiSamples;
    rssiSum = 0;
    rssiSamples = 0;
  }
  
  // Update peak based on instant RSSI
  if(instantRSSI > peakRSSI) {
    peakRSSI = instantRSSI;
  }
  
  // Calibration phase - first 1 second
  if(!baselineSet) {
    if(millis() - baselineStartTime < 1000) {
      baselineSum += instantRSSI;
      baselineSamples++;
      
      // Update calibration display
      display.clearDisplay();
      display.setCursor(0, 20);
      display.println(F("Calibrating..."));
      display.printf("RSSI: %d dBm\n", instantRSSI);
      display.printf("Samples: %d\n", baselineSamples);
      display.printf("Module: %c", activeModule == 0 ? 'A' : 'B');
      display.display();
      return;
    } else {
      // Set baseline
      baselineRSSI = baselineSum / baselineSamples;
      baselineSet = true;
      avgRSSI = baselineRSSI; // Initialize average
      Serial.printf("[RX] Baseline set: %d dBm (%d samples)\n", baselineRSSI, baselineSamples);
      
      display.clearDisplay();
      display.setCursor(0, 20);
      display.println(F("Ready!"));
      display.printf("Baseline: %d dBm\n", baselineRSSI);
      display.display();
      delay(500);
    }
  }
  
  // ALWAYS shift waterfall down
  for(int y = WATERFALL_HEIGHT - 1; y > 0; y--) {
    memcpy(waterfallBuffer[y], waterfallBuffer[y-1], WATERFALL_WIDTH / 8);
  }
  
  // ALWAYS clear top line first
  memset(waterfallBuffer[0], 0, WATERFALL_WIDTH / 8);
  
  // Check for signal using INSTANT RSSI (not averaged!)
  int spike = instantRSSI - baselineRSSI;
  
  if(spike > 3) {  // Signal detected
    signalCount++;
    lastSignalTime = millis();

    if(!rxCaptureActive) {
      beginRawCapture();
      rxCaptureActive = true;
      Serial.println(F("[RX] Signal capture started"));
    }

    // Map spike to width
    int width = map(spike, 3, 40, 1, WATERFALL_WIDTH);
    width = constrain(width, 1, WATERFALL_WIDTH);
    
    // Draw centered horizontal bar on TOP LINE ONLY
    int start = (WATERFALL_WIDTH - width) / 2;
    int end = start + width;
    
    for(int x = start; x < end; x++) {
      if(x >= 0 && x < WATERFALL_WIDTH) {
        waterfallBuffer[0][x / 8] |= (1 << (7 - (x % 8)));
      }
    }
    
    signalPixelBrightness = map(spike, 3, 40, 50, 255);
    
    Serial.printf("[RX] Signal! Instant RSSI: %d, Spike: +%d dB, Width: %d\n", 
                  instantRSSI, spike, width);
    
    // After detecting a strong signal, flush the RX FIFO to prevent overflow
    if(spike > 20) {
      strobeCC1101(0x3A); // SFRX - flush RX FIFO
      strobeCC1101(0x34); // SRX - back to RX mode
    }
  } else {
    // No signal - the line is already cleared above
    // Add minimal noise for visual feedback
    for(int i = 0; i < 2; i++) {
      int x = random(WATERFALL_WIDTH);
      waterfallBuffer[0][x / 8] |= (1 << (7 - (x % 8)));
    }
  }
  
  // Slowly drift baseline if needed (but use instant RSSI)
  static int noSignalCount = 0;
  if(spike <= 3) {
    noSignalCount++;
    if(noSignalCount > 200) {  // After 10 seconds of no signal
      // Slowly adjust baseline toward current instant RSSI
      baselineRSSI = (baselineRSSI * 19 + instantRSSI) / 20;
      noSignalCount = 100;  // Don't reset completely, keep some count
    }
  } else {
    noSignalCount = 0;  // Reset on signal detection
  }

  // Finish an in-progress capture once the transmission has gone quiet for
  // 300ms, or once the buffer is full - whichever comes first
  if(rxCaptureActive && (millis() - lastSignalTime > 300 || isrSampleIndex >= SAMPLE_SIZE)) {
    endRawCapture();
    rxCaptureActive = false;

    if(samplecount > 0) {
      Serial.printf("[RX] Signal capture complete: %d samples\n", samplecount);

      if(sdCardPresent) {
        char fname[32];
        snprintf(fname, sizeof(fname), "/rx_data/rx_%lu.txt", millis());
        saveRXData(fname);
      }

      if(weatherListening) {
        tryDecodeWeather();
      }
    }
  }
}

void drawWaterfall() {
  display.clearDisplay();
  
  // Header
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print(frequency, 1);
  display.print(F("MHz "));
  display.print(activeModule == 0 ? 'A' : 'B');
  
  // Show instant RSSI delta (not averaged)
  display.setCursor(50, 0);
  int delta = instantRSSI - baselineRSSI;
  if(delta > 0) {
    display.printf("+%d", delta);
  } else {
    display.print(delta);
  }
  
  // Signal count
  display.setCursor(90, 0);
  display.printf("S:%d", signalCount);
  
  // Signal indicator (based on instant signal detection)
  if(millis() - lastSignalTime < 100) {
    display.fillCircle(120, 4, 3, SH110X_WHITE);
  }
  
  // Draw separator
  display.drawLine(0, 9, 127, 9, SH110X_WHITE);
  
  // Draw waterfall
  for(int y = 0; y < WATERFALL_HEIGHT && (y + 11) < 64; y++) {
    for(int byteX = 0; byteX < WATERFALL_WIDTH / 8; byteX++) {
      uint8_t byte = waterfallBuffer[y][byteX];
      if(byte != 0) {
        for(int bit = 0; bit < 8; bit++) {
          if(byte & (1 << (7 - bit))) {
            int x = byteX * 8 + bit;
            display.drawPixel(x, y + 11, SH110X_WHITE);
          }
        }
      }
    }
  }
  
  display.display();
}

// NeoPixel functions
void updateRXPixels() {
  if(!rxActive) return;
  if(millis() - lastRXPixelUpdate < 20) return;
  lastRXPixelUpdate = millis();
  
  // Fade signal brightness
  if(signalPixelBrightness > 0) {
    signalPixelBrightness -= 5;
    if(signalPixelBrightness < 0) signalPixelBrightness = 0;
  }
  
  // Simple scanner with signal flash
  rxScannerEffect(signalPixelBrightness);
  pixels.show();
}

void rxScannerEffect(int signalBrightness) {
  static int scanPos = 0;
  static int scanDir = 1;
  
  pixels.clear();
  
  // Base scanner
  for(int i = 0; i < 5; i++) {
    int pos = scanPos - (i * scanDir);
    if(pos >= 0 && pos < NEOPIXEL_COUNT) {
      int brightness = 100 - (i * 20);
      pixels.setPixelColor(pos, pixels.Color(0, 0, brightness));
    }
  }
  
  // Signal flash overlay
  if(signalBrightness > 0) {
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      uint32_t current = pixels.getPixelColor(i);
      uint8_t b = current & 0xFF;
      pixels.setPixelColor(i, pixels.Color(
        signalBrightness / 4,
        signalBrightness / 2,
        min(255, b + signalBrightness)
      ));
    }
  }
  
  scanPos += scanDir;
  if(scanPos >= NEOPIXEL_COUNT - 1 || scanPos <= 0) {
    scanDir = -scanDir;
  }
}

// Stub functions
void rxBreathingEffect(int strength) {}
void rxSpectrumAnalyzer(int strength) {}
void signalDetectedAnimation() {}
void enableReceive() {}
void receiver() {}
bool checkReceived() { return false; }
void processSignal() {}
void cycleRXVisualization() {}
void printReceived() {}
void signalanalyse() {}
void drawRFMatrix() {}
void drawRFScope() {}
void drawSignalMeter() {}
void showError(const char* error) {}