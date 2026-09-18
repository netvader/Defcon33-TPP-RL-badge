// Weather.ino - "Weather Station" mode
//
// Inspired by the Flipper Zero weather_station FAP
// (https://github.com/flipperdevices/flipperzero-good-faps/tree/dev/weather_station):
// listens for 433MHz OOK temperature/humidity sensors and reacts with the badge's
// NeoPixels once a reading comes in - blue-ish for rain (high humidity), yellow for
// sun, red for too hot, and a soft grey "cloud" animation for anything in between.
//
// This decodes ONE protocol family - "Nexus"-style sensors (Nexus, Solight TE82S,
// Rubicson 1444 and various clones), which is one of the simpler ones the Flipper
// app supports. It reuses the raw pulse-timing capture already wired up in RX.ino
// (see beginRawCapture()/rxEdgeISR()) rather than re-implementing signal capture.
//
// IMPORTANT: the exact Nexus bit-timings/field layout below are taken from public
// protocol write-ups (e.g. rtl_433's nexus.c), not verified against a real sensor -
// I had no badge or sensor to test this against. The bit decoder classifies each
// pulse as short/long relative to the capture's OWN average width rather than fixed
// microsecond constants, which should make it more tolerant of clone-to-clone clock
// differences, but the field layout (id/battery/channel/temp/humidity bit positions)
// may need adjusting against a real sensor if it doesn't decode anything useful.

bool weatherListening = false;
WeatherCondition weatherCondition = WEATHER_NONE;
float lastWeatherTempC = 0;
int lastWeatherHumidity = -1;
uint8_t lastWeatherSensorId = 0;
bool lastWeatherBatteryLow = false;
int weatherPacketCount = 0;
unsigned long lastWeatherPacketTime = 0;

#define NEXUS_FRAME_BITS 36

// samples[] holds alternating HIGH,LOW pulse durations in microseconds (starting
// with a HIGH), exactly as captured by RX.ino's rxEdgeISR(). Nexus-style sensors are
// PWM-encoded: each data bit is one HIGH pulse whose width is either "short" (a 0)
// or "long" (a 1), followed by a roughly fixed LOW gap.
bool decodeNexusWeather(unsigned long *samples, int count, float &tempC, int &humidity, uint8_t &id, bool &batteryLow) {
  if(count < NEXUS_FRAME_BITS * 2) return false;

  unsigned long highs[SAMPLE_SIZE / 2];
  int highCount = 0;
  for(int i = 0; i < count && highCount < (int)(sizeof(highs) / sizeof(highs[0])); i += 2) {
    highs[highCount++] = samples[i];
  }
  if(highCount < NEXUS_FRAME_BITS) return false;

  // Calibrate the short/long threshold from the back half of the capture - a sync
  // preamble (short pulses), if present, sits at the start and would skew a plain average
  int calStart = highCount / 2;
  unsigned long sum = 0;
  for(int i = calStart; i < highCount; i++) sum += highs[i];
  unsigned long avg = sum / (highCount - calStart);
  if(avg == 0) return false;

  // Skip a leading sync preamble: pulses much shorter than the calibrated average
  int startIdx = 0;
  while(startIdx < highCount - NEXUS_FRAME_BITS && highs[startIdx] < (avg / 2)) startIdx++;

  if(highCount - startIdx != NEXUS_FRAME_BITS) return false;

  uint64_t bits = 0;
  for(int i = 0; i < NEXUS_FRAME_BITS; i++) {
    bits = (bits << 1) | (highs[startIdx + i] > avg ? 1 : 0);
  }

  // Assumed 36-bit layout, MSB first: id(8) battery(1) unknown(1) channel(2) temp(12) humidity(8) pad(4)
  id = (bits >> 28) & 0xFF;
  batteryLow = (bits >> 27) & 0x01;
  int tempRaw = (bits >> 12) & 0x0FFF;
  if(tempRaw & 0x0800) tempRaw -= 0x1000; // sign-extend 12-bit value
  tempC = tempRaw / 10.0;
  humidity = (bits >> 4) & 0xFF;

  // No real CRC exists for this frame family (real receivers rely on 3x repeats and
  // majority-vote, which a single captured frame can't do) - reject anything outside
  // plausible weather-sensor ranges as a basic sanity filter instead
  if(tempC < -40 || tempC > 60) return false;
  if(humidity != 0xFF && humidity > 100) return false;

  return true;
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

// Called from RX.ino right after a raw capture finishes, while weatherListening is active
void tryDecodeWeather() {
  float tempC;
  int humidity;
  uint8_t id;
  bool batteryLow;

  if(!decodeNexusWeather(sample, samplecount, tempC, humidity, id, batteryLow)) {
    Serial.println(F("[Weather] Capture did not decode as a Nexus-style weather frame"));
    return;
  }

  lastWeatherTempC = tempC;
  lastWeatherHumidity = humidity;
  lastWeatherSensorId = id;
  lastWeatherBatteryLow = batteryLow;
  lastWeatherPacketTime = millis();
  weatherPacketCount++;

  classifyWeather();

  Serial.printf("[Weather] Sensor 0x%02X: %.1f C, %d%% RH, battery %s -> condition %d\n",
                id, tempC, humidity == 0xFF ? -1 : humidity, batteryLow ? "LOW" : "OK", (int)weatherCondition);
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
