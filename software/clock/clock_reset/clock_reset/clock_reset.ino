#include <Wire.h>
#include <EEPROM.h>

const unsigned long CLOCK_FREQ = 16000000ul;

const unsigned long COUNT_LIMIT_FREQ = CLOCK_FREQ / 160;

const int UP_BTN_PIN = 2;
const int DOWN_BTN_PIN = 3;
const int RUN_BTN_PIN = 5;
const int STEP_BTN_PIN = 6;
const int RESET_BTN_PIN = 7;
const int RESET_OUT_PIN = 8;
const int CLOCK_OUT_PIN = 9;

const int LCD_ADDR = 0x3e;
const int LCD_CONTRAST_DEFAULT = 6;
const int LCD_CONTRAST_MIN = 0, LCD_CONTRAST_MAX = 0x3f;
const int LCD_CONTRAST_EEPROM_ADDRESS = 0;

void stopClock() {
  // stop timer, disable PWM, output LOW
  TCCR1B = 0;
  TCCR1A = 0x80;
  TCCR1C = 0x80;
}

void startClock(unsigned long freq) {
  // stop timer, disable PWM, output LOW
  TCCR1B = 0;
  TCCR1A = 0x80;
  TCCR1C = 0x80;

  // calculate period
  unsigned int period;
  int clockSelect;
  if (CLOCK_FREQ % (freq * 2) == 0 && CLOCK_FREQ / freq < 65536ul) {
    period = CLOCK_FREQ / freq;
    clockSelect = 1;
  } else if ((CLOCK_FREQ / 8) % (freq * 2) == 0 && (CLOCK_FREQ / 8) / freq < 65536ul) {
    period = (CLOCK_FREQ / 8) / freq;
    clockSelect = 2;
  } else if ((CLOCK_FREQ / 64) % (freq * 2) == 0 && (CLOCK_FREQ / 64) / freq < 65536ul) {
    period = (CLOCK_FREQ / 64) / freq;
    clockSelect = 3;
  } else if ((CLOCK_FREQ / 256) % (freq * 2) == 0 && (CLOCK_FREQ / 256) / freq < 65536ul) {
    period = (CLOCK_FREQ / 256) / freq;
    clockSelect = 4;
  } else if ((CLOCK_FREQ / 1024) % (freq * 2) == 0 && (CLOCK_FREQ / 1024) / freq < 65536ul) {
    period = (CLOCK_FREQ / 1024) / freq;
    clockSelect = 5;
  } else if (CLOCK_FREQ / freq < 65536ul) {
    period = CLOCK_FREQ / freq;
    clockSelect = 1;
  } else if ((CLOCK_FREQ / 8) / freq < 65536ul) {
    period = (CLOCK_FREQ / 8) / freq;
    clockSelect = 2;
  } else if ((CLOCK_FREQ / 64) / freq < 65536ul) {
    period = (CLOCK_FREQ / 64) / freq;
    clockSelect = 3;
  } else if ((CLOCK_FREQ / 256) / freq < 65536ul) {
    period = (CLOCK_FREQ / 256) / freq;
    clockSelect = 4;
  } else if ((CLOCK_FREQ / 1024) / freq < 65536ul) {
    period = (CLOCK_FREQ / 1024) / freq;
    clockSelect = 5;
  } else {
    clockSelect = 0;
  }

  // reset counter and interrupt
  TCNT1 = 0;
  OCR1A = period / 2 - 1;
  ICR1 = period - 1;
  TIFR0 = 2;
  if (freq > COUNT_LIMIT_FREQ) {
    TIMSK1 = 0;
  } else {
    TIMSK1 = 2; // enable Output Compare A Match Interrupt
  }

  // set PWM mode and start timer
  TCCR1A = 0xc2;
  TCCR1B = 0x18 | clockSelect;
}

volatile unsigned long clockCount = 0;

ISR(TIMER1_COMPA_vect) {
  clockCount++;
  if (clockCount >= 1000000ul) clockCount = 0;
}

void sendLCDCommand(int command, int waitTimeUs) {
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(0x80);
  Wire.write(command);
  Wire.endTransmission();
  delayMicroseconds(waitTimeUs);
}

void sendLCDData(const void* data, int length) {
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(0x40);
  for (int i = 0; i < length; i++) {
    Wire.write(((unsigned char*)data)[i]);
  }
  Wire.endTransmission();
}

void setLCDContrast(int contrast) {
  sendLCDCommand(0x39, 27); // Function set, IS = 1
  sendLCDCommand(0x70 | (contrast & 0xf), 27); // Contrast set (lower)
  sendLCDCommand(0x54 | ((contrast >> 4) & 3), 27); // Power/ICON/Constrast (higher) control
  sendLCDCommand(0x38, 27); // Function set, IS = 0
}

const int BUTTON_PRESSED_UP = 1;
const int BUTTON_PRESSED_DOWN = 2;
const int BUTTON_PRESSED_RUN = 4;
const int BUTTON_PRESSED_STEP = 8;
const int BUTTON_PRESSED_RESET = 0x10;
int getButtonStatus() {
  int status = 0;
  if (digitalRead(UP_BTN_PIN) == LOW) status |= BUTTON_PRESSED_UP;
  if (digitalRead(DOWN_BTN_PIN) == LOW) status |= BUTTON_PRESSED_DOWN;
  if (digitalRead(RUN_BTN_PIN) == LOW) status |= BUTTON_PRESSED_RUN;
  if (digitalRead(STEP_BTN_PIN) == LOW) status |= BUTTON_PRESSED_STEP;
  if (digitalRead(RESET_BTN_PIN) == LOW) status |= BUTTON_PRESSED_RESET;
  return status;
}

unsigned long prevFrameTime;
int prevButton = 0;

unsigned long clockFreq = 1;
int clockFreqTop = 1;
bool clockRunning = false;

bool contrastSetMode = false;
int lcdContrast;

unsigned long freqShown = -1;
unsigned long countShown = -1;
bool runningShown = false;
bool countEnabledShown = false;

int lcdContrastShown = -1;

void updateLCD() {
  if (contrastSetMode) {
    if (lcdContrastShown != lcdContrast) {
      sendLCDCommand(0x80, 27);
      sendLCDData("Contrast", 8);
      char contrastBuffer[8] = "        ";
      contrastBuffer[7] = lcdContrast % 10 + '0';
      if (lcdContrast >= 10) contrastBuffer[6] = lcdContrast / 10 + '0';
      sendLCDCommand(0x80 | 0x40, 27);
      sendLCDData(contrastBuffer, 8);
      lcdContrastShown = lcdContrast;
      freqShown = -1;
      countShown = -1;
    }
  } else {
    noInterrupts();
    unsigned long count = clockCount;
    interrupts();
    if (freqShown != clockFreq) {
      char clockBuffer[8] = "      Hz";
      char* clockBufferPtr = &clockBuffer[6];
      unsigned long freq2 = clockFreq;
      if (freq2 >= 1000000ul) {
        *(--clockBufferPtr) = 'M';
        freq2 /= 1000000ul;
      } else if (freq2 >= 1000ul) {
        *(--clockBufferPtr) = 'k';
        freq2 /= 1000ul;
      }
      while (freq2 > 0) {
        *(--clockBufferPtr) = '0' + freq2 % 10;
        freq2 /= 10;
      }
      sendLCDCommand(0x80, 27);
      sendLCDData(clockBuffer, 8);
      freqShown = clockFreq;
      lcdContrastShown = -1;
    }
    bool countEnabled = clockFreq <= COUNT_LIMIT_FREQ;
    if (countShown != count || countEnabledShown != countEnabled || runningShown != clockRunning) {
      char statusBuffer[8] = "        ";
      statusBuffer[0] = clockRunning ? 0 : 1;
      statusBuffer[1] = ' ';
      if (countEnabled) {
        unsigned long count2 = count;
        for (int i = 7; i >= 2; i--) {
          statusBuffer[i] = count2 % 10 + '0';
          count2 /= 10;
        }
      }
      sendLCDCommand(0x80 | 0x40, 27);
      sendLCDData(statusBuffer, 8);
      countShown = count;
      countEnabledShown = countEnabled;
      runningShown = clockRunning;
      lcdContrastShown = -1;
    }
  }
}

void setup() {
  // initialize pins
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(DOWN_BTN_PIN, INPUT);
  pinMode(RUN_BTN_PIN, INPUT);
  pinMode(STEP_BTN_PIN, INPUT);
  pinMode(RESET_BTN_PIN, INPUT);
  pinMode(RESET_OUT_PIN, OUTPUT);
  pinMode(CLOCK_OUT_PIN, OUTPUT);
  digitalWrite(RESET_OUT_PIN, LOW);
  digitalWrite(CLOCK_OUT_PIN, LOW);

  // initialize clock
  stopClock();

  // initialize LCD
  lcdContrast = EEPROM.read(LCD_CONTRAST_EEPROM_ADDRESS);
  if (lcdContrast < LCD_CONTRAST_MIN || LCD_CONTRAST_MAX < lcdContrast) {
    lcdContrast = LCD_CONTRAST_DEFAULT;
  }
  delay(40);
  Wire.begin();
  sendLCDCommand(0x39, 27); // Function set, IS = 1
  sendLCDCommand(0x14, 27); // Internal OSC frequency
  sendLCDCommand(0x70 | (lcdContrast & 0xf), 27); // Contrast set (lower)
  sendLCDCommand(0x54 | ((lcdContrast >> 4) & 3), 27); // Power/ICON/Constrast (higher) control
  sendLCDCommand(0x6c, 0); // Follower control
  delay(200);
  sendLCDCommand(0x38, 27); // Function set, IS = 0
  sendLCDCommand(0x01, 1080); // Clear Display
  sendLCDCommand(0x0c, 27); // Display ON/OFF control

  // set CGRAM
  sendLCDCommand(0x40, 27);
  sendLCDData(
    "\x10\x18\x1c\x1e\x1c\x18\x10\x00" // play icon
    "\x1b\x1b\x1b\x1b\x1b\x1b\x1b\x00" // pause icon
  , 16);

  // show info
  updateLCD();

  // initialization end
  digitalWrite(RESET_OUT_PIN, HIGH);
  prevFrameTime = millis();
}

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - prevFrameTime < 20u) return;
  prevFrameTime = currentTime;
  int currentButton = getButtonStatus();
  int pressedButton = currentButton & ~prevButton;
  int releasedButton = prevButton & ~currentButton;
  prevButton = currentButton;
  if (pressedButton & BUTTON_PRESSED_RUN) {
    if (!contrastSetMode) {
      if (clockRunning) {
        stopClock();
      } else {
        startClock(clockFreq);
      }
      clockRunning = !clockRunning;
    }
  }
  if (pressedButton & BUTTON_PRESSED_STEP) {
    if (!clockRunning && !contrastSetMode) {
      digitalWrite(CLOCK_OUT_PIN, HIGH);
      delay(10);
      digitalWrite(CLOCK_OUT_PIN, LOW);
      prevFrameTime += 10;
      noInterrupts();
      clockCount++;
      if (clockCount >= 1000000ul) clockCount = 0;
      interrupts();
    }
  }
  if (pressedButton & BUTTON_PRESSED_RESET) {
    stopClock();
    noInterrupts();
    clockCount = 0;
    interrupts();
    clockRunning = false;
    digitalWrite(RESET_OUT_PIN, LOW);
    delay(10);
    digitalWrite(RESET_OUT_PIN, HIGH);
    prevFrameTime += 10;
  }
  if(releasedButton & BUTTON_PRESSED_RESET) {
    if (contrastSetMode) {
      EEPROM.write(LCD_CONTRAST_EEPROM_ADDRESS, lcdContrast);
      contrastSetMode = false;
    }
  }
  if (pressedButton & BUTTON_PRESSED_UP) {
    if (currentButton & BUTTON_PRESSED_RESET) {
      if (!contrastSetMode) {
        contrastSetMode = true;
      } else {
        if (lcdContrast < LCD_CONTRAST_MAX) {
          lcdContrast++;
          setLCDContrast(lcdContrast);
        }
      }
    } else {
      unsigned long clockFreqBackup = clockFreq;
      int clockFreqTopBackup = clockFreqTop;
      if (clockFreq < 1000000ul) {
        switch (clockFreqTop) {
          case 1:
            clockFreq *= 2;
            clockFreqTop = 2;
            break;
          case 2:
            clockFreq = clockFreq / 2 * 5;
            clockFreqTop = 5;
            break;
          case 5:
            clockFreq *= 2;
            clockFreqTop = 1;
            break;
          default:
            clockFreq = 1;
            clockFreqTop = 1;
            break;
        }
      } else {
        clockFreq *= 2;
        clockFreqTop *= 2;
      }
      if (clockFreq <= CLOCK_FREQ / 2) {
        if (clockRunning) startClock(clockFreq);
      } else {
        clockFreq = clockFreqBackup;
        clockFreqTop = clockFreqTopBackup;
      }
    }
  }
  if (pressedButton & BUTTON_PRESSED_DOWN) {
    if (currentButton & BUTTON_PRESSED_RESET) {
      if (!contrastSetMode) {
        contrastSetMode = true;
      } else {
        if (lcdContrast > LCD_CONTRAST_MIN) {
          lcdContrast--;
          setLCDContrast(lcdContrast);
        }
      }
    } else {
      if (clockFreq > 1000000ul) {
        clockFreq /= 2;
        clockFreqTop /= 2;
      } else if (clockFreq > 1) {
        switch (clockFreqTop) {
          case 1:
            clockFreq /= 2;
            clockFreqTop = 5;
            break;
          case 2:
            clockFreq /= 2;
            clockFreqTop = 1;
            break;
          case 5:
            clockFreq = clockFreq / 5 * 2;
            clockFreqTop = 2;
            break;
          default:
            clockFreq = 1;
            clockFreqTop = 1;
            break;
        }
      }
      if (clockRunning) startClock(clockFreq);
    }
  }
  updateLCD();
}
