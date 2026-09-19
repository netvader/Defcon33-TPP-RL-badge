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
// All of the following are fixed-code PWM protocols (no rolling code) ported
// directly from the real encoders in flipperdevices/flipperzero-firmware
// (lib/subghz/protocols/*.c) rather than guessed at - each bit is a mark/space
// pair with fixed timing:
//   Princeton, Holtek, Ansonic, Hormann HSM, Legrand, Nice FLO (the fixed-code
//   "Nice Flo", not the rolling-code "Nice FloR-S"), GateTX, Dooya, Linear,
//   Magellan, LinearDelta3, Holtek_HT12X, SMC5326, Intertechno_V3, Mastercode,
//   BETT, Doitrand, Elplast, Nero Radio, Nero Sketch, Clemsa, Roger,
//   Dickert_MAHS, Feron, Honeywell.
// Legrand, Holtek_HT12X and SMC5326 read their own "TE:" field from the file
// (like Princeton does) instead of using a fixed built-in timing.
//
// Power Smart and Revers_RB2 use standard Manchester encoding instead of the
// mark/space PWM above (see buildManchesterPulses()) - the conversion itself is
// textbook Manchester, but I couldn't verify it matches Flipper's exact bit-polarity
// convention without hardware, so treat these two as lower-confidence than the rest.
//
// NOT implemented:
// - Somfy (Telis/Keytis): impossible without protocol-specific hardware knowledge -
//   even Flipper's own firmware can't send it (.yield = NULL in its encoder), since
//   Somfy RTS uses a real encrypted rolling code tied to the physical remote's own
//   secret counter. RAW playback of a captured Somfy signal still works, just not
//   "encode from a Key".
// - Hollarm, GangQi: their real encoders need a button/channel value neither
//   visibly reads from the file in the source I read, plus a checksum I couldn't
//   verify without a real remote - left out rather than guessing.
// - Chamberlain (chamberlain_code): real encoder branches on the dial-code length
//   (7/8/9 digits) with different checksum masks per length plus an unseen
//   bit-transform helper - too much unverified surface area to port faithfully.
// - MegaCode: each bit's gap width depends on BOTH the current and the previous
//   bit's value (not a fixed per-bit mark/space pair), built backwards in the real
//   encoder - too easy to get subtly wrong without hardware to check against.
// - Nice FloR-S, KeeLoq family (came, hormann's rolling variants, etc.), Security+,
//   etc.: each needs its own rolling-code/key-derivation scheme - out of scope here.
//
// Any Protocol: value not listed above is reported as unsupported rather than guessed at.
//
// The badge's CC1101 modules are Ebyte E07 units, which (per the E07-M1101D-SMA
// datasheet - the E07 doesn't have a single fixed range, it varies by exact SKU)
// only tune ~387-464 MHz. A .sub file's Frequency: is checked against that range
// before anything is sent - files for other bands (315MHz, 868MHz, 915MHz, etc.)
// are rejected rather than silently tuned to the nearest thing the hardware can do.

#define E07_FREQ_MIN_HZ 387000000UL
#define E07_FREQ_MAX_HZ 464000000UL

// How many times to repeat whichever .sub file gets sent - asked via a UP/DOWN
// prompt right after picking a file, instead of each protocol hardcoding its own
int subFileRepeatCount = 5;

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
  sendRawData(data_to_send, data_count, subFileRepeatCount); // fixed-code remotes are usually repeated several times
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
  sendRawData(data_to_send, data_count, subFileRepeatCount);
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
  sendRawData(data_to_send, data_count, subFileRepeatCount);
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
  sendRawData(data_to_send, data_count, subFileRepeatCount);
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
  sendRawData(data_to_send, data_count, subFileRepeatCount); // repeats are already baked into the buffer above
  return true;
}

// Nice Flo (fixed-code, te_short=700us, te_long=1400us) - same shape as Holtek/Ansonic
bool sendNiceFloFromKey(String keyHex, int bitCount) {
  const int te_short = 700, te_long = 1400;
  if(bitCount <= 0 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Nice Flo bit count in .sub file"));
    return false;
  }

  uint32_t code = parseHexKey(keyHex);

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

  Serial.printf("[TX] Nice Flo: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Gate TX (fixed-code, te_short=350us, te_long=700us, start bit is te_long not te_short)
bool sendGateTxFromKey(String keyHex, int bitCount) {
  const int te_short = 350, te_long = 700;
  if(bitCount <= 0 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Gate TX bit count in .sub file"));
    return false;
  }

  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te_long; // start bit (HIGH)
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

  Serial.printf("[TX] Gate TX: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Dooya (roller shutter motors, te_short=366us, te_long=733us, 40 bits) - the real
// header's exact length depends on data bit 0 (a minor rounding tweak); dropped here
// like the other protocols' pure-silence headers, starting directly at the start bit
bool sendDooyaFromKey(String keyHex, int bitCount) {
  const int te_short = 366, te_long = 733;
  if(bitCount <= 0 || bitCount > 40) {
    Serial.println(F("[TX] Invalid Dooya bit count in .sub file"));
    return false;
  }

  uint64_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te_short * 13; // start bit (HIGH)
  data_to_send[data_count++] = te_long * 2;   // LOW
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

  Serial.printf("[TX] Dooya: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Linear (fixed-code garage remotes, te_short=500us, te_long=1500us) - all bits
// except the last are a HIGH,LOW pair; the last bit sends only its HIGH mark
// followed by a long inter-frame gap instead of the usual LOW component
bool sendLinearFromKey(String keyHex, int bitCount) {
  const int te_short = 500, te_long = 1500;
  if(bitCount <= 1 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Linear bit count in .sub file"));
    return false;
  }

  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 1 && data_count < 1990; i--) {
    bool bitVal = (code >> i) & 0x01;
    if(bitVal) {
      data_to_send[data_count++] = te_long;  // HIGH
      data_to_send[data_count++] = te_short; // LOW
    } else {
      data_to_send[data_count++] = te_short; // HIGH
      data_to_send[data_count++] = te_long;  // LOW
    }
  }
  bool lastBit = code & 0x01;
  if(lastBit) {
    data_to_send[data_count++] = te_long;      // HIGH
    data_to_send[data_count++] = te_short * 42; // LOW gap
  } else {
    data_to_send[data_count++] = te_short;      // HIGH
    data_to_send[data_count++] = te_short * 44; // LOW gap
  }

  Serial.printf("[TX] Linear: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Magellan (fixed-code, te_short=200us, te_long=400us, 32 bits) - preamble of 12
// toggle pulses, then a 3-pulse start bit, the data, and a long stop-bit gap
bool sendMagellanFromKey(String keyHex, int bitCount) {
  const int te_short = 200, te_long = 400;
  if(bitCount <= 0 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Magellan bit count in .sub file"));
    return false;
  }

  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te_short * 4; // HIGH
  data_to_send[data_count++] = te_short;     // LOW
  for(int k = 0; k < 12 && data_count < 1970; k++) {
    data_to_send[data_count++] = te_short; // HIGH
    data_to_send[data_count++] = te_short; // LOW
  }
  data_to_send[data_count++] = te_short; // HIGH
  data_to_send[data_count++] = te_long;  // LOW
  data_to_send[data_count++] = te_long * 3; // HIGH (start bit)
  data_to_send[data_count++] = te_long;     // LOW
  for(int i = bitCount - 1; i >= 0 && data_count < 1990; i--) {
    bool bitVal = (code >> i) & 0x01;
    if(bitVal) {
      data_to_send[data_count++] = te_short; // HIGH
      data_to_send[data_count++] = te_long;  // LOW
    } else {
      data_to_send[data_count++] = te_long;  // HIGH
      data_to_send[data_count++] = te_short; // LOW
    }
  }
  data_to_send[data_count++] = te_short;      // HIGH (stop bit)
  data_to_send[data_count++] = te_long * 100; // LOW

  Serial.printf("[TX] Magellan: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// --- Batch 3: further simple fixed-code PWM protocols, all ported from the real
// encoders the same way as the ones above. Two (Power Smart, Revers_RB2) use
// standard Manchester encoding instead of mark/space PWM - see
// buildManchesterPulses() below; that conversion is generic/textbook, but I
// couldn't verify it matches Flipper's exact bit-polarity convention without
// hardware, so treat those two as lower-confidence than the rest.

// LinearDelta3 (te_short=500us, te_long=2000us, 8 bits)
bool sendLinearDelta3FromKey(String keyHex, int bitCount) {
  const int te_short = 500, te_long = 2000;
  if(bitCount <= 1 || bitCount > 32) {
    Serial.println(F("[TX] Invalid LinearDelta3 bit count in .sub file"));
    return false;
  }
  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 1 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_short * 7; }
    else  { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_long; }
  }
  bool lastBit = code & 0x01;
  if(lastBit) { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_short * 73; }
  else        { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short * 70; }

  Serial.printf("[TX] LinearDelta3: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Holtek HT12X (per-file "TE:", header=te*36, start bit=te, bit1=LOW te*2+HIGH te, bit0=LOW te+HIGH te*2)
bool sendHoltekHt12xFromKey(String keyHex, int bitCount, int te) {
  if(bitCount <= 0 || bitCount > 32 || te <= 0) {
    Serial.println(F("[TX] Invalid Holtek_HT12X parameters in .sub file"));
    return false;
  }
  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te; // start bit (HIGH)
  for(int i = bitCount - 1; i >= 0 && data_count < 1996; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te * 2; data_to_send[data_count++] = te; }
    else  { data_to_send[data_count++] = te;     data_to_send[data_count++] = te * 2; }
  }

  Serial.printf("[TX] Holtek_HT12X: %d bits, TE=%dus, code=0x%08lX\n", bitCount, te, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// SMC5326 (per-file "TE:", bit1=HIGH te*3+LOW te, bit0=HIGH te+LOW te*3, stop bit + guard)
bool sendSmc5326FromKey(String keyHex, int bitCount, int te) {
  if(bitCount <= 0 || bitCount > 32 || te <= 0) {
    Serial.println(F("[TX] Invalid SMC5326 parameters in .sub file"));
    return false;
  }
  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 0 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te * 3; data_to_send[data_count++] = te; }
    else  { data_to_send[data_count++] = te;     data_to_send[data_count++] = te * 3; }
  }
  data_to_send[data_count++] = te;      // stop bit (HIGH)
  data_to_send[data_count++] = te * 25; // guard (LOW)

  Serial.printf("[TX] SMC5326: %d bits, TE=%dus, code=0x%08lX\n", bitCount, te, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Intertechno_V3 (te_short=275us, te_long=1375us, 32 bits) - the real protocol has a
// "dimming" variant with a special bit at a fixed position for a different total bit
// count; not handled here, only the plain on/off frame encoding
bool sendIntertechnoV3FromKey(String keyHex, int bitCount) {
  const int te_short = 275, te_long = 1375;
  if(bitCount <= 0 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Intertechno_V3 bit count in .sub file"));
    return false;
  }
  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te_short;      // header (HIGH)
  data_to_send[data_count++] = te_short * 38; // LOW
  data_to_send[data_count++] = te_short;      // sync (HIGH)
  data_to_send[data_count++] = te_short * 10; // LOW
  for(int i = bitCount - 1; i >= 0 && data_count < 1980; i--) {
    bool b = (code >> i) & 0x01;
    if(b) {
      data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long;
      data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_short;
    } else {
      data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_short;
      data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long;
    }
  }

  Serial.printf("[TX] Intertechno_V3: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Mastercode (te_short=1072us, te_long=2145us, 36 bits)
bool sendMastercodeFromKey(String keyHex, int bitCount) {
  const int te_short = 1072, te_long = 2145;
  if(bitCount <= 1 || bitCount > 40) {
    Serial.println(F("[TX] Invalid Mastercode bit count in .sub file"));
    return false;
  }
  uint64_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 1 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short; }
    else  { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long; }
  }
  bool lastBit = code & 0x01;
  if(lastBit) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short + te_short * 13; }
  else        { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long + te_short * 13; }

  Serial.printf("[TX] Mastercode: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// BETT (te_short=340us, te_long=2000us, 18 bits)
bool sendBettFromKey(String keyHex, int bitCount) {
  const int te_short = 340, te_long = 2000;
  if(bitCount <= 1 || bitCount > 32) {
    Serial.println(F("[TX] Invalid BETT bit count in .sub file"));
    return false;
  }
  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 1 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short; }
    else  { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long; }
  }
  bool lastBit = code & 0x01;
  if(lastBit) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short + te_long * 7; }
  else        { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long + te_long * 7; }

  Serial.printf("[TX] BETT: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Doitrand (te_short=400us, te_long=1100us, 37 bits)
bool sendDoitrandFromKey(String keyHex, int bitCount) {
  const int te_short = 400, te_long = 1100;
  if(bitCount <= 0 || bitCount > 40) {
    Serial.println(F("[TX] Invalid Doitrand bit count in .sub file"));
    return false;
  }
  uint64_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te_short * 2 - 100; // start bit (HIGH)
  for(int i = bitCount - 1; i >= 0 && data_count < 1996; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short; }
    else  { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long; }
  }

  Serial.printf("[TX] Doitrand: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Elplast (te_short=230us, te_long=1550us, 18 bits)
bool sendElplastFromKey(String keyHex, int bitCount) {
  const int te_short = 230, te_long = 1550;
  if(bitCount <= 0 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Elplast bit count in .sub file"));
    return false;
  }
  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 0 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    bool isLast = (i == 0);
    if(b) {
      data_to_send[data_count++] = te_long;
      data_to_send[data_count++] = isLast ? (te_long * 8) : te_short;
    } else {
      data_to_send[data_count++] = te_short;
      data_to_send[data_count++] = isLast ? (te_long * 8) : te_long;
    }
  }

  Serial.printf("[TX] Elplast: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Nero Radio (te_short=200us, te_long=400us, 56 bits) - 49x header toggle, start bit, data
bool sendNeroRadioFromKey(String keyHex, int bitCount) {
  const int te_short = 200, te_long = 400;
  if(bitCount <= 1 || bitCount > 64) {
    Serial.println(F("[TX] Invalid Nero Radio bit count in .sub file"));
    return false;
  }
  uint64_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int k = 0; k < 49 && data_count < 1900; k++) {
    data_to_send[data_count++] = te_short; // HIGH
    data_to_send[data_count++] = te_short; // LOW
  }
  data_to_send[data_count++] = te_short * 4; // start bit (HIGH)
  data_to_send[data_count++] = te_short;     // LOW
  for(int i = bitCount - 1; i >= 1 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short; }
    else  { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long; }
  }
  bool lastBit = code & 0x01;
  if(lastBit) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short * 37; }
  else        { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_short * 37; }

  Serial.printf("[TX] Nero Radio: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Nero Sketch (te_short=330us, te_long=660us, 40 bits) - 47x header toggle, start/stop bits
bool sendNeroSketchFromKey(String keyHex, int bitCount) {
  const int te_short = 330, te_long = 660;
  if(bitCount <= 0 || bitCount > 40) {
    Serial.println(F("[TX] Invalid Nero Sketch bit count in .sub file"));
    return false;
  }
  uint64_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int k = 0; k < 47 && data_count < 1900; k++) {
    data_to_send[data_count++] = te_short; // HIGH
    data_to_send[data_count++] = te_short; // LOW
  }
  data_to_send[data_count++] = te_short * 4; // start bit (HIGH)
  data_to_send[data_count++] = te_short;     // LOW
  for(int i = bitCount - 1; i >= 0 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short; }
    else  { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long; }
  }
  data_to_send[data_count++] = te_short * 3; // stop bit (HIGH)
  data_to_send[data_count++] = te_short;     // LOW

  Serial.printf("[TX] Nero Sketch: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Clemsa (te_short=385us, te_long=2695us, 18 bits)
bool sendClemsaFromKey(String keyHex, int bitCount) {
  const int te_short = 385, te_long = 2695;
  if(bitCount <= 1 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Clemsa bit count in .sub file"));
    return false;
  }
  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 1 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short; }
    else  { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long; }
  }
  bool lastBit = code & 0x01;
  if(lastBit) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short + te_long * 7; }
  else        { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long + te_long * 7; }

  Serial.printf("[TX] Clemsa: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Roger (te_short=500us, te_long=1000us, 28 bits)
bool sendRogerFromKey(String keyHex, int bitCount) {
  const int te_short = 500, te_long = 1000;
  if(bitCount <= 0 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Roger bit count in .sub file"));
    return false;
  }
  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 0 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    bool isLast = (i == 0);
    if(b) {
      data_to_send[data_count++] = te_long;
      data_to_send[data_count++] = isLast ? (te_short * 19) : te_short;
    } else {
      data_to_send[data_count++] = te_short;
      data_to_send[data_count++] = isLast ? (te_short * 19) : te_long;
    }
  }

  Serial.printf("[TX] Roger: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Dickert_MAHS (te_short=400us, te_long=800us, 36 bits)
bool sendDickertMahsFromKey(String keyHex, int bitCount) {
  const int te_short = 400, te_long = 800;
  if(bitCount <= 0 || bitCount > 40) {
    Serial.println(F("[TX] Invalid Dickert_MAHS bit count in .sub file"));
    return false;
  }
  uint64_t code = parseHexKey(keyHex);

  data_count = 0;
  data_to_send[data_count++] = te_short; // start bit (HIGH)
  for(int i = bitCount - 1; i >= 0 && data_count < 1996; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short; }
    else  { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long; }
  }

  Serial.printf("[TX] Dickert_MAHS: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Feron (te_short=350us, te_long=750us, 32 bits)
bool sendFeronFromKey(String keyHex, int bitCount) {
  const int te_short = 350, te_long = 750;
  if(bitCount <= 0 || bitCount > 32) {
    Serial.println(F("[TX] Invalid Feron bit count in .sub file"));
    return false;
  }
  uint32_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 0 && data_count < 1980; i--) {
    bool b = (code >> i) & 0x01;
    bool isLast = (i == 0);
    if(b) {
      data_to_send[data_count++] = te_long;
      if(isLast) {
        data_to_send[data_count++] = te_short + 150;
        data_to_send[data_count++] = te_short + 150;
        data_to_send[data_count++] = te_long * 6;
      } else {
        data_to_send[data_count++] = te_short;
      }
    } else {
      data_to_send[data_count++] = te_short;
      if(isLast) {
        data_to_send[data_count++] = te_short + 150;
        data_to_send[data_count++] = te_short + 150;
        data_to_send[data_count++] = te_long * 6;
      } else {
        data_to_send[data_count++] = te_long;
      }
    }
  }

  Serial.printf("[TX] Feron: %d bits, code=0x%08lX\n", bitCount, (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Honeywell (te_short=160us, te_long=320us, 48 bits)
bool sendHoneywellFromKey(String keyHex, int bitCount) {
  const int te_short = 160, te_long = 320;
  if(bitCount <= 0 || bitCount > 48) {
    Serial.println(F("[TX] Invalid Honeywell bit count in .sub file"));
    return false;
  }
  uint64_t code = parseHexKey(keyHex);

  data_count = 0;
  for(int i = bitCount - 1; i >= 0 && data_count < 1990; i--) {
    bool b = (code >> i) & 0x01;
    if(b) { data_to_send[data_count++] = te_long;  data_to_send[data_count++] = te_short; }
    else  { data_to_send[data_count++] = te_short; data_to_send[data_count++] = te_long; }
  }
  data_to_send[data_count++] = te_short * 3; // trailing (HIGH)

  Serial.printf("[TX] Honeywell: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Generic standard Manchester encoder (half-period = teHalf). Builds a half-bit
// level sequence, merges consecutive equal-level halves into single durations, and
// drops any leading LOW run (equivalent to idle silence) so the result starts on a
// HIGH duration like sendRawData() expects. invert flips which half is HIGH/LOW per
// bit, since Power Smart and Revers_RB2 use opposite polarity conventions.
int buildManchesterPulses(uint64_t code, int bitCount, int teHalf, bool invert) {
  bool levels[128];
  int halfCount = 0;
  for(int i = bitCount - 1; i >= 0 && halfCount < 126; i--) {
    bool b = (code >> i) & 0x01;
    if(invert) b = !b;
    if(b) { levels[halfCount++] = true;  levels[halfCount++] = false; }
    else  { levels[halfCount++] = false; levels[halfCount++] = true; }
  }

  data_count = 0;
  int i = 0;
  while(i < halfCount && !levels[i]) i++; // drop a leading LOW run
  if(i >= halfCount) return 0;

  bool curLevel = true;
  unsigned long dur = 0;
  for(; i < halfCount; i++) {
    if(levels[i] == curLevel) {
      dur += teHalf;
    } else {
      data_to_send[data_count++] = dur;
      curLevel = levels[i];
      dur = teHalf;
    }
  }
  if(dur > 0) data_to_send[data_count++] = dur;
  return data_count;
}

// Power Smart (te_short=225us, 64 bits, Manchester) - real firmware transmits the
// same 64-bit frame 8 times back to back; approximated here as sendRawData()'s
// repeat count instead of baking 8x into one buffer
bool sendPowerSmartFromKey(String keyHex, int bitCount) {
  const int te_short = 225;
  if(bitCount <= 0 || bitCount > 64) {
    Serial.println(F("[TX] Invalid Power Smart bit count in .sub file"));
    return false;
  }
  uint64_t code = parseHexKey(keyHex);

  if(buildManchesterPulses(code, bitCount, te_short, true) == 0) {
    Serial.println(F("[TX] Power Smart: empty frame"));
    return false;
  }

  Serial.printf("[TX] Power Smart: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

// Revers_RB2 (te_short=250us, 64 bits, Manchester) - real firmware repeats the frame
// 6 times internally; approximated here via sendRawData()'s repeat count
bool sendReversRb2FromKey(String keyHex, int bitCount) {
  const int te_short = 250;
  if(bitCount <= 0 || bitCount > 64) {
    Serial.println(F("[TX] Invalid Revers_RB2 bit count in .sub file"));
    return false;
  }
  uint64_t code = parseHexKey(keyHex);

  if(buildManchesterPulses(code, bitCount, te_short, false) == 0) {
    Serial.println(F("[TX] Revers_RB2: empty frame"));
    return false;
  }

  Serial.printf("[TX] Revers_RB2: %d bits, code=0x%08lX%08lX\n", bitCount, (unsigned long)(code >> 32), (unsigned long)code);
  sendRawData(data_to_send, data_count, subFileRepeatCount);
  return true;
}

String subFileProtocol = "";
long subFileFreqHz = 0;
String subFilePreset = "";
int subFileBitCount = 0;
String subFileKeyHex = "";
int subFileTE = 0;

// Reads the file into the module-level fields above, without checking anything or
// sending anything yet - split out so compatibility can be checked before the user
// is asked how many times to send it
bool parseSubFileHeader(String filename) {
  Serial.printf("[TX] Loading Flipper .sub file: %s\n", filename.c_str());

  File f = SD_MMC.open(filename, FILE_READ);
  if(!f) {
    debugPrint("File open failed!", true, true, 2000);
    return false;
  }

  subFileProtocol = "";
  subFileFreqHz = 0;
  subFilePreset = "";
  subFileBitCount = 0;
  subFileKeyHex = "";
  subFileTE = 0;
  data_count = 0;

  while(f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    if(line.startsWith("Frequency:")) {
      subFileFreqHz = line.substring(10).toInt();
    } else if(line.startsWith("Preset:")) {
      subFilePreset = line.substring(7);
      subFilePreset.trim();
    } else if(line.startsWith("Protocol:")) {
      subFileProtocol = line.substring(9);
      subFileProtocol.trim();
    } else if(line.startsWith("Bit:")) {
      subFileBitCount = line.substring(4).toInt();
    } else if(line.startsWith("Key:")) {
      subFileKeyHex = line.substring(4);
      subFileKeyHex.trim();
    } else if(line.startsWith("TE:")) {
      subFileTE = line.substring(3).toInt();
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
  return true;
}

bool isSubProtocolSupported(String protocol) {
  const char* supported[] = {
    "RAW", "Princeton", "Holtek", "Ansonic", "Hormann HSM", "Legrand", "Nice FLO",
    "GateTX", "Dooya", "Linear", "Magellan", "LinearDelta3", "Holtek_HT12X",
    "SMC5326", "Intertechno_V3", "Mastercode", "BETT", "Doitrand", "Elplast",
    "Nero Radio", "Nero Sketch", "Clemsa", "Roger", "Dickert_MAHS", "Feron",
    "Honeywell", "Power Smart", "Revers_RB2"
  };
  for(int i = 0; i < (int)(sizeof(supported) / sizeof(supported[0])); i++) {
    if(protocol == supported[i]) return true;
  }
  return false;
}

// Checks the fields parseSubFileHeader() just read - frequency in the E07's range,
// protocol actually supported - and shows the matching error screen if not. Doesn't
// send anything either way; called before the repeat-count prompt so an
// incompatible file is rejected up front instead of after the user picks a count.
bool checkSubFileCompatible() {
  if(subFileFreqHz > 0 && (subFileFreqHz < E07_FREQ_MIN_HZ || subFileFreqHz > E07_FREQ_MAX_HZ)) {
    Serial.printf("[TX] Rejected .sub file: %.3f MHz is outside the E07 module's ~%.0f-%.0f MHz range\n",
                  subFileFreqHz / 1000000.0, E07_FREQ_MIN_HZ / 1000000.0, E07_FREQ_MAX_HZ / 1000000.0);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Freq out of range"));
    display.printf("%.3f MHz\n", subFileFreqHz / 1000000.0);
    display.printf("E07: %.0f-%.0fMHz\n", E07_FREQ_MIN_HZ / 1000000.0, E07_FREQ_MAX_HZ / 1000000.0);
    display.display();
    delay(2500);
    return false;
  }

  if(!isSubProtocolSupported(subFileProtocol)) {
    Serial.printf("[TX] Unsupported .sub protocol: %s\n", subFileProtocol.c_str());
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println(F("Unsupported"));
    display.println(F("protocol:"));
    display.println(subFileProtocol);
    display.display();
    delay(2000);
    return false;
  }

  if(subFileProtocol == "RAW" && data_count == 0) {
    debugPrint("No RAW data!", true, true, 2000);
    return false;
  }

  return true;
}

// Applies frequency/modulation and actually sends, using the fields already parsed
// and validated by the two functions above
bool sendParsedSubFile() {
  if(subFileFreqHz > 0) {
    frequency = subFileFreqHz / 1000000.0;
  }
  if(subFilePreset.indexOf("Ook") >= 0) mod = 2;
  else if(subFilePreset.indexOf("GFSK") >= 0) mod = 1;
  else if(subFilePreset.indexOf("MSK") >= 0) mod = 4;
  else if(subFilePreset.indexOf("FSK") >= 0) mod = 0; // 2FSK presets, checked after the more specific FSK variants above

  Serial.printf("[TX] .sub file: protocol=%s, %.2f MHz, mod=%d\n", subFileProtocol.c_str(), frequency, mod);

  if(subFileProtocol == "RAW") {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("=[ FLIPPER RAW ]="));
    display.printf("%d pulses\n", data_count);
    display.printf("%.2f MHz\n", frequency);
    display.display();
    Serial.printf("[TX] Flipper .sub RAW: %d pulses\n", data_count);
    sendRawData(data_to_send, data_count, subFileRepeatCount);
    return true;
  }

  if(subFileProtocol == "Princeton") return sendPrincetonFromKey(subFileKeyHex, subFileBitCount, subFileTE);
  if(subFileProtocol == "Holtek") return sendHoltekFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Ansonic") return sendAnsonicFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Hormann HSM") return sendHormannFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Legrand") return sendLegrandFromKey(subFileKeyHex, subFileBitCount, subFileTE);
  if(subFileProtocol == "Nice FLO") return sendNiceFloFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "GateTX") return sendGateTxFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Dooya") return sendDooyaFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Linear") return sendLinearFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Magellan") return sendMagellanFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "LinearDelta3") return sendLinearDelta3FromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Holtek_HT12X") return sendHoltekHt12xFromKey(subFileKeyHex, subFileBitCount, subFileTE);
  if(subFileProtocol == "SMC5326") return sendSmc5326FromKey(subFileKeyHex, subFileBitCount, subFileTE);
  if(subFileProtocol == "Intertechno_V3") return sendIntertechnoV3FromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Mastercode") return sendMastercodeFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "BETT") return sendBettFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Doitrand") return sendDoitrandFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Elplast") return sendElplastFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Nero Radio") return sendNeroRadioFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Nero Sketch") return sendNeroSketchFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Clemsa") return sendClemsaFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Roger") return sendRogerFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Dickert_MAHS") return sendDickertMahsFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Feron") return sendFeronFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Honeywell") return sendHoneywellFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Power Smart") return sendPowerSmartFromKey(subFileKeyHex, subFileBitCount);
  if(subFileProtocol == "Revers_RB2") return sendReversRb2FromKey(subFileKeyHex, subFileBitCount);

  return false; // unreachable if checkSubFileCompatible() was called first
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

  File dir = SD_MMC.open(SUBGHZ_DIR);
  if(!dir) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("=[ FLIPPER .SUB ]="));
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
    }
    file = dir.openNextFile();
  }
  dir.close();

  if(fileCount == 0) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("=[ FLIPPER .SUB ]="));
    display.println(F("No .sub files found"));
    display.println(F("Copy to /subghz"));
    display.display();
    delay(2000);
    return;
  }

  // Wait for the button that opened this menu to be released first - otherwise it's
  // still LOW on the very first check below and would instantly act on it
  while(digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
    delay(10);
  }
  delay(150); // settle past contact bounce

  // Browse the list - UP/DOWN to move, SELECT/RIGHT to send the highlighted file, LEFT to cancel
  int selected = 0;
  int fileOffset = 0;
  const int maxVisible = 4;
  bool browsing = true;
  bool confirmed = false;

  // Scrolling marquee state for the currently-highlighted file, if its name is too
  // long to fit - only the selected row scrolls, the rest stay statically truncated
  int scrollOffset = 0;
  unsigned long lastScrollTime = millis();
  int lastSelected = -1;

  while(browsing) {
    if(selected != lastSelected) {
      scrollOffset = 0;
      lastScrollTime = millis();
      lastSelected = selected;
    }

    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("=[ SUB FILES ]=")); // shorter than "FLIPPER .SUB" - that ran into the counter below
    display.setCursor(90, 0);
    display.printf("%d/%d", selected + 1, fileCount);
    display.drawLine(0, 9, 127, 9, SH110X_WHITE);

    for(int i = 0; i < maxVisible && (fileOffset + i) < fileCount; i++) {
      bool isSelected = (fileOffset + i == selected);
      if(isSelected) {
        display.fillRect(0, 12 + i * 12, 128, 12, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
      } else {
        display.setTextColor(SH110X_WHITE);
      }
      display.setCursor(2, 15 + i * 12);

      String fullName = fileNames[fileOffset + i];
      String shownName;
      if(fullName.length() <= 20) {
        shownName = fullName;
      } else if(isSelected) {
        if(millis() - lastScrollTime > 300) {
          lastScrollTime = millis();
          scrollOffset++;
        }
        String padded = fullName + "   ";
        if(scrollOffset >= (int)padded.length()) scrollOffset = 0;
        String doubled = padded + padded;
        shownName = doubled.substring(scrollOffset, scrollOffset + 20);
      } else {
        shownName = fullName.substring(0, 17) + "...";
      }
      display.print(shownName);
    }
    display.setTextColor(SH110X_WHITE);
    display.display();

    if(millis() - lastButtonPress > buttonDebounce) {
      if(digitalRead(BTN_UP) == LOW) {
        lastButtonPress = millis();
        if(selected > 0) {
          selected--;
          if(selected < fileOffset) fileOffset = selected;
        }
      } else if(digitalRead(BTN_DOWN) == LOW) {
        lastButtonPress = millis();
        if(selected < fileCount - 1) {
          selected++;
          if(selected >= fileOffset + maxVisible) fileOffset = selected - maxVisible + 1;
        }
      } else if(digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
        lastButtonPress = millis();
        confirmed = true;
        browsing = false;
      } else if(digitalRead(BTN_LEFT) == LOW) {
        lastButtonPress = millis();
        browsing = false;
      }
    }
    delay(10);
  }

  if(!confirmed) {
    Serial.println(F("[TX] Flipper .sub cancelled"));
    return;
  }

  // Wait for the SELECT/RIGHT that just confirmed the file to be released before
  // reading buttons again below, for the same reason as every other screen here
  while(digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
    delay(10);
  }
  delay(150);

  // Parse and check compatibility (frequency in range, protocol supported) BEFORE
  // asking how many times to send it - no point picking a repeat count for a file
  // that's going to be rejected anyway
  String path = String(SUBGHZ_DIR) + "/" + fileNames[selected];
  if(!parseSubFileHeader(path) || !checkSubFileCompatible()) {
    return;
  }

  // Ask how many times to send it, instead of each protocol silently hardcoding
  // its own repeat count
  bool pickingCount = true;
  while(pickingCount) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("=[ REPEAT COUNT ]="));
    display.setCursor(10, 24);
    display.setTextSize(2);
    display.printf("%d", subFileRepeatCount);
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.println(F("UP/DOWN: +/- 1"));
    display.println(F("SELECT: Send"));
    display.display();

    if(millis() - lastButtonPress > buttonDebounce) {
      if(digitalRead(BTN_UP) == LOW) {
        lastButtonPress = millis();
        subFileRepeatCount++;
        if(subFileRepeatCount > 20) subFileRepeatCount = 20;
      } else if(digitalRead(BTN_DOWN) == LOW) {
        lastButtonPress = millis();
        subFileRepeatCount--;
        if(subFileRepeatCount < 1) subFileRepeatCount = 1;
      } else if(digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
        lastButtonPress = millis();
        pickingCount = false;
      } else if(digitalRead(BTN_LEFT) == LOW) {
        lastButtonPress = millis();
        Serial.println(F("[TX] Flipper .sub cancelled"));
        return;
      }
    }
    delay(10);
  }

  // Wait for that confirm press to release too, before the actual send logic
  // (which itself checks buttons for abort) starts reading them
  while(digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
    delay(10);
  }
  delay(150);

  Serial.printf("[TX] Sending \"%s\" x%d\n", fileNames[selected].c_str(), subFileRepeatCount);
  sendParsedSubFile();
}
