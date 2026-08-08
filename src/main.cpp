#include "pdmAudio.h"
#include <Arduino.h>

#include <NeoPixelBus.h>

pdmAudio pdm;

constexpr pin_size_t LEFT_BUTTON = 4;
constexpr pin_size_t RIGHT_BUTTON = 7;

constexpr pin_size_t LEFT_LED = 2;
constexpr pin_size_t RIGHT_LED = 8;

constexpr pin_size_t PADDLE_DIT = 24;
constexpr pin_size_t PADDLE_DAH = 25;

constexpr pin_size_t KEY_OUTPUT_DIT = 9;
constexpr pin_size_t KEY_OUTPUT_DAH = 10;

constexpr pin_size_t SPEED_BUTTON = 5;
constexpr pin_size_t MODE_BUTTON = 6;

constexpr uint32_t TONE_FREQUENCY = 440; // Frequency in Hz

bool speedDebounce = false;
bool modeDebounce = false;

unsigned long toneStartTime = 0;
bool finishedCurrentState = false;
bool iambicBPlaying = false;

// Create custom UART on UART0: GP0 (TX) and GP1 (RX)
// This is exposed on the TC-2030 connector
UART debugSerial(0, 1, NC, NC);

NeoPixelBus<NeoGrbFeature, NeoSk6812Method>
    rgbLed(2, 11); // note: RGB (not RGBW) Sk6812, smaller strip
enum class DitLength { Slow = 120, Medium = 80, Fast = 60, Turbo = 40 };
enum class Speed { Slow = 1, Medium = 2, Fast = 3, Turbo = 4 };
inline Speed &operator++(Speed &speed) {
  speed = static_cast<Speed>(static_cast<int>(speed) + 1);
  return speed;
}
enum class Mode { StraightKey = 1, IambicA = 2, IambicB = 3, Passthrough = 4 };
inline Mode &operator++(Mode &mode) {
  mode = static_cast<Mode>(static_cast<int>(mode) + 1);
  return mode;
}
Speed speedSelection = Speed::Slow;
Mode modeSelection = Mode::StraightKey;

enum class IambicState { Idle = 1, Dit = 2, Dah = 3 };
IambicState currentIambicState = IambicState::Idle;

constexpr unsigned long ditLengthFor(Speed speed) {
  switch (speed) {
  case Speed::Slow:
    return static_cast<unsigned long>(DitLength::Slow);
  case Speed::Medium:
    return static_cast<unsigned long>(DitLength::Medium);
  case Speed::Fast:
    return static_cast<unsigned long>(DitLength::Fast);
  case Speed::Turbo:
    return static_cast<unsigned long>(DitLength::Turbo);
  default:
    return static_cast<unsigned long>(DitLength::Slow);
  }
}

void setup() {
  pdm.begin(14);
  pdm.USB_UAC(); // This blocks - comment out if not using USB audio

  rgbLed.Begin();
  // Start in StraightThrough mode with red LED
  rgbLed.SetPixelColor(1, RgbColor(128, 0, 0));
  rgbLed.Show();

  // LEDs
  pinMode(LEFT_LED, OUTPUT);
  pinMode(RIGHT_LED, OUTPUT);

  // 3.5mm output
  pinMode(KEY_OUTPUT_DIT, OUTPUT);
  pinMode(KEY_OUTPUT_DAH, OUTPUT);

  // Morse Inputs
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);

  // Paddle inputs
  pinMode(PADDLE_DIT, INPUT_PULLUP);
  pinMode(PADDLE_DAH, INPUT_PULLUP);

  // Speed and Mode buttons
  pinMode(SPEED_BUTTON, INPUT_PULLUP);
  pinMode(MODE_BUTTON, INPUT_PULLUP);

  debugSerial.begin(115200);
}

void setSpeedLight(Speed speed) {
  switch (speed) {
  case Speed::Slow:
    rgbLed.SetPixelColor(0, RgbColor(128, 0, 0));
    break;
  case Speed::Medium:
    rgbLed.SetPixelColor(0, RgbColor(128, 128, 0));
    break;
  case Speed::Fast:
    rgbLed.SetPixelColor(0, RgbColor(0, 128, 0));
    break;
  case Speed::Turbo:
    rgbLed.SetPixelColor(0, RgbColor(0, 128, 128));
    break;
  default:
    rgbLed.SetPixelColor(0, RgbColor(128, 0, 0));
    break;
  }
}

void doModeDetection() {
  int speedButton = digitalRead(SPEED_BUTTON);
  int modeButton = digitalRead(MODE_BUTTON);

  if (modeButton == LOW && !modeDebounce) {
    modeDebounce = true;

    ++modeSelection;
    if (modeSelection > Mode::Passthrough) {
      modeSelection = Mode::StraightKey;
    }
    switch (modeSelection) {
    case Mode::StraightKey:
      rgbLed.SetPixelColor(1, RgbColor(128, 0, 0));
      rgbLed.SetPixelColor(0, RgbColor(0, 0, 0));
      break;
    case Mode::IambicA:
      rgbLed.SetPixelColor(1, RgbColor(0, 128, 0));
      setSpeedLight(speedSelection);
      break;
    case Mode::IambicB:
      rgbLed.SetPixelColor(1, RgbColor(0, 0, 128));
      setSpeedLight(speedSelection);
      break;
    case Mode::Passthrough:
      rgbLed.SetPixelColor(1, RgbColor(128, 128, 0));
      rgbLed.SetPixelColor(0, RgbColor(0, 0, 0));
      break;
    }
  } else if (speedButton == LOW && !speedDebounce &&
             modeSelection != Mode::StraightKey) {
    speedDebounce = true;

    ++speedSelection;
    if (speedSelection > Speed::Turbo) {
      speedSelection = Speed::Slow;
    }

    setSpeedLight(speedSelection);
  } else if (speedButton == HIGH && modeButton == HIGH &&
             (speedDebounce || modeDebounce)) {
    speedDebounce = false;
    modeDebounce = false;
  }
}

struct Inputs {
  bool leftButton;
  bool rightButton;
  bool paddleDit;
  bool paddleDah;
};

Inputs getInputs() {
  bool leftButton = digitalRead(LEFT_BUTTON) == LOW;
  bool rightButton = digitalRead(RIGHT_BUTTON) == LOW;
  bool paddleDit = digitalRead(PADDLE_DIT) == LOW;
  bool paddleDah = digitalRead(PADDLE_DAH) == LOW;

  return Inputs{leftButton, rightButton, paddleDit, paddleDah};
}

void doOutputs(bool dit, bool dah) {
  digitalWrite(LEFT_LED, dit ? HIGH : LOW);
  digitalWrite(RIGHT_LED, dah ? HIGH : LOW);
  digitalWrite(KEY_OUTPUT_DIT, dit ? HIGH : LOW);
  digitalWrite(KEY_OUTPUT_DAH, dah ? HIGH : LOW);

  int16_t sample = pdm.sine_lu(TONE_FREQUENCY);
  pdm.USBtransfer(sample, sample);
}

void resetActions() {
  digitalWrite(LEFT_LED, LOW);
  digitalWrite(RIGHT_LED, LOW);
  digitalWrite(KEY_OUTPUT_DIT, LOW);
  digitalWrite(KEY_OUTPUT_DAH, LOW);

  pdm.USBtransfer(0, 0);
}

void doStraightKey() {
  Inputs inputs = getInputs();

  if (inputs.leftButton || inputs.rightButton || inputs.paddleDit ||
      inputs.paddleDah) {
    doOutputs(true, false);
  } else {
    resetActions();
  }
}

void doPassthrough() {

  Inputs inputs = getInputs();

  if (inputs.paddleDit) {
    digitalWrite(LEFT_LED, HIGH);
    digitalWrite(KEY_OUTPUT_DIT, HIGH);
  } else {
    digitalWrite(LEFT_LED, LOW);
    digitalWrite(KEY_OUTPUT_DIT, LOW);
  }
  if (inputs.paddleDah) {
    digitalWrite(RIGHT_LED, HIGH);
    digitalWrite(KEY_OUTPUT_DAH, HIGH);
  } else {
    digitalWrite(RIGHT_LED, LOW);
    digitalWrite(KEY_OUTPUT_DAH, LOW);
  }
}

bool playDit() {
  unsigned long difference = millis() - toneStartTime;
  unsigned long ditLength = ditLengthFor(speedSelection);

  if (difference < ditLength) {
    doOutputs(true, false);
  } else if (difference < 2 * ditLength) {
    resetActions();
  } else {
    toneStartTime = millis();
    resetActions();
    return true;
  }
  return false;
}

bool playDah() {
  unsigned long difference = millis() - toneStartTime;
  unsigned long ditLength = ditLengthFor(speedSelection);

  if (difference < 3 * ditLength) {
    doOutputs(false, true);
  } else if (difference < 4 * ditLength) {
    resetActions();
  } else {
    toneStartTime = millis();
    resetActions();
    return true;
  }
  return false;
}

void resetIambicState() {
  debugSerial.println("Resetting Iambic State");
  currentIambicState = IambicState::Idle;
  finishedCurrentState = false;
  toneStartTime = 0;
  resetActions();
}

void doIambic(bool isIambicB) {
  Inputs inputs = getInputs();

  bool doDit = inputs.leftButton || inputs.paddleDit;
  bool doDah = inputs.rightButton || inputs.paddleDah;

  if ((doDit || doDah) && currentIambicState == IambicState::Idle) {

    if (doDit) {
      currentIambicState = IambicState::Dit;
      toneStartTime = millis();
    } else if (doDah) {
      currentIambicState = IambicState::Dah;
      toneStartTime = millis();
    }
  } else if (doDit && doDah) {

    if (currentIambicState == IambicState::Dit) {
      if (finishedCurrentState) {
        if (isIambicB) {
          iambicBPlaying = true;
        }
        currentIambicState = IambicState::Dah;
        toneStartTime = millis();
        finishedCurrentState = false;
      }
    } else if (currentIambicState == IambicState::Dah) {
      if (finishedCurrentState) {
        if (isIambicB) {
          iambicBPlaying = true;
        }
        currentIambicState = IambicState::Dit;
        toneStartTime = millis();
        finishedCurrentState = false;
      }
    }
  } else if (!(doDit || doDah) && currentIambicState != IambicState::Idle &&
             finishedCurrentState) {
    if (iambicBPlaying) {
      iambicBPlaying = false;
      currentIambicState = (currentIambicState == IambicState::Dit)
                               ? IambicState::Dah
                               : IambicState::Dit;
      toneStartTime = millis();
      finishedCurrentState = false;
    } else {
      resetIambicState();
    }
  }

  switch (currentIambicState) {
  case IambicState::Dit:
    finishedCurrentState = playDit();
    break;
  case IambicState::Dah:
    finishedCurrentState = playDah();
    break;
  default:
    break;
  }
}

void loop() {

  doModeDetection();

  switch (modeSelection) {
  case Mode::StraightKey:
    doStraightKey();
    break;
  case Mode::IambicA:
    doIambic(false);
    break;
  case Mode::IambicB:
    doIambic(true);
    break;
  case Mode::Passthrough:
    doPassthrough();
    break;
  default:
    break;
  }

  if (rgbLed.IsDirty()) {
    rgbLed.Show();
  }
}
