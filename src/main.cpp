#include <Arduino.h>
#include "pdmAudio.h"

#include <NeoPixelBus.h>

pdmAudio pdm;

// Create custom UART on UART0: GP0 (TX) and GP1 (RX)
UART mySerial(0, 1, NC, NC);

NeoPixelBus<NeoGrbwFeature, NeoSk6812Method> rgb_led(1, 11); // note: RGBW and Sk6812 and smaller strip

bool ditLEDState = false;
bool dahLEDState = false;


int left_button = 4;
int right_button = 6;

int left_led = 2;
int right_led = 8;

int paddle_dit = 24;
int paddle_dah = 25;

int tone_frequency = 440; // Frequency in Hz

void setup() {
  //pdm.begin(14);
  //pdm.USB_UAC();  // This blocks - comment out if not using USB audio

  rgb_led.Begin();
  rgb_led.SetPixelColor(0, RgbColor(0, 255, 0));
  rgb_led.Show();

  // LEDs
  pinMode(left_led, OUTPUT);
  pinMode(right_led, OUTPUT);

  // 3.5mm output
  pinMode(19, OUTPUT);

  // Morse Inputs
  pinMode(left_button, INPUT_PULLUP);
  pinMode(right_button, INPUT_PULLUP);

  // Paddle inputs
  pinMode(paddle_dit, INPUT_PULLUP);
  pinMode(paddle_dah, INPUT_PULLUP);

  mySerial.begin(115200);
}

void loop() {


  if (ditLEDState == true) {
    int16_t left = pdm.sine_lu(tone_frequency);
  int16_t right = pdm.sine_lu(tone_frequency);

    pdm.USBtransfer(left, right);
  }
  else {
    int16_t left = pdm.sine_lu(0);
    int16_t right = pdm.sine_lu(0);

    pdm.USBtransfer(left, right);
  }

  int button1 = digitalRead(left_button);
  int button2 = digitalRead(right_button);

  int paddleDit = digitalRead(paddle_dit);
  int paddleDah = digitalRead(paddle_dah);

  if (button1 == LOW) {
    //mySerial.println("Button 1 Pressed");
    ditLEDState = true;
    digitalWrite(left_led, ditLEDState ? HIGH : LOW);
    //digitalWrite(8, ditLEDState ? HIGH : LOW);
  } else {
    if (ditLEDState) {
      ditLEDState = false;
      digitalWrite(left_led, ditLEDState ? HIGH : LOW);
      //digitalWrite(8, ditLEDState ? HIGH : LOW);
    }
  }
  if (button2 == LOW) {
    dahLEDState = true;
    digitalWrite(right_led, dahLEDState ? HIGH : LOW);
  } else {
    if (dahLEDState) {
      dahLEDState = false;
      digitalWrite(right_led, dahLEDState ? HIGH : LOW);
    }
  }
}
