#if 1 // for board v1.0
#define REVERSE_PIN_A0_TO_A5
#endif

// pin definitions and other configurations
const int PIN_CE = 10;
const int PIN_VPP_INV = 11;
const int PINS_ADDRESS[8] = {
#ifdef REVERSE_PIN_A0_TO_A5
  A5, A4, A3, A2, A1, A0, 12, 13
#else
  A0, A1, A2, A3, A4, A5, 12, 13
#endif
};
const int PINS_DATA[8] = {
  2, 3, 4, 5, 6, 7, 8, 9
};
const int BIT_ADDRESS_EXPANDED_DATA  = 6;
const int BIT_ADDRESS_EXPANDED_CLOCK = 7;

const int EPROM_PROGRAM_MAX_RETRY = 25;

// STK500 protocol constants
const int Resp_STK_OK      = 0x10;
const int Resp_STK_FAILED  = 0x11;
const int Resp_STK_UNKNOWN = 0x12;
const int Resp_STK_INSYNC  = 0x14;
const int Resp_STK_NOSYNC  = 0x15;

const int Sync_CRC_EOP = 0x20;

unsigned int currentAddress;
void setup() {
  // set CE = 1, VPP = 0
  pinMode(PIN_CE, OUTPUT);
  digitalWrite(PIN_CE, HIGH);
  pinMode(PIN_VPP_INV, OUTPUT);
  digitalWrite(PIN_VPP_INV, HIGH);

  // set address pins to output
  for (int i = 0; i < 8; i++) {
    pinMode(PINS_ADDRESS[i], OUTPUT);
    digitalWrite(PINS_ADDRESS[i], LOW);
  }

  // set data pins to input
  for (int i = 0; i < 8; i++) {
    pinMode(PINS_DATA[i], INPUT);
  }

  currentAddress = 0;
  Serial.begin(115200);
}

void setEpromAddress(unsigned int address, bool expanded) {
  for (int i = 0; i < 6; i++) {
    digitalWrite(PINS_ADDRESS[i], (address >> i) & 1 ? HIGH : LOW);
  }
  if (expanded) {
    static unsigned int cachedAddressUpper = -1;
    unsigned int addressUpper = address >> 6;
    if (cachedAddressUpper != addressUpper) {
      for (int i = 10 - 1; i >= 0; i--) {
        digitalWrite(PINS_ADDRESS[BIT_ADDRESS_EXPANDED_DATA],
          (addressUpper >> i) & 1 ? HIGH : LOW);
        delayMicroseconds(1);
        digitalWrite(PINS_ADDRESS[BIT_ADDRESS_EXPANDED_CLOCK], HIGH);
        delayMicroseconds(1);
        digitalWrite(PINS_ADDRESS[BIT_ADDRESS_EXPANDED_CLOCK], LOW);
      }
      cachedAddressUpper = addressUpper;
    }
  } else {
    digitalWrite(PINS_ADDRESS[6], (address >> 6) & 1 ? HIGH : LOW);
    digitalWrite(PINS_ADDRESS[7], (address >> 7) & 1 ? HIGH : LOW);
  }
}

void readEprom(unsigned char* buf, unsigned int address, int length, bool expanded) {
  // set data pins to input
  for (int i = 0; i < 8; i++) {
    pinMode(PINS_DATA[i], INPUT);
  }
  // enable read
  digitalWrite(PIN_CE, LOW);
  // read data
  for (int i = 0; i < length; i++) {
    setEpromAddress(address + i, expanded);
    delayMicroseconds(1);
    unsigned char dataRead = 0;
    for (int j = 0; j < 8; j++) {
      if (digitalRead(PINS_DATA[j]) != LOW) dataRead |= 1 << j;
    }
    buf[i] = dataRead;
  }
  // disable read
  digitalWrite(PIN_CE, HIGH);
}

bool writeEprom(unsigned char data, unsigned int address, bool expanded) {
  setEpromAddress(address, expanded);
  // note, switching VPP takes some time, so add enough delay
  for (int round = 0; round < EPROM_PROGRAM_MAX_RETRY; round++) {
    // VPP = 1
    digitalWrite(PIN_VPP_INV, LOW);
    // set data
    for (int i = 0; i < 8; i++) {
      pinMode(PINS_DATA[i], OUTPUT);
      digitalWrite(PINS_DATA[i], (data >> i) & 1 ? HIGH : LOW);
    }
    delayMicroseconds(40);
    // send programming pulse
    noInterrupts();
    digitalWrite(PIN_CE, LOW);
    delayMicroseconds(97);
    digitalWrite(PIN_CE, HIGH);
    interrupts();
    // verify
    delayMicroseconds(2);
    for (int i = 0; i < 8; i++) {
      pinMode(PINS_DATA[i], INPUT);
    }
    digitalWrite(PIN_VPP_INV, HIGH);
    delayMicroseconds(40);
    digitalWrite(PIN_CE, LOW);
    delayMicroseconds(1);
    unsigned char verifyReadValue = 0;
    for (int i = 0; i < 8; i++) {
      if (digitalRead(PINS_DATA[i]) != LOW) verifyReadValue |= 1 << i;
    }
    digitalWrite(PIN_CE, HIGH);
    delayMicroseconds(1);
    if (data == verifyReadValue) return true;
  }
  return false;
}

bool readStk500Params(unsigned char* buf, int paramLength) {
  for (int i = 0; i < paramLength; ) {
    int c = Serial.read();
    if (c < 0) continue;
    if (buf != NULL) buf[i] = c;
    i++;
  }
  int sync;
  do {
    sync = Serial.read();
  } while (sync < 0);
  return sync == Sync_CRC_EOP;
}

bool stk500Nop(int dataLength, int replyLength) {
  if (readStk500Params(NULL, dataLength)) {
    Serial.write(Resp_STK_INSYNC);
    for (int i = 0; i < replyLength; i++) {
      Serial.write(0);
    }
    Serial.write(Resp_STK_OK);
    return true;
  } else {
    Serial.write(Resp_STK_NOSYNC);
    return false;
  }
}

void loop() {
  int cmd = Serial.read();
  if (cmd < 0) return;
  switch (cmd) {
    case 0x31: // Check if Starterkit Present
      if (readStk500Params(NULL, 0)) {
        Serial.write(Resp_STK_INSYNC);
        Serial.write('A');
        Serial.write('V');
        Serial.write('R');
        Serial.write(' ');
        Serial.write('S');
        Serial.write('T');
        Serial.write('K');
        Serial.write(Resp_STK_OK);
      } else {
        Serial.write(Resp_STK_NOSYNC);
      }
      break;
    case 0x30: // Get Synchronization
      stk500Nop(0, 0);
      break;
    case 0x41: // Get Parameter Value
      stk500Nop(1, 1);
      break;
    case 0x40: // Set Parameter Value
      stk500Nop(2, 0);
      break;
    case 0x42: // Set Device Programming Parameters
      stk500Nop(20, 0);
      break;
    case 0x45: // Set Extended Device Programming Parameters
      stk500Nop(4, 0);
      break;
    case 0x50: // Enter Program Mode
      stk500Nop(0, 0);
      break;
    case 0x51: // Leave Program Mode
      stk500Nop(0, 0);
      break;
    case 0x52: // Chip Erase
      stk500Nop(0, 0);
      break;
    case 0x53: // Check for Address Autoincrement
      stk500Nop(0, 0);
      break;
    case 0x55: // Load Address
      {
        unsigned char addr[2];
        if (readStk500Params(addr, 2)) {
          currentAddress = (addr[0] | (addr[1] << 8)) << 1;
          Serial.write(Resp_STK_INSYNC);
          Serial.write(Resp_STK_OK);
        } else {
          Serial.write(Resp_STK_NOSYNC);
        }
      }
      break;
    case 0x60: // Program Flash Memory
      {
        unsigned char data[2];
        if (readStk500Params(data, 2)) {
          Serial.write(Resp_STK_INSYNC);
          if (writeEprom(data[0], currentAddress, true) && writeEprom(data[1], currentAddress + 1, true)) {
            currentAddress += 2;
            Serial.write(Resp_STK_OK);
          } else {
            Serial.write(Resp_STK_FAILED);
          }
        } else {
          Serial.write(Resp_STK_NOSYNC);
        }
      }
      break;
    case 0x61: // Program Data Memory
      {
        unsigned char data[1];
        if (readStk500Params(data, 1)) {
          Serial.write(Resp_STK_INSYNC);
          if (writeEprom(data[0], currentAddress, false)) {
            currentAddress += 1;
            Serial.write(Resp_STK_OK);
          } else {
            Serial.write(Resp_STK_FAILED);
          }
        } else {
          Serial.write(Resp_STK_NOSYNC);
        }
      }
      break;
    case 0x62: // Program Fuse Bits
      stk500Nop(2, 0);
      break;
    case 0x65: // Program Fuse Bits Extended
      stk500Nop(2, 0);
      break;
    case 0x63: // Program Lock bits
      stk500Nop(0, 0);
      break;
    case 0x64: // Program Page
      {
        unsigned char params[3];
        for (int i = 0; i < 3; ) {
          int c = Serial.read();
          if (c < 0) continue;
          params[i++] = c;
        }
        unsigned int size = params[1] | (params[0] << 8);
        if (size > 256 || (params[2] != 'E' && params[2] != 'F')) {
          Serial.write(Resp_STK_NOSYNC);
        } else {
          Serial.write(Resp_STK_INSYNC);
          unsigned char data[256];
          if (readStk500Params(data, size)) {
            bool success = true;
            for (unsigned int i = 0; i < size; i++) {
              if (!writeEprom(data[i], currentAddress + i, params[2] == 'F')) {
                success = false;
                break;
              }
            }
            if (success) {
              currentAddress += size;
              Serial.write(Resp_STK_OK);
            } else {
              Serial.write(Resp_STK_FAILED);
            }
          } else {
            Serial.write(Resp_STK_NOSYNC);
          }
        }
      }
      break;
    case 0x70: // Read Flash Memory
      if (readStk500Params(NULL, 0)) {
        Serial.write(Resp_STK_INSYNC);
        unsigned char data[2];
        readEprom(data, currentAddress, 2, true);
        currentAddress += 2;
        Serial.write(data[0]);
        Serial.write(data[1]);
        Serial.write(Resp_STK_OK);
      } else {
        Serial.write(Resp_STK_NOSYNC);
      }
      break;
    case 0x71: // Read Data Memory
      if (readStk500Params(NULL, 0)) {
        Serial.write(Resp_STK_INSYNC);
        unsigned char data[1];
        readEprom(data, currentAddress, 1, false);
        currentAddress += 1;
        Serial.write(data[0]);
        Serial.write(Resp_STK_OK);
      } else {
        Serial.write(Resp_STK_NOSYNC);
      }
      break;
    case 0x72: // Read Fuse Bits
      stk500Nop(0, 2);
      break;
    case 0x77: // Read Fuse Bits Extended
      stk500Nop(0, 3);
      break;
    case 0x73: // Read Lock Bits
      stk500Nop(0, 1);
      break;
    case 0x74: // Read Page
      {
        unsigned char params[3];
        if (readStk500Params(params, 3)) {
          unsigned int size = params[1] | (params[0] << 8);
          if (size <= 256 && (params[2] == 'E' || params[2] == 'F')) {
            Serial.write(Resp_STK_INSYNC);
            char data[256];
            readEprom(data, currentAddress, size, params[2] == 'F');
            for (unsigned int i = 0; i < size; i++) {
              Serial.write(data[i]);
            }
            currentAddress += size;
            Serial.write(Resp_STK_OK);
          } else {
            Serial.write(Resp_STK_NOSYNC);
          }
        } else {
          Serial.write(Resp_STK_NOSYNC);
        }
      }
      break;
    case 0x75: // Read Signature Bytes
      if (readStk500Params(NULL, 0)) {
        Serial.write(Resp_STK_INSYNC);
        Serial.write(0xEE);
        Serial.write(0x96);
        Serial.write(0x60);
        Serial.write(Resp_STK_OK);
      } else {
        Serial.write(Resp_STK_NOSYNC);
      }
      break;
    case 0x76: // Read Oscillator Calibration Byte
      stk500Nop(0, 1);
      break;
    case 0x78: // Read Oscillator Calibration Byte Extended
      stk500Nop(1, 1);
      break;
    case 0x56: // Universal Command
      {
        unsigned char params[4];
        if (readStk500Params(params, 4)) {
          Serial.write(Resp_STK_INSYNC);
          if (params[0] == 0xa5 && params[1] == 0x16) {
            // read signature
            Serial.write("\xEE\x96\x60"[params[2] & 3]);
          } else if (params[0] == 0xce) {
            // chip erase
            Serial.write(0);
          } else {
            Serial.write(0);
          }
          Serial.write(Resp_STK_OK);
        } else {
          Serial.write(Resp_STK_NOSYNC);
        }
      }
      break;
    case 0x57: // Extended Universal Command
      {
        int s;
        do {
          s = Serial.read();
        } while (s < 0);
        stk500Nop(s + 1, 0);
      }
      break;
    default:
      if (readStk500Params(NULL, 0)) {
        Serial.write(Resp_STK_UNKNOWN);
      } else {
        Serial.write(Resp_STK_NOSYNC);
      }
      break;
  }
}
