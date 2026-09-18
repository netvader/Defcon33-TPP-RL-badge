// FullDuplex.ino - simultaneous RX (module A) + TX (module B), for testing your own
// devices: capture a signal on module A and immediately replay it out on module B,
// without ever stopping listening. Everything else in this firmware only uses one
// CC1101 module at a time (see activeModule); this mode is the exception - both run
// concurrently, on separate SPI buses, with module A's capture driven by a
// background FreeRTOS task pinned to core 0 so it keeps running no matter what the
// main loop (core 1) is doing.
//
// ONLY for devices you own or are otherwise authorized to test - this is exactly the
// working principle of a relay attack (capture a signal in one place, instantly
// retransmit it elsewhere), which is why it's kept as its own explicit mode rather
// than something that happens automatically.

SPIClass fdSpiB(HSPI);

bool fdActive = false;
bool fdAutoReplay = true;
unsigned long fdSample[SAMPLE_SIZE];
volatile int fdSampleCount = 0;
volatile unsigned long fdLastEdge = 0;
volatile bool fdCapturing = false;
int fdPacketsCaptured = 0;
int fdPacketsReplayed = 0;
unsigned long fdLastActivityTime = 0;
TaskHandle_t fdTaskHandle = NULL;

void IRAM_ATTR fdEdgeISR() {
  unsigned long now = micros();
  unsigned long delta = now - fdLastEdge;
  fdLastEdge = now;
  if(!fdCapturing) return;
  if(fdSampleCount < SAMPLE_SIZE) {
    fdSample[fdSampleCount++] = delta;
  }
}

// --- Module A (RX side) register access, shares the default SPI bus ---
byte fdReadRegA(byte addr) {
  digitalWrite(CC1101_CS_A, LOW);
  while(digitalRead(CC1101_MISO_A));
  SPI.transfer(addr | 0x80);
  byte value = SPI.transfer(0x00);
  digitalWrite(CC1101_CS_A, HIGH);
  return value;
}

void fdWriteRegA(byte addr, byte value) {
  digitalWrite(CC1101_CS_A, LOW);
  while(digitalRead(CC1101_MISO_A));
  SPI.transfer(addr);
  SPI.transfer(value);
  digitalWrite(CC1101_CS_A, HIGH);
}

void fdStrobeA(byte cmd) {
  digitalWrite(CC1101_CS_A, LOW);
  while(digitalRead(CC1101_MISO_A));
  SPI.transfer(cmd);
  digitalWrite(CC1101_CS_A, HIGH);
}

// --- Module B (TX side) register access, its own dedicated SPI bus so a transmit
// never has to wait on whatever module A's continuous RX polling is doing ---
void fdWriteRegB(byte addr, byte value) {
  digitalWrite(CC1101_CS_B, LOW);
  while(digitalRead(CC1101_MISO_B));
  fdSpiB.transfer(addr);
  fdSpiB.transfer(value);
  digitalWrite(CC1101_CS_B, HIGH);
}

void fdStrobeB(byte cmd) {
  digitalWrite(CC1101_CS_B, LOW);
  while(digitalRead(CC1101_MISO_B));
  fdSpiB.transfer(cmd);
  digitalWrite(CC1101_CS_B, HIGH);
}

// Shared frequency/modulation table (same values used elsewhere in the firmware),
// applied to whichever module's register-write function is passed in
void fdConfigureFrequencyMod(void (*writeReg)(byte, byte)) {
  byte freq2, freq1, freq0;
  if(frequency >= 433 && frequency <= 435) { freq2 = 0x10; freq1 = 0xB0; freq0 = 0x7A; }
  else if(frequency >= 314 && frequency <= 316) { freq2 = 0x0C; freq1 = 0x4E; freq0 = 0xC4; }
  else if(frequency >= 867 && frequency <= 869) { freq2 = 0x21; freq1 = 0x65; freq0 = 0x6A; }
  else if(frequency >= 914 && frequency <= 916) { freq2 = 0x23; freq1 = 0x31; freq0 = 0x3B; }
  else { freq2 = 0x10; freq1 = 0xB0; freq0 = 0x7A; }
  writeReg(0x0D, freq2);
  writeReg(0x0E, freq1);
  writeReg(0x0F, freq0);

  byte mdmcfg2 = 0x00;
  switch(mod) {
    case 0: mdmcfg2 = 0x00; break;
    case 1: mdmcfg2 = 0x10; break;
    case 2: mdmcfg2 = 0x30; break;
    case 3: mdmcfg2 = 0x40; break;
    case 4: mdmcfg2 = 0x70; break;
  }
  writeReg(0x10, 0x00);
  writeReg(0x11, 0x22);
  writeReg(0x12, mdmcfg2);
  writeReg(0x07, 0x04);
  writeReg(0x08, 0x00);
  writeReg(0x1B, 0x40);
  writeReg(0x1C, 0x00);
  writeReg(0x1D, 0x91);
}

void fdSetupModuleA() {
  pinMode(CC1101_CS_A, OUTPUT);
  digitalWrite(CC1101_CS_A, HIGH);
  SPI.begin(CC1101_CLK_A, CC1101_MISO_A, CC1101_MOSI_A, CC1101_CS_A);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

  fdStrobeA(0x30); // SRES
  delay(10);
  fdConfigureFrequencyMod(fdWriteRegA);
  fdWriteRegA(0x02, 0x0D); // IOCFG0 - async serial data output, for raw capture
  fdStrobeA(0x3A);         // SFRX
  fdStrobeA(0x33);         // SCAL
  delay(5);
  fdStrobeA(0x34); // SRX
  delay(5);
}

void fdSetupModuleB() {
  pinMode(CC1101_CS_B, OUTPUT);
  digitalWrite(CC1101_CS_B, HIGH);
  fdSpiB.begin(CC1101_CLK_B, CC1101_MISO_B, CC1101_MOSI_B, CC1101_CS_B);
  fdSpiB.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

  fdStrobeB(0x30); // SRES
  delay(10);
  fdConfigureFrequencyMod(fdWriteRegB);
  fdWriteRegB(0x3E, 0xC0); // PATABLE - max power
  fdWriteRegB(0x02, 0x0C); // IOCFG0 - serial TX data
  fdStrobeB(0x33);         // SCAL
  delay(5);
  fdStrobeB(0x36); // SIDLE - stay idle, only key up when actually replaying
}

void fdReplayOnModuleB(unsigned long *data, int count) {
  fdStrobeB(0x35); // STX
  for(int i = 0; i < count; i += 2) {
    digitalWrite(CC1101_GDO0_B, HIGH);
    delayMicroseconds(data[i]);
    digitalWrite(CC1101_GDO0_B, LOW);
    if(i + 1 < count) {
      delayMicroseconds(data[i + 1]);
    }
  }
  digitalWrite(CC1101_GDO0_B, LOW);
  fdStrobeB(0x36); // SIDLE
  fdPacketsReplayed++;
  fdLastActivityTime = millis();
}

// Runs continuously on core 0: mirrors RX.ino's RSSI-spike capture logic, but
// entirely independent of the normal RX mode's state (rxActive/sample[]) so both
// can never interfere with each other
void fdTask(void *param) {
  unsigned long baselineStart = millis();
  long baselineSum = 0;
  int baselineSamples = 0;
  int baselineRSSI = -100;
  bool baselineSet = false;
  bool capturing = false;
  unsigned long lastSignalTime = 0;

  while(fdActive) {
    byte rawRSSI = fdReadRegA(0x34 | 0x40);
    int instantRSSI = (rawRSSI >= 128) ? (((int)(rawRSSI - 256) / 2) - 74) : ((rawRSSI / 2) - 74);

    if(!baselineSet) {
      if(millis() - baselineStart < 1000) {
        baselineSum += instantRSSI;
        baselineSamples++;
      } else {
        baselineRSSI = (baselineSamples > 0) ? (baselineSum / baselineSamples) : -100;
        baselineSet = true;
      }
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    int spike = instantRSSI - baselineRSSI;
    if(spike > 3) {
      lastSignalTime = millis();
      if(!capturing) {
        // Only attach the interrupt for the duration of an actual capture window -
        // left attached continuously, it fires nonstop on GDO0's demodulator noise
        // (no signal present most of the time) fast enough to starve the watchdog
        fdSampleCount = 0;
        fdLastEdge = micros();
        fdCapturing = true;
        attachInterrupt(digitalPinToInterrupt(CC1101_GDO0_A), fdEdgeISR, CHANGE);
        capturing = true;
      }
    }

    if(capturing && (millis() - lastSignalTime > 300 || fdSampleCount >= SAMPLE_SIZE)) {
      detachInterrupt(digitalPinToInterrupt(CC1101_GDO0_A));
      noInterrupts();
      fdCapturing = false;
      int count = fdSampleCount;
      interrupts();
      capturing = false;

      if(count > 0) {
        fdPacketsCaptured++;
        fdLastActivityTime = millis();
        if(fdAutoReplay) {
          fdReplayOnModuleB(fdSample, count);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }

  fdTaskHandle = NULL;
  vTaskDelete(NULL);
}

void startFullDuplex() {
  Serial.println(F("\n[FullDuplex] ========== STARTING FULL DUPLEX =========="));

  if(!cc1101APresent || !cc1101BPresent) {
    debugPrint("Need both modules!", true, true, 2000);
    return;
  }

  pinMode(CC1101_GDO0_A, INPUT);
  pinMode(CC1101_GDO0_B, OUTPUT);
  digitalWrite(CC1101_GDO0_B, LOW);

  fdSetupModuleA();
  fdSetupModuleB();

  fdSampleCount = 0;
  fdCapturing = false;
  fdPacketsCaptured = 0;
  fdPacketsReplayed = 0;
  fdLastActivityTime = millis();
  fdActive = true;

  // The interrupt itself is only attached for the duration of an actual capture
  // window (armed/disarmed inside fdTask()), not for the whole session
  xTaskCreatePinnedToCore(fdTask, "FullDuplexRX", 4096, NULL, 1, &fdTaskHandle, 0);

  currentMenu = MENU_FULLDUPLEX;
  pixelMode = PIXEL_FULLDUPLEX;
}

void stopFullDuplex() {
  Serial.println(F("[FullDuplex] Stopping"));

  fdActive = false;
  unsigned long waitStart = millis();
  while(fdTaskHandle != NULL && millis() - waitStart < 300) {
    delay(5);
  }

  detachInterrupt(digitalPinToInterrupt(CC1101_GDO0_A));

  fdStrobeA(0x36); // SIDLE
  SPI.endTransaction();
  SPI.end();

  fdStrobeB(0x36); // SIDLE
  fdSpiB.endTransaction();
  fdSpiB.end();

  currentMenu = MENU_MAIN;
  menuSelection = 0;
  menuOffset = 0;
  pixelMode = PIXEL_MENU;
  updateDisplay();
}

void handleFullDuplexMode() {
  static unsigned long lastDraw = 0;
  if(millis() - lastDraw < 100) return;
  lastDraw = millis();

  drawFullDuplexMenu();
}

void drawFullDuplexMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("=[ FULL DUPLEX ]="));
  display.drawLine(0, 9, 127, 9, SH110X_WHITE);

  display.setCursor(0, 14);
  display.printf("A(RX) -> B(TX)\n");
  display.printf("%.2f MHz\n", frequency);
  display.println();
  display.printf("Captured: %d\n", fdPacketsCaptured);
  display.printf("Replayed: %d\n", fdPacketsReplayed);
  display.println();
  display.printf("Auto-replay: %s\n", fdAutoReplay ? "ON" : "OFF");
  display.print(F("SELECT=toggle"));

  display.display();
}

void fullDuplexPixelEffect() {
  static float phase = 0;

  // Base: slow blue breathing on module A's "listening" side
  float breath = (sin(phase * 0.0174533) + 1.0) / 2.0;
  uint8_t baseB = 20 + breath * 40;
  for(int i = 0; i < NEOPIXEL_COUNT / 2; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, baseB));
  }

  // Flash the other half red briefly whenever module B just replayed something
  bool recentReplay = (millis() - fdLastActivityTime) < 150;
  for(int i = NEOPIXEL_COUNT / 2; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, recentReplay ? pixels.Color(255, 0, 0) : pixels.Color(20, 0, 0));
  }

  phase += 2;
  if(phase >= 360) phase = 0;
}
