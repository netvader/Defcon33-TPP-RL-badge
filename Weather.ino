// Weather.ino - "Weather Station" mode
//
// Inspired by the Flipper Zero weather_station FAP
// (https://github.com/flipperdevices/flipperzero-good-faps/tree/dev/weather_station):
// listens for 433MHz OOK temperature/humidity sensors and reacts with the badge's
// NeoPixels once a reading comes in - blue-ish for rain (high humidity), yellow for
// sun, red for too hot, and a soft grey "cloud" animation for anything in between.
//
// This decodes ONE protocol family - "Nexus"-style sensors (Nexus, Solight TE82S,
// Rubicson 1444, FreeTec NC-7345/NX-3980 and various clones), which is one of the
// simpler ones the Flipper app supports. It reuses the raw pulse-timing capture
// already wired up in RX.ino (see beginRawCapture()/rxEdgeISR()) rather than
// re-implementing signal capture.
//
// Ported from the real decoder in flipperdevices/flipperzero-good-faps
// (weather_station/protocols/nexus_th.c, dev branch) rather than guessed at:
// PPM/distance-coded, HIGH mark is always ~500us, and the LOW gap after it decides
// the bit (~1000us = 0, ~2000us = 1); a sync gap of ~4000us precedes each 36-bit
// frame, sent 12x per transmission. Field layout, MSB first:
//   id(8) battery_low(1, inverted) unused(1) channel(2) sign(1) temp_magnitude(11)
//   const(4, must be 0xF - used as a basic validity check) humidity(8)

bool weatherListening = false;
WeatherCondition weatherCondition = WEATHER_NONE;
float lastWeatherTempC = 0;
int lastWeatherHumidity = -1;
uint8_t lastWeatherSensorId = 0;
bool lastWeatherBatteryLow = false;
int weatherPacketCount = 0;
unsigned long lastWeatherPacketTime = 0;

#define NEXUS_FRAME_BITS 36
#define NEXUS_TE_SHORT 500
#define NEXUS_TE_LOW_0 1000
#define NEXUS_TE_LOW_1 2000
#define NEXUS_TE_SYNC  4000
#define NEXUS_TE_DELTA 200

// samples[] holds alternating HIGH,LOW pulse durations in microseconds (starting
// with a HIGH), exactly as captured by RX.ino's rxEdgeISR(). Scan for a sync gap
// (~4000us LOW), then try to decode the 36 bits that follow it.
bool decodeNexusWeather(unsigned long *samples, int count, float &tempC, int &humidity, uint8_t &id, bool &batteryLow) {
  for(int start = 1; start + NEXUS_FRAME_BITS * 2 <= count; start += 2) {
    long syncDiff = (long)samples[start] - NEXUS_TE_SYNC;
    if(abs(syncDiff) > NEXUS_TE_DELTA * 2) continue;

    uint64_t bits = 0;
    bool ok = true;
    for(int b = 0; b < NEXUS_FRAME_BITS; b++) {
      unsigned long high = samples[start + 1 + b * 2];
      unsigned long low = samples[start + 2 + b * 2];

      if(abs((long)high - NEXUS_TE_SHORT) > NEXUS_TE_DELTA) {
        ok = false;
        break;
      }
      if(abs((long)low - NEXUS_TE_LOW_0) <= NEXUS_TE_DELTA) {
        bits = (bits << 1) | 0;
      } else if(abs((long)low - NEXUS_TE_LOW_1) <= NEXUS_TE_DELTA * 2) {
        bits = (bits << 1) | 1;
      } else {
        ok = false;
        break;
      }
    }
    if(!ok) continue;

    // "const" nibble must be 0xF - the only integrity check this frame format has
    if(((bits >> 8) & 0x0F) != 0x0F) continue;

    id = (bits >> 28) & 0xFF;
    batteryLow = !((bits >> 27) & 0x01);
    bool negative = (bits >> 23) & 0x01;
    int magnitude = (bits >> 12) & 0x07FF;
    tempC = negative ? -(float)(((~magnitude) & 0x07FF) + 1) / 10.0 : (float)magnitude / 10.0;
    humidity = bits & 0xFF;

    return true;
  }

  return false;
}

void classifyWeather() {
  if(lastWeatherTempC >= 30.0) {
    weatherCondition = WEATHER_HOT;
  } else if(lastWeatherHumidity != 0xFF && lastWeatherHumidity >= 80) {
    weatherCondition = WEATHER_RAIN;
  } else if(lastWeatherTempC >= 20.0 && (lastWeatherHumidity == 0xFF || lastWeatherHumidity < 50)) {
    weatherCondition = WEATHER_SUN;
  } else {
    weatherCondition = WEATHER_CLOUDY;
  }
}

#define GTWT02_FRAME_BITS 37
#define GTWT02_TE_SHORT 500
#define GTWT02_TE_LONG 2000
#define GTWT02_TE_SYNC 9000 // te_short * 18
#define GTWT02_TE_DELTA 200

// GT-WT-02 (ALDI Globaltronics, Lidl Auriol and clones) - ported from
// weather_station/protocols/gt_wt_02.c. Same PPM shape as Nexus (constant-width
// HIGH mark, LOW gap carries the bit) but its own sync width, bit layout, and an
// actual checksum (6-bit sum, modulo 64) rather than a fixed validity nibble.
bool decodeGtWt02Weather(unsigned long *samples, int count, float &tempC, int &humidity, uint8_t &id, bool &batteryLow) {
  for(int start = 1; start + GTWT02_FRAME_BITS * 2 <= count; start += 2) {
    if(abs((long)samples[start] - GTWT02_TE_SYNC) > GTWT02_TE_DELTA * 4) continue;

    uint64_t bits = 0;
    bool ok = true;
    for(int b = 0; b < GTWT02_FRAME_BITS; b++) {
      unsigned long high = samples[start + 1 + b * 2];
      unsigned long low = samples[start + 2 + b * 2];

      if(abs((long)high - GTWT02_TE_SHORT) > GTWT02_TE_DELTA) {
        ok = false;
        break;
      }
      if(abs((long)low - GTWT02_TE_LONG) <= GTWT02_TE_DELTA * 2) {
        bits = (bits << 1) | 0;
      } else if(abs((long)low - GTWT02_TE_LONG * 2) <= GTWT02_TE_DELTA * 4) {
        bits = (bits << 1) | 1;
      } else {
        ok = false;
        break;
      }
    }
    if(!ok) continue;

    uint8_t sum = (bits >> 5) & 0x0E;
    uint64_t tempData = bits >> 9;
    for(int i = 0; i < 7; i++) sum += (tempData >> (i * 4)) & 0x0F;
    if((uint8_t)(bits & 0x3F) != (sum & 0x3F)) continue; // checksum mismatch

    id = (bits >> 29) & 0xFF;
    batteryLow = (bits >> 28) & 0x01;
    bool negative = (bits >> 24) & 0x01;
    int magnitude = (bits >> 13) & 0x07FF;
    tempC = negative ? -(float)(((~magnitude) & 0x07FF) + 1) / 10.0 : (float)magnitude / 10.0;
    humidity = (bits >> 6) & 0x7F;
    if(humidity <= 10) humidity = 0;       // sensor sends 10 below its 20% working range
    else if(humidity > 90) humidity = 100; // and 110 above its 90% working range

    return true;
  }
  return false;
}

#define BRESSER_FRAME_BITS 40
#define BRESSER_TE_SHORT 250
#define BRESSER_TE_LONG 500
#define BRESSER_TE_DELTA 150

// Bresser 3CH / Renkforce DM-7511 - ported from
// weather_station/protocols/bresser_3ch.c. Real PWM (mark length carries the bit,
// not the gap). No distinct sync marker simple enough to anchor on reliably from a
// single capture, so this tries every possible bit-aligned offset and relies on the
// real 8-bit byte-sum checksum to reject anything that isn't actually a valid frame.
bool decodeBresserWeather(unsigned long *samples, int count, float &tempC, int &humidity, uint8_t &id, bool &batteryLow) {
  for(int start = 0; start + BRESSER_FRAME_BITS * 2 <= count; start += 2) {
    uint64_t bits = 0;
    bool ok = true;
    for(int b = 0; b < BRESSER_FRAME_BITS; b++) {
      unsigned long high = samples[start + b * 2];
      unsigned long low = samples[start + 1 + b * 2];
      bool bitVal;
      if(abs((long)high - BRESSER_TE_SHORT) <= BRESSER_TE_DELTA && abs((long)low - BRESSER_TE_LONG) <= BRESSER_TE_DELTA) {
        bitVal = 0;
      } else if(abs((long)high - BRESSER_TE_LONG) <= BRESSER_TE_DELTA && abs((long)low - BRESSER_TE_SHORT) <= BRESSER_TE_DELTA) {
        bitVal = 1;
      } else {
        ok = false;
        break;
      }
      bits = (bits << 1) | bitVal;
    }
    if(!ok) continue;

    uint8_t sum = ((bits >> 32) & 0xFF) + ((bits >> 24) & 0xFF) + ((bits >> 16) & 0xFF) + ((bits >> 8) & 0xFF);
    if((bits & 0xFF) != sum) continue; // checksum mismatch

    id = (bits >> 32) & 0xFF;
    batteryLow = (bits >> 31) & 0x01;
    int rawTemp = (bits >> 16) & 0x0FFF;
    float tempF = (rawTemp - 900) / 10.0; // encoded as Fahrenheit, offset 90, scaled 10
    tempC = (tempF - 32.0) * 5.0 / 9.0;
    humidity = (bits >> 8) & 0xFF;

    return true;
  }
  return false;
}

#define ACURITE606_FRAME_BITS 32
#define ACURITE606_TE_SHORT 500
#define ACURITE606_TE_LONG 2000
#define ACURITE606_TE_SYNC 8500 // te_short * 17
#define ACURITE606_TE_DELTA 200

// Acurite-606TX - ported from weather_station/protocols/acurite_606tx.c. Same PPM
// shape as Nexus/GT-WT-02, validated against its real LFSR-8 checksum (a standard,
// well-documented digest function, ported as-is from blocks/math.c) rather than a
// fixed validity nibble or plain sum.
bool decodeAcurite606Weather(unsigned long *samples, int count, float &tempC, int &humidity, uint8_t &id, bool &batteryLow) {
  for(int start = 1; start + ACURITE606_FRAME_BITS * 2 <= count; start += 2) {
    if(abs((long)samples[start] - ACURITE606_TE_SYNC) > ACURITE606_TE_DELTA * 4) continue;

    uint64_t bits = 0;
    bool ok = true;
    for(int b = 0; b < ACURITE606_FRAME_BITS; b++) {
      unsigned long high = samples[start + 1 + b * 2];
      unsigned long low = samples[start + 2 + b * 2];

      if(abs((long)high - ACURITE606_TE_SHORT) > ACURITE606_TE_DELTA) {
        ok = false;
        break;
      }
      if(abs((long)low - ACURITE606_TE_LONG) <= ACURITE606_TE_DELTA * 2) {
        bits = (bits << 1) | 0;
      } else if(abs((long)low - ACURITE606_TE_LONG * 2) <= ACURITE606_TE_DELTA * 4) {
        bits = (bits << 1) | 1;
      } else {
        ok = false;
        break;
      }
    }
    if(!ok) continue;

    uint8_t msg[3] = {
      (uint8_t)(bits >> 24),
      (uint8_t)(bits >> 16),
      (uint8_t)(bits >> 8)
    };
    uint8_t sum = 0;
    uint8_t key = 0xF1;
    for(int byteIdx = 0; byteIdx < 3; byteIdx++) {
      uint8_t data = msg[byteIdx];
      for(int i = 7; i >= 0; i--) {
        if((data >> i) & 1) sum ^= key;
        key = (key & 1) ? ((key >> 1) ^ 0x98) : (key >> 1);
      }
    }
    if(sum != (bits & 0xFF)) continue; // checksum mismatch

    id = (bits >> 24) & 0xFF;
    batteryLow = (bits >> 23) & 0x01;
    bool negative = (bits >> 19) & 0x01;
    int magnitude = (bits >> 8) & 0x07FF;
    tempC = negative ? -(float)(((~magnitude) & 0x07FF) + 1) / 10.0 : (float)magnitude / 10.0;
    humidity = 0xFF; // Acurite-606TX doesn't send humidity

    return true;
  }
  return false;
}

// Called from RX.ino right after a raw capture finishes, while weatherListening is active
void tryDecodeWeather() {
  float tempC;
  int humidity;
  uint8_t id;
  bool batteryLow;
  const char* protoName;

  if(decodeNexusWeather(sample, samplecount, tempC, humidity, id, batteryLow)) {
    protoName = "Nexus-TH";
  } else if(decodeGtWt02Weather(sample, samplecount, tempC, humidity, id, batteryLow)) {
    protoName = "GT-WT-02";
  } else if(decodeBresserWeather(sample, samplecount, tempC, humidity, id, batteryLow)) {
    protoName = "Bresser-3CH";
  } else if(decodeAcurite606Weather(sample, samplecount, tempC, humidity, id, batteryLow)) {
    protoName = "Acurite-606TX";
  } else {
    Serial.println(F("[Weather] Capture did not decode as a known weather-sensor frame"));
    return;
  }

  lastWeatherTempC = tempC;
  lastWeatherHumidity = humidity;
  lastWeatherSensorId = id;
  lastWeatherBatteryLow = batteryLow;
  lastWeatherPacketTime = millis();
  weatherPacketCount++;

  classifyWeather();

  Serial.printf("[Weather] %s sensor 0x%02X: %.1f C, %d%% RH, battery %s -> condition %d\n",
                protoName, id, tempC, humidity == 0xFF ? -1 : humidity, batteryLow ? "LOW" : "OK", (int)weatherCondition);
}

void startWeatherStation() {
  Serial.println(F("\n[Weather] ========== STARTING WEATHER STATION =========="));

  frequency = 433.92; // common frequency for Nexus-style sensors
  mod = 2;            // ASK/OOK

  weatherListening = true;
  weatherCondition = WEATHER_NONE;
  weatherPacketCount = 0;
  lastWeatherTempC = 0;
  lastWeatherHumidity = -1;
  lastWeatherSensorId = 0;
  lastWeatherBatteryLow = false;
  lastWeatherPacketTime = 0;

  currentMenu = MENU_WEATHER;
  startRX(); // arms CC1101 + raw capture; sets pixelMode = PIXEL_RX
  pixelMode = PIXEL_WEATHER;
}

void stopWeatherStation() {
  Serial.println(F("[Weather] Stopping weather station"));

  weatherListening = false;
  stopRX(); // shuts down CC1101/SPI, shows an RX stats summary, sets currentMenu = MENU_RX

  currentMenu = MENU_MAIN;
  menuSelection = 0;
  menuOffset = 0;
  pixelMode = PIXEL_MENU;
  updateDisplay();
}

void handleWeatherMode() {
  if(!rxActive) return;

  if(millis() - lastRXRefresh > 5000) {
    lastRXRefresh = millis();
    refreshRXState();
  }

  updateWaterfall(); // drives signal detection + raw capture + tryDecodeWeather()
  drawWeatherMenu();
  weatherPixelEffect();
  pixels.show();
}

void drawWeatherMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("=[ WEATHER STATION ]="));
  display.drawLine(0, 9, 127, 9, SH110X_WHITE);

  display.setCursor(0, 14);
  display.printf("Packets: %d\n", weatherPacketCount);

  if(weatherPacketCount == 0) {
    display.setCursor(0, 30);
    display.println(F("Listening..."));
    display.printf("%.2f MHz OOK\n", frequency);
  } else {
    display.setCursor(0, 26);
    display.printf("Sensor: 0x%02X\n", lastWeatherSensorId);
    display.printf("Temp:   %.1f C\n", lastWeatherTempC);
    if(lastWeatherHumidity != 0xFF) {
      display.printf("RH:     %d%%\n", lastWeatherHumidity);
    } else {
      display.println(F("RH:     n/a"));
    }

    display.setCursor(0, 54);
    switch(weatherCondition) {
      case WEATHER_HOT:    display.print(F("Condition: HOT"));    break;
      case WEATHER_RAIN:   display.print(F("Condition: RAIN"));   break;
      case WEATHER_SUN:    display.print(F("Condition: SUN"));    break;
      case WEATHER_CLOUDY: display.print(F("Condition: CLOUDY")); break;
      default: break;
    }

    if(lastWeatherBatteryLow) {
      display.setCursor(100, 0);
      display.print(F("LOW"));
    }
  }

  display.display();
}

void weatherListeningEffect() {
  static float phase = 0;
  float breath = (sin(phase * 0.0174533) + 1.0) / 2.0;
  uint8_t brightness = 10 + breath * 40;
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(brightness, brightness, brightness));
  }
  phase += 2;
  if(phase >= 360) phase = 0;
}

void weatherHotEffect() {
  static float phase = 0;
  float breath = (sin(phase * 0.0174533) + 1.0) / 2.0;
  uint8_t brightness = 80 + breath * 175;
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(brightness, 0, 0));
  }
  phase += 4;
  if(phase >= 360) phase = 0;
}

void weatherSunEffect() {
  static float phase = 0;
  float breath = (sin(phase * 0.0174533) + 1.0) / 2.0;
  uint8_t brightness = 100 + breath * 155;
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(brightness, (uint8_t)(brightness * 0.75), 0));
  }
  phase += 3;
  if(phase >= 360) phase = 0;
}

void weatherRainEffect() {
  // Deep-blue background with a few brighter "raindrops" trickling along the strip
  static unsigned long lastStep = 0;
  static uint8_t dropPos[4] = {0, 8, 16, 24};

  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 40));
  }

  if(millis() - lastStep > 60) {
    lastStep = millis();
    for(int d = 0; d < 4; d++) {
      dropPos[d] = (dropPos[d] + 1) % NEOPIXEL_COUNT;
    }
  }

  for(int d = 0; d < 4; d++) {
    pixels.setPixelColor(dropPos[d], pixels.Color(30, 60, 255));
  }
}

void weatherCloudyEffect() {
  // Soft grey/white blobs slowly drifting across the strip, like clouds
  static float phase = 0;

  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    float wave = sin((i * 0.3) + phase * 0.0174533);
    uint8_t brightness = 30 + (uint8_t)((wave + 1.0) / 2.0 * 90);
    pixels.setPixelColor(i, pixels.Color(brightness, brightness, brightness + 10));
  }

  phase += 1.5;
  if(phase >= 360) phase = 0;
}

void weatherPixelEffect() {
  switch(weatherCondition) {
    case WEATHER_HOT:
      weatherHotEffect();
      break;
    case WEATHER_RAIN:
      weatherRainEffect();
      break;
    case WEATHER_SUN:
      weatherSunEffect();
      break;
    case WEATHER_CLOUDY:
      weatherCloudyEffect();
      break;
    case WEATHER_NONE:
    default:
      weatherListeningEffect();
      break;
  }
}
