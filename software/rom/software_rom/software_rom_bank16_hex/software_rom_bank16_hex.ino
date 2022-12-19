#include <EEPROM.h>

const int ADDR_PINS[8] = {8, 9, 10, 11, 12, 4, 5, 6};
const int DATA_PINS[8] = {A0, A1, A2, A3, A4, A5, 2, 3};

const int XON = 0x11;
const int XOFF = 0x13;

unsigned char romData[256];

int lineCount = 0;
bool lineRead = false;
const int LINE_BUFFER_MAX = (4 + 256 + 2 + 2) * 2;
char lineBuffer[LINE_BUFFER_MAX];
unsigned char lineBufferDecoded[LINE_BUFFER_MAX / 2];

unsigned long addressOffset = 0;

int decodeOneNibble(char c) {
  if ('0' <= c && c <= '9') return c - '0';
  if ('a' <= c && c <= 'f') return c - 'a' + 10;
  if ('A' <= c && c <= 'F') return c - 'A' + 10;
  return 0;
}

void parseHexLine() {
  if (lineCount % 2 != 0) return;
  int byteCount = lineCount / 2;
  if (byteCount < 5) return;
  unsigned char checksum = 0;
  for (int i = 0; i < byteCount; i++) {
    lineBufferDecoded[i] = (decodeOneNibble(lineBuffer[i * 2]) << 4) |
      decodeOneNibble(lineBuffer[i * 2 + 1]);
    checksum += lineBufferDecoded[i];
  }
  if (byteCount != lineBufferDecoded[0] + 5) return;
  if (checksum != 0) return;
  switch (lineBufferDecoded[3]) {
    case 0: // data
      {
        unsigned long addressStart = addressOffset +
          ((unsigned long)lineBufferDecoded[1] << 8) + lineBufferDecoded[2];
        for (int i = 0; i < lineBufferDecoded[0]; i++) {
          unsigned long address = addressStart + i; // TOOD: better wraparound support
          if (address < 0x100) {
            unsigned char value = lineBufferDecoded[4 + i];
            romData[address] = value;
            EEPROM.write(address, value);
          }
        }
      }
      break;
    case 1: // EOF
      Serial.println("OK");
      break;
    case 2: // segment offset
      if (lineBufferDecoded[0] != 2) break;
      addressOffset = (((unsigned long)lineBuffer[4] << 8) | lineBuffer[5]) << 4;
      break;
    case 4: // linear offset
      if (lineBufferDecoded[0] != 2) break;
      addressOffset = (((unsigned long)lineBuffer[4] << 8) | lineBuffer[5]) << 16;
      break;
  }
}

int romBank;
int prevRomDataRead;

void setup() {
  for (int i = 0; i < 8; i++) {
    pinMode(ADDR_PINS[i], INPUT);
    pinMode(DATA_PINS[i], OUTPUT);
  }
  for (int i = 0; i < 256; i++) {
    romData[i] = EEPROM.read(i);
  }
  romBank = 0;
  prevRomDataRead = 0;
  Serial.begin(9600);
}

void loop() {
  int c = Serial.read();
  if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F') ||
  c == ':' || c == '\r' || c == '\n') {
    if (c == ':') {
      if (!lineRead) {
        lineRead = true;
        lineCount = 0;
      }
    } else if (c == '\r' || c == '\n') {
      if (lineRead) {
        Serial.write(XOFF);
        parseHexLine();
        Serial.write(XON);
        lineRead = false;
      }
    } else {
      if (lineRead) {
        if (lineCount < LINE_BUFFER_MAX) lineBuffer[lineCount++] = c;
      }
    }
  }

  int romAddr = 0;
  for (int i = 0; i < 8; i++) {
    if (digitalRead(ADDR_PINS[i]) != LOW) romAddr |= 1 << i;
  }
  int romDataRead = romData[((romBank << 4) & 0xf0) | (romAddr & 0xf)];
  for (int i = 0; i < 8; i++) {
    digitalWrite(DATA_PINS[i], (romDataRead >> i) & 1 ? HIGH : LOW);
  }
  if (romDataRead == prevRomDataRead && (romDataRead & 0xf0) == 0xa0) {
    romBank = romDataRead & 0xf;
  }
  prevRomDataRead = romDataRead;
}
