// Pixels.ino - NeoPixel LED animation functions

// Animation variables
unsigned long lastPixelUpdate = 0;
uint8_t pixelHue = 0;
uint8_t fireAnimation[NEOPIXEL_COUNT];
uint8_t breathePhase = 0;
int8_t breatheDirection = 1;
bool lavaInit = false;

// Pattern-specific variables
uint8_t plasmaTime = 0;
uint8_t wavePosition = 0;
float sparkleDecay[NEOPIXEL_COUNT];
uint8_t meteorPosition = 0;
uint8_t twinkleState[NEOPIXEL_COUNT];
uint8_t stripeOffset = 0;
uint8_t theaterChaseQ = 0;
uint8_t runningLightsPos = 0;
float bouncingBallPos[3] = {0, 10, 20};
float bouncingBallVel[3] = {1.0, 1.5, 2.0};
uint8_t heartbeatPhase = 0;
uint8_t strobeCount = 0;
uint8_t cylonPos = 0;
int8_t cylonDir = 1;
uint8_t snowSparkleTime = 0;
uint8_t pacificaTime = 0;
uint8_t lavaHeat[NEOPIXEL_COUNT] = {0};  // Initialize to ensure it's not garbage

// Helper function declarations
uint8_t qsub8(uint8_t x, uint8_t b);
uint8_t qadd8(uint8_t i, uint8_t j);
int8_t random8(uint8_t min = 0, uint8_t max = 255);

// Forward declarations for animations
void breatheAnimation(uint8_t r, uint8_t g, uint8_t b);
void rainbowCycle();
void scannerAnimation(uint8_t r, uint8_t g, uint8_t b);
void sparkleAnimation();
void fireEffect();
void oceanWave();
void matrixRain();
void superGayAnimation();
void solidRed();
void solidGreen();
void solidBlue();
void solidWhite();
void solidMagenta();
void solidCyan();
void solidYellow();
void solidOrange();
void neonPulse();
void plasmaAnimation();
void meteorRain();
void twinkleStars();
void vaporwave();
void policeStrobe();
void pirateShip();
void theaterChase();
void runningLights();
void bouncingBalls();
void heartbeat();
void strobeEffect();
void cylonBounce();
void snowSparkle();
void pacifica();
void lavaLamp();
void lightning();
void confetti();
void pulseAnimation(uint8_t r, uint8_t g, uint8_t b);
void chaosAnimation();
void lightningAnimation();

void updatePixels() {
  if (millis() - lastPixelUpdate < 20) return;
  lastPixelUpdate = millis();
  
  switch(pixelMode) {
    case PIXEL_OFF:
      pixels.clear();
      break;
      
    case PIXEL_IDLE:
      switch(idlePattern) {
        case PATTERN_BREATHE:
          breatheAnimation(0, 0, 255);
          break;
        case PATTERN_RAINBOW:
          rainbowCycle();
          break;
        case PATTERN_SCANNER:
          scannerAnimation(0, 0, 255);
          break;
        case PATTERN_SPARKLE:
          sparkleAnimation();
          break;
        case PATTERN_FIRE:
          fireEffect();
          break;
        case PATTERN_OCEAN:
          oceanWave();
          break;
        case PATTERN_MATRIX:
          matrixRain();
          break;
        case PATTERN_SUPERGAY:
          superGayAnimation();
          break;
        case PATTERN_SOLID_RED:
          solidRed();
          break;
        case PATTERN_SOLID_GREEN:
          solidGreen();
          break;
        case PATTERN_SOLID_BLUE:
          solidBlue();
          break;
        case PATTERN_SOLID_WHITE:
          solidWhite();
          break;
        case PATTERN_SOLID_MAGENTA:
          solidMagenta();
          break;
        case PATTERN_SOLID_CYAN:
          solidCyan();
          break;
        case PATTERN_SOLID_YELLOW:
          solidYellow();
          break;
        case PATTERN_SOLID_ORANGE:
          solidOrange();
          break;
        case PATTERN_NEON:
          neonPulse();
          break;
        case PATTERN_PLASMA:
          plasmaAnimation();
          break;
        case PATTERN_METEOR:
          meteorRain();
          break;
        case PATTERN_TWINKLE:
          twinkleStars();
          break;
        case PATTERN_VAPORWAVE:
          vaporwave();
          break;
        case PATTERN_POLICE:
          policeStrobe();
          break;
        case PATTERN_PIRATE:
          pirateShip();
          break;
        case PATTERN_THEATER:
          theaterChase();
          break;
        case PATTERN_RUNNING:
          runningLights();
          break;
        case PATTERN_BOUNCING:
          bouncingBalls();
          break;
        case PATTERN_HEARTBEAT:
          heartbeat();
          break;
        case PATTERN_STROBE:
          strobeEffect();
          break;
        case PATTERN_CYLON:
          cylonBounce();
          break;
        case PATTERN_SNOW:
          snowSparkle();
          break;
        case PATTERN_PACIFICA:
          pacifica();
          break;
        case PATTERN_LAVA:
          lavaLamp();
          break;
        case PATTERN_LIGHTNING:
          lightning();
          break;
        case PATTERN_CONFETTI:
          confetti();
          break;
        case PATTERN_OFF:
          pixels.clear();
          break;
      }
      break;
      
    case PIXEL_RX:
      scannerAnimation(0, 255, 0);
      break;
      
    case PIXEL_TX:
      pulseAnimation(255, 0, 0);
      break;
      
    case PIXEL_JAMMER:
      chaosAnimation();
      break;
      
    case PIXEL_TESLA:
      lightningAnimation();
      break;
      
    case PIXEL_MENU:
      rainbowCycle();
      break;

    case PIXEL_WEATHER:
      weatherPixelEffect();
      break;

    case PIXEL_FULLDUPLEX:
      fullDuplexPixelEffect();
      break;
  }
  
  pixels.show();
}

void breatheAnimation(uint8_t r, uint8_t g, uint8_t b) {
  float breath = (sin(breathePhase * 0.0174533) + 1.0) / 2.0;
  uint8_t brightness = 20 + (breath * 235);
  
  uint32_t color = pixels.Color((r * brightness) / 255, 
                                (g * brightness) / 255, 
                                (b * brightness) / 255);
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, color);
  }
  
  breathePhase += 3;
  if(breathePhase >= 360) breathePhase = 0;
}

void fireEffect() {
  // Cool down every cell a little
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    fireAnimation[i] = qsub8(fireAnimation[i], random8(0, ((55 * 10) / NEOPIXEL_COUNT) + 2));
  }
  
  // Heat from each cell drifts UP (higher index = up)
  for(int k = NEOPIXEL_COUNT - 1; k >= 2; k--) {
    fireAnimation[k] = (fireAnimation[k - 1] + fireAnimation[k - 2] + fireAnimation[k - 2]) / 3;
  }
  
  // Randomly ignite new sparks at the BOTTOM (index 0)
  if(random8() < 120) {
    int y = random8(0, 3);  // Only ignite at bottom 3 LEDs
    fireAnimation[y] = qadd8(fireAnimation[y], random8(160, 255));
  }
  
  // Convert heat to LED colors
  for(int j = 0; j < NEOPIXEL_COUNT; j++) {
    uint8_t heat = fireAnimation[j];
    
    if(heat < 85) {
      pixels.setPixelColor(j, pixels.Color(heat * 3, 0, 0));
    } else if(heat < 170) {
      pixels.setPixelColor(j, pixels.Color(255, (heat - 85) * 3, 0));
    } else {
      pixels.setPixelColor(j, pixels.Color(255, 255, (heat - 170) * 3));
    }
  }
}

void superGayAnimation() {
  static uint16_t gayOffset = 0;
  
  uint32_t prideColors[] = {
    pixels.Color(255, 0, 0),
    pixels.Color(255, 127, 0),
    pixels.Color(255, 255, 0),
    pixels.Color(0, 255, 0),
    pixels.Color(0, 0, 255),
    pixels.Color(75, 0, 130),
    pixels.Color(148, 0, 211),
    pixels.Color(255, 20, 147)
  };
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    int colorIndex = ((i * 8 / NEOPIXEL_COUNT) + (gayOffset / 20)) % 8;
    pixels.setPixelColor(i, prideColors[colorIndex]);
    
    if(random(100) < 10) {
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    }
  }
  
  gayOffset += 5;
}

// Solid colors
void solidRed() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 0, 0));
  }
}

void solidGreen() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 255, 0));
  }
}

void solidBlue() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 255));
  }
}

void solidWhite() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  }
}

void solidMagenta() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 0, 255));
  }
}

void solidCyan() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 255, 255));
  }
}

void solidYellow() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 0));
  }
}

void solidOrange() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 100, 0));
  }
}

void theaterChase() {
  pixels.clear();
  
  for(int i = 0; i < NEOPIXEL_COUNT; i += 3) {
    if(i + theaterChaseQ < NEOPIXEL_COUNT) {
      pixels.setPixelColor(i + theaterChaseQ, pixels.Color(255, 255, 255));
    }
  }
  
  theaterChaseQ++;
  if(theaterChaseQ >= 3) theaterChaseQ = 0;
}

void runningLights() {
  runningLightsPos++;
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    float level = sin((i + runningLightsPos) * 0.5) * 127 + 128;
    pixels.setPixelColor(i, pixels.Color(level, 0, 0));
  }
}

void bouncingBalls() {
  pixels.clear();
  
  uint32_t colors[] = {
    pixels.Color(255, 0, 0),
    pixels.Color(0, 255, 0),
    pixels.Color(0, 0, 255)
  };
  
  for(int i = 0; i < 3; i++) {
    bouncingBallPos[i] += bouncingBallVel[i];
    
    if(bouncingBallPos[i] >= NEOPIXEL_COUNT - 1 || bouncingBallPos[i] <= 0) {
      bouncingBallVel[i] = -bouncingBallVel[i];
    }
    
    int pos = round(bouncingBallPos[i]);
    if(pos >= 0 && pos < NEOPIXEL_COUNT) {
      pixels.setPixelColor(pos, colors[i]);
    }
  }
}

void heartbeat() {
  float val = (exp(sin(heartbeatPhase * 0.0174533)) - 0.36787944) * 108.0;
  uint8_t brightness = val;
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(brightness, 0, 0));
  }
  
  heartbeatPhase += 2;
  if(heartbeatPhase >= 360) heartbeatPhase = 0;
}

void strobeEffect() {
  if(strobeCount < 10) {
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    }
  } else {
    pixels.clear();
  }
  
  strobeCount++;
  if(strobeCount > 20) strobeCount = 0;
}

void cylonBounce() {
  pixels.clear();
  
  for(int i = 0; i < 5; i++) {
    int pos = cylonPos - (i * cylonDir);
    if(pos >= 0 && pos < NEOPIXEL_COUNT) {
      int brightness = 255 - (i * 50);
      pixels.setPixelColor(pos, pixels.Color(brightness, 0, 0));
    }
  }
  
  cylonPos += cylonDir;
  if(cylonPos >= NEOPIXEL_COUNT - 1 || cylonPos <= 0) {
    cylonDir = -cylonDir;
  }
}

void snowSparkle() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(10, 10, 10));
  }
  
  if(snowSparkleTime % 5 == 0) {
    int pos = random(NEOPIXEL_COUNT);
    pixels.setPixelColor(pos, pixels.Color(255, 255, 255));
  }
  
  snowSparkleTime++;
}

void pacifica() {
  static uint16_t sTime = 0;
  sTime++;
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    // Create waves of blues and greens
    uint8_t wave1 = (sin((i * 3 + sTime) * 0.1) + 1) * 100;
    uint8_t wave2 = (sin((i * 5 - sTime) * 0.05) + 1) * 50;
    
    uint8_t r = wave2 / 8;
    uint8_t g = wave1 / 4 + wave2 / 4;
    uint8_t b = wave1 + wave2;
    
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
}

void lavaLamp() {
  static bool lavaInit = false;
  
  // Initialize with some random heat values
  if(!lavaInit) {
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      lavaHeat[i] = random8(50, 150);  // Start with some heat
    }
    lavaInit = true;
  }
  
  // Cool down every cell just a tiny bit
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    if(lavaHeat[i] > 0) {
      lavaHeat[i] = qsub8(lavaHeat[i], random8(0, 2));  // Much slower cooling
    }
  }
  
  // Heat drifts up and diffuses
  for(int k = NEOPIXEL_COUNT - 1; k >= 2; k--) {
    lavaHeat[k] = (lavaHeat[k - 1] + lavaHeat[k - 2] + lavaHeat[k - 2]) / 3;
  }
  
  // Create new bubbles more frequently
  if(random8() < 80) {  // Increased chance
    int y = random8(NEOPIXEL_COUNT / 2);  // Start bubbles in lower half
    lavaHeat[y] = qadd8(lavaHeat[y], random8(180, 255));
  }
  
  // Add medium bubbles too
  if(random8() < 50) {
    int y = random8(NEOPIXEL_COUNT);
    lavaHeat[y] = qadd8(lavaHeat[y], random8(100, 180));
  }
  
  // Convert heat to LED colors
  for(int j = 0; j < NEOPIXEL_COUNT; j++) {
    uint8_t temperature = lavaHeat[j];
    
    if(temperature > 20) {  // Only show if there's some heat
      if(temperature < 85) {
        // Black to dark red
        pixels.setPixelColor(j, pixels.Color(temperature * 3, 0, 0));
      } else if(temperature < 170) {
        // Dark red to bright orange
        pixels.setPixelColor(j, pixels.Color(255, (temperature - 85) * 3, 0));
      } else {
        // Orange to yellow
        pixels.setPixelColor(j, pixels.Color(255, 200, (temperature - 170) * 3));
      }
    } else {
      // Very low heat = dark/black
      pixels.setPixelColor(j, pixels.Color(temperature, 0, 0));
    }
  }
}

void lightning() {
  static uint8_t flashCount = 0;
  static uint8_t dimmer = 1;
  
  if(flashCount == 0) {
    dimmer = 5;
    if(random8() < 16) {
      flashCount = random8(3, 12);
    }
  }
  
  pixels.clear();
  
  if(flashCount) {
    if(dimmer) {
      for(int i = 0; i < NEOPIXEL_COUNT; i++) {
        pixels.setPixelColor(i, pixels.Color(255, 255, 255));
      }
      dimmer--;
    } else {
      flashCount--;
      dimmer = 5;
    }
  } else {
    // Ambient blue between strikes
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      if(random8() < 2) {
        pixels.setPixelColor(i, pixels.Color(0, 0, 48));
      }
    }
  }
}

void confetti() {
  fadeToBlackBy(10);
  
  int pos = random16(NEOPIXEL_COUNT);
  uint8_t hue = random8();
  pixels.setPixelColor(pos, pixels.ColorHSV(hue * 256, 200, 255));
}

void neonPulse() {
  static uint8_t neonPhase = 0;
  static uint8_t colorMode = 0;
  
  float pulse = (sin(neonPhase * 0.0174533) + 1.0) / 2.0;
  
  uint32_t neonColors[] = {
    pixels.Color(255, 0, 255),
    pixels.Color(0, 255, 255),
    pixels.Color(255, 0, 128),
    pixels.Color(128, 0, 255)
  };
  
  uint32_t color1 = neonColors[colorMode % 4];
  uint32_t color2 = neonColors[(colorMode + 1) % 4];
  
  uint8_t r1 = (color1 >> 16) & 0xFF;
  uint8_t g1 = (color1 >> 8) & 0xFF;
  uint8_t b1 = color1 & 0xFF;
  
  uint8_t r2 = (color2 >> 16) & 0xFF;
  uint8_t g2 = (color2 >> 8) & 0xFF;
  uint8_t b2 = color2 & 0xFF;
  
  uint8_t r = r1 + (pulse * (r2 - r1));
  uint8_t g = g1 + (pulse * (g2 - g1));
  uint8_t b = b1 + (pulse * (b2 - b1));
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  
  neonPhase += 3;
  if(neonPhase >= 360) {
    neonPhase = 0;
    colorMode++;
  }
}

void plasmaAnimation() {
  plasmaTime++;
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    uint8_t x = i;
    
    int plasma = 128 + (127 * sin((x / 2.0) + (plasmaTime / 10.0)));
    plasma += 128 + (127 * sin((x / 3.0) - (plasmaTime / 8.0)));
    plasma += 128 + (127 * sin((x / 5.0) + (plasmaTime / 12.0)));
    plasma /= 3;
    
    uint8_t hue = map(plasma, 0, 255, 0, 255);
    pixels.setPixelColor(i, pixels.ColorHSV(hue * 256));
  }
}

void meteorRain() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    uint32_t oldColor = pixels.getPixelColor(i);
    uint8_t r = (oldColor >> 16) & 0xFF;
    uint8_t g = (oldColor >> 8) & 0xFF;
    uint8_t b = oldColor & 0xFF;
    
    r = (r * 240) / 256;
    g = (g * 240) / 256;
    b = (b * 240) / 256;
    
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  
  int meteorSize = 5;
  for(int j = 0; j < meteorSize; j++) {
    if((meteorPosition - j >= 0) && (meteorPosition - j < NEOPIXEL_COUNT)) {
      int brightness = 255 - (j * 40);
      pixels.setPixelColor(meteorPosition - j, pixels.Color(brightness, brightness, brightness));
    }
  }
  
  meteorPosition++;
  if(meteorPosition >= NEOPIXEL_COUNT + meteorSize) {
    meteorPosition = 0;
    
    if(random(10) < 3) {
      meteorPosition = -meteorSize * 2;
    }
  }
}

void twinkleStars() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    if(twinkleState[i] == 0) {
      if(random(100) < 5) {
        twinkleState[i] = 255;
      }
    } else {
      twinkleState[i] = (twinkleState[i] * 9) / 10;
      if(twinkleState[i] < 10) twinkleState[i] = 0;
    }
    
    uint8_t brightness = twinkleState[i];
    pixels.setPixelColor(i, pixels.Color(brightness, brightness, brightness * 0.8));
  }
}

void vaporwave() {
  static uint8_t vaporOffset = 0;
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    float wave = sin((i + vaporOffset) * 0.2);
    
    if(wave > 0.3) {
      pixels.setPixelColor(i, pixels.Color(255, 20, 147));
    } else if(wave > -0.3) {
      pixels.setPixelColor(i, pixels.Color(138, 43, 226));
    } else {
      pixels.setPixelColor(i, pixels.Color(0, 255, 255));
    }
    
    if(random(200) < 2) {
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    }
  }
  
  vaporOffset++;
}

void policeStrobe() {
  static uint8_t strobePhase = 0;
  static unsigned long lastStrobe = 0;
  
  if(millis() - lastStrobe > 50) {
    lastStrobe = millis();
    strobePhase++;
    
    pixels.clear();
    
    if(strobePhase % 8 < 2) {
      for(int i = 0; i < NEOPIXEL_COUNT / 2; i++) {
        pixels.setPixelColor(i, pixels.Color(255, 0, 0));
      }
    } else if(strobePhase % 8 >= 4 && strobePhase % 8 < 6) {
      for(int i = NEOPIXEL_COUNT / 2; i < NEOPIXEL_COUNT; i++) {
        pixels.setPixelColor(i, pixels.Color(0, 0, 255));
      }
    }
  }
}

void pirateShip() {
  static int shipPos = 0;
  static uint8_t wavePhase = 0;
  
  pixels.clear();
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    uint8_t waveHeight = 30 + (25 * sin((i * 0.3) + (wavePhase * 0.05)));
    pixels.setPixelColor(i, pixels.Color(0, 0, waveHeight));
  }
  
  if(shipPos >= 0 && shipPos < NEOPIXEL_COUNT) {
    pixels.setPixelColor(shipPos, pixels.Color(139, 0, 0));
    if(shipPos > 0) pixels.setPixelColor(shipPos - 1, pixels.Color(100, 0, 0));
    if(shipPos < NEOPIXEL_COUNT - 1) pixels.setPixelColor(shipPos + 1, pixels.Color(100, 0, 0));
    
    if(shipPos > 1) pixels.setPixelColor(shipPos - 2, pixels.Color(255, 255, 255));
    if(shipPos > 2) pixels.setPixelColor(shipPos - 3, pixels.Color(255, 0, 0));
  }
  
  for(int j = 1; j < 5; j++) {
    int wakePos = shipPos + j + 1;
    if(wakePos >= 0 && wakePos < NEOPIXEL_COUNT) {
      uint8_t wakeBright = 100 - (j * 20);
      pixels.setPixelColor(wakePos, pixels.Color(wakeBright, wakeBright, 255));
    }
  }
  
  shipPos--;
  if(shipPos < -10) shipPos = NEOPIXEL_COUNT + 10;
  
  wavePhase += 2;
}

void scannerAnimation(uint8_t r, uint8_t g, uint8_t b) {
  static int scanPos = 0;
  static int scanDir = 1;
  
  pixels.clear();
  
  pixels.setPixelColor(scanPos, pixels.Color(r, g, b));
  
  for(int i = 1; i <= 5; i++) {
    int brightness = 255 - (i * 40);
    if(brightness < 0) brightness = 0;
    
    int trailPos1 = scanPos - (i * scanDir);
    int trailPos2 = scanPos + (i * scanDir);
    
    if(trailPos1 >= 0 && trailPos1 < NEOPIXEL_COUNT) {
      pixels.setPixelColor(trailPos1, pixels.Color((r * brightness) / 255,
                                                   (g * brightness) / 255,
                                                   (b * brightness) / 255));
    }
    if(trailPos2 >= 0 && trailPos2 < NEOPIXEL_COUNT && i <= 2) {
      pixels.setPixelColor(trailPos2, pixels.Color((r * brightness) / 255,
                                                   (g * brightness) / 255,
                                                   (b * brightness) / 255));
    }
  }
  
  scanPos += scanDir;
  if(scanPos >= NEOPIXEL_COUNT - 1 || scanPos <= 0) {
    scanDir = -scanDir;
  }
}

void sparkleAnimation() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    if(sparkleDecay[i] > 0) {
      sparkleDecay[i] *= 0.9;
      if(sparkleDecay[i] < 1) sparkleDecay[i] = 0;
      
      uint8_t brightness = sparkleDecay[i];
      pixels.setPixelColor(i, pixels.Color(brightness, brightness, brightness));
    }
  }
  
  for(int j = 0; j < 3; j++) {
    if(random(100) < 30) {
      int pos = random(NEOPIXEL_COUNT);
      sparkleDecay[pos] = 255;
    }
  }
}

void oceanWave() {
  wavePosition += 2;
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    float wave1 = sin((i * 0.2) + (wavePosition * 0.02));
    float wave2 = sin((i * 0.3) + (wavePosition * 0.03));
    float combinedWave = (wave1 + wave2) / 2;
    
    uint8_t blue = 100 + (80 * combinedWave);
    uint8_t green = 20 + (40 * combinedWave);
    uint8_t white = 0;
    
    // Add foam on wave peaks
    if(combinedWave > 0.8) {
      white = (combinedWave - 0.8) * 255;
    }
    
    pixels.setPixelColor(i, pixels.Color(white, green + white, blue + white));
  }
}

void matrixRain() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    uint32_t color = pixels.getPixelColor(i);
    uint8_t g = (color >> 8) & 0xFF;
    
    if(g > 0) {
      g = (g * 230) / 256;
      pixels.setPixelColor(i, pixels.Color(0, g, 0));
    }
  }
  
  for(int j = 0; j < 3; j++) {
    if(random(10) < 4) {
      int pos = random(NEOPIXEL_COUNT);
      uint8_t brightness = random(128, 255);
      pixels.setPixelColor(pos, pixels.Color(0, brightness, 0));
    }
  }
}

void rainbowCycle() {
  pixelHue += 256;
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    int pixelHueOffset = (i * 65536L / NEOPIXEL_COUNT) + pixelHue;
    pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV(pixelHueOffset)));
  }
}

void pulseAnimation(uint8_t r, uint8_t g, uint8_t b) {
  static uint8_t pulsePhase = 0;
  
  pulsePhase += 5;
  
  uint8_t intensity = (sin(pulsePhase * 0.0174533) + 1) * 127;
  uint32_t color = pixels.Color((r * intensity) / 255,
                                (g * intensity) / 255,
                                (b * intensity) / 255);
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, color);
  }
}

void chaosAnimation() {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    if(random(10) < 3) {
      pixels.setPixelColor(i, pixels.Color(random(256), random(256), random(256)));
    } else {
      uint32_t oldColor = pixels.getPixelColor(i);
      uint8_t r = ((oldColor >> 16) & 0xFF) * 0.9;
      uint8_t g = ((oldColor >> 8) & 0xFF) * 0.9;
      uint8_t b = (oldColor & 0xFF) * 0.9;
      pixels.setPixelColor(i, pixels.Color(r, g, b));
    }
  }
}

void lightningAnimation() {
  static unsigned long lastStrike = 0;
  
  if(millis() - lastStrike > random(100, 500)) {
    lastStrike = millis();
    
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    }
    pixels.show();
    delay(50);
    
    for(int fade = 255; fade > 0; fade -= 15) {
      for(int i = 0; i < NEOPIXEL_COUNT; i++) {
        pixels.setPixelColor(i, pixels.Color(fade * 0.7, fade * 0.7, fade));
      }
      pixels.show();
      delay(10);
    }
  } else {
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      if(random(100) < 5) {
        uint8_t flicker = random(50, 150);
        pixels.setPixelColor(i, pixels.Color(0, 0, flicker));
      } else {
        pixels.setPixelColor(i, 0);
      }
    }
  }
}

void successAnimation() {
  Serial.println(F("[LED] Success animation"));
  
  for(int wave = 0; wave < 3; wave++) {
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      pixels.clear();
      for(int j = 0; j < 5; j++) {
        int pos = (i + j) % NEOPIXEL_COUNT;
        int brightness = 255 - (j * 50);
        pixels.setPixelColor(pos, pixels.Color(0, brightness, 0));
      }
      pixels.show();
      delay(30);
    }
  }
  pixels.clear();
  pixels.show();
}

void errorAnimation() {
  Serial.println(F("[LED] Error animation"));
  
  for(int flash = 0; flash < 5; flash++) {
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      pixels.setPixelColor(i, pixels.Color(255, 0, 0));
    }
    pixels.show();
    delay(100);
    pixels.clear();
    pixels.show();
    delay(100);
  }
}

void bootAnimation() {
  Serial.println(F("[LED] Boot animation"));
  
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    pixels.show();
    delay(30);
  }
  
  for(int fade = 255; fade > 0; fade -= 5) {
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      pixels.setPixelColor(i, pixels.Color(fade * 0.2, fade * 0.2, fade));
    }
    pixels.show();
    delay(10);
  }
  
  pixels.clear();
  pixels.show();
}

void signalStrengthLEDs(int rssi) {
  int ledsToLight = map(rssi, -120, -30, 0, NEOPIXEL_COUNT);
  ledsToLight = constrain(ledsToLight, 0, NEOPIXEL_COUNT);
  
  pixels.clear();
  
  for(int i = 0; i < ledsToLight; i++) {
    uint32_t color;
    if(i < NEOPIXEL_COUNT * 0.3) {
      color = pixels.Color(255, 0, 0);
    } else if(i < NEOPIXEL_COUNT * 0.7) {
      color = pixels.Color(255, 255, 0);
    } else {
      color = pixels.Color(0, 255, 0);
    }
    pixels.setPixelColor(i, color);
  }
  
  pixels.show();
}

void teslaSuccessAnimation() {
  Serial.println(F("[LED] Tesla success animation"));
  
  for(int flash = 0; flash < 5; flash++) {
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 50, 255));
    }
    pixels.show();
    delay(50);
    
    for(int i = 0; i < NEOPIXEL_COUNT; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 0, 50));
    }
    pixels.show();
    delay(50);
  }
  
  for(int r = 0; r < NEOPIXEL_COUNT/2; r++) {
    pixels.clear();
    pixels.setPixelColor(NEOPIXEL_COUNT/2 + r, pixels.Color(0, 0, 255));
    pixels.setPixelColor(NEOPIXEL_COUNT/2 - r, pixels.Color(0, 0, 255));
    if(r > 0) {
      pixels.setPixelColor(NEOPIXEL_COUNT/2 + r - 1, pixels.Color(0, 0, 128));
      pixels.setPixelColor(NEOPIXEL_COUNT/2 - r + 1, pixels.Color(0, 0, 128));
    }
    pixels.show();
    delay(30);
  }
  
  pixels.clear();
  pixels.show();
}

// Helper function implementations
uint8_t qsub8(uint8_t x, uint8_t b) {
  if(x < b) return 0;
  return x - b;
}

uint8_t qadd8(uint8_t i, uint8_t j) {
  unsigned int t = i + j;
  if(t > 255) return 255;
  return t;
}

int8_t random8(uint8_t min, uint8_t max) {
  if(min >= max) return min;
  return random(min, max + 1);
}

uint8_t sin8(uint8_t theta) {
  uint16_t angle = theta * 2;
  return (sin(angle * 0.0174533) + 1) * 127.5;
}

uint8_t cos8(uint8_t theta) {
  uint16_t angle = theta * 2;
  return (cos(angle * 0.0174533) + 1) * 127.5;
}

uint8_t scale8(uint8_t i, uint8_t scale) {
  return ((uint16_t)i * scale) >> 8;
}

void fadeToBlackBy(uint8_t fadeBy) {
  for(int i = 0; i < NEOPIXEL_COUNT; i++) {
    uint32_t oldColor = pixels.getPixelColor(i);
    uint8_t r = (oldColor >> 16) & 0xFF;
    uint8_t g = (oldColor >> 8) & 0xFF;
    uint8_t b = oldColor & 0xFF;
    
    r = scale8(r, 255 - fadeBy);
    g = scale8(g, 255 - fadeBy);
    b = scale8(b, 255 - fadeBy);
    
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
}

uint16_t random16(uint16_t max) {
  return random(max);
}