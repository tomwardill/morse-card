#include <Arduino.h>
#include "pdmAudio.h"

pdmAudio pdm;


void setup() {
  pdm.begin(14);
  pdm.USB_UAC();
}

void loop() {
  int16_t left = pdm.sine_lu(440);
  int16_t right = pdm.sine_lu(440);
  pdm.USBtransfer(left, right);
}
