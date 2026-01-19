#include <Arduino.h>
#include "pdmAudio.h"

pdmAudio pdm;

// Create custom UART on UART0: GP0 (TX) and GP1 (RX)
UART mySerial(0, 1, NC, NC);

bool ditLEDState = false;
bool dahLEDState = false;


void setup() {
  pdm.begin(14);
  pdm.USB_UAC();  // This blocks - comment out if not using USB audio

  // LEDs
  pinMode(2, OUTPUT);
  pinMode(8, OUTPUT);

  // 3.5mm output
  pinMode(19, OUTPUT);

  // Morse Inputs
  pinMode(4, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);

  // Paddle inputs
  pinMode(24, INPUT_PULLUP);
  pinMode(25, INPUT_PULLUP);

  mySerial.begin(115200);
}

void loop() {


  if (ditLEDState == true) {
    int16_t left = pdm.sine_lu(440);
    int16_t right = pdm.sine_lu(440);

    pdm.USBtransfer(left, right);
  }
  else {
    int16_t left = pdm.sine_lu(0);
    int16_t right = pdm.sine_lu(0);

    pdm.USBtransfer(left, right);
  }

  int button1 = digitalRead(4);
  int button2 = digitalRead(6);

  int paddleDit = digitalRead(24);
  int paddleDah = digitalRead(25);

  if (button1 == LOW) {
    //mySerial.println("Button 1 Pressed");
    ditLEDState = true;
    digitalWrite(2, ditLEDState ? HIGH : LOW);
    //digitalWrite(8, ditLEDState ? HIGH : LOW);
  } else {
    if (ditLEDState) {
      ditLEDState = false;
      digitalWrite(2, ditLEDState ? HIGH : LOW);
      //digitalWrite(8, ditLEDState ? HIGH : LOW);
    }
  }
  if (button2 == LOW) {
    dahLEDState = true;
    digitalWrite(8, dahLEDState ? HIGH : LOW);
  } else {
    if (dahLEDState) {
      dahLEDState = false;
      digitalWrite(8, dahLEDState ? HIGH : LOW);
    }
  }
}
