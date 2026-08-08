#include "pdmAudio.h"

#if defined ARDUINO_ARCH_MBED_RP2040 || defined ARDUINO_ARCH_RP2040 

#include "pdm.pio.h"
#include "pico/multicore.h"

#ifdef ARDUINO_ARCH_MBED_RP2040
#include "usb_phy_api.h"

// USBAudio with a custom iProduct string (base class always reports "Mbed Audio")
class MorseUSBAudio : public USBAudio {
public:
    MorseUSBAudio(uint32_t frequency_rx, uint8_t channel_count_rx, uint32_t frequency_tx, uint8_t channel_count_tx)
        : USBAudio(get_usb_phy(), frequency_rx, channel_count_rx, frequency_tx, channel_count_tx, 10, 0x7bb8, 0x1111, 0x0100)
    {
        connect();
    }

protected:
    virtual const uint8_t *string_iproduct_desc() override {
        static const uint8_t stringIproductDescriptor[] = {
            0x16,               // bLength
            STRING_DESCRIPTOR,  // bDescriptorType
            'M', 0, 'o', 0, 'r', 0, 's', 0, 'e', 0, ' ', 0, 'C', 0, 'a', 0, 'r', 0, 'd', 0 // bString iProduct - "Morse Card"
        };
        return stringIproductDescriptor;
    }
};
#endif


void core1_worker() {
 PIO pio = pio1;
 uint sm;
 
 // wait untill pin number is received
 while(!multicore_fifo_rvalid()){     
 }
 uint32_t pin = multicore_fifo_pop_blocking();
 
 // Set the appropriate clock
 set_sys_clock_khz(115200, false);
 uint offset = pio_add_program(pio, &pdm_program);
 sm = pio_claim_unused_sm(pio, true);

 // PIO configuration
 pio_sm_config c = pdm_program_get_default_config(offset);
 sm_config_set_out_pins(&c, pin, 1);
 pio_gpio_init(pio, pin);
 pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

 sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
 sm_config_set_clkdiv(&c, 115200 * 1000 / (48000.0 * 32));
 sm_config_set_out_shift(&c, true, true, 32);
 pio_sm_init(pio, sm, offset, &c);
 pio_sm_set_enabled(pio, sm, true);
    
    
 uint32_t a = 0;
 int16_t pinput = 0;
 SDM sdm;

  //startup pop suppression
  for (int i = -32767; i < 0; i++) {
    a = sdm.o2_os32(i);
    while (pio_sm_is_tx_fifo_full(pio, sm)) {}
    pio->txf[sm] = a;
  }

  while (1) {
    //if fifo empty modulate the previous value to keep voltage constant
    // helps prevents clicks and pops I think

    if (pio_sm_is_tx_fifo_empty(pio, sm)) {
      pio->txf[sm] = a;
      pio->txf[sm] = a;
      pio->txf[sm] = a;
      pio->txf[sm] = a;
      pio->txf[sm] = a;
      pio->txf[sm] = a;
      pio->txf[sm] = a;
      pio->txf[sm] = a;
    }
    
    // if other core sends value
    if (multicore_fifo_rvalid()) {
      uint32_t rec = multicore_fifo_pop_blocking();

      //save previous value so we can stuff buffer with it if music paused.
      pinput = (int16_t)(rec);
      a = sdm.o4_os32_df2(pinput);
      //a = sdm.o1_os32(pinput);

      //write to state PIO when its not full
      while (pio_sm_is_tx_fifo_full(pio, sm)) {}
      pio->txf[sm] = a;
    }
  }
}



pdmAudio::pdmAudio() {
 #ifdef ARDUINO_ARCH_MBED_RP2040
 audio = nullptr;
 #endif
}

void pdmAudio::begin(uint32_t pin) {
    
     float delta = (2.0*PI/6000.0);
    for (int i=0;i<6000;i++){
        sina[i]=(int16_t)(sin(i*delta)*32767);
    }
  //delay(1000);
	// less noisy power supply
  #ifdef ARDUINO_ARCH_MBED_RP2040 
  _gpio_init(23);
  #elif  defined ARDUINO_ARCH_RP2040 
  gpio_init(23);
  #endif
  
  gpio_set_dir(23, GPIO_OUT);
  gpio_put(23, 1);
  multicore_launch_core1(core1_worker);
  multicore_fifo_push_blocking((uint32_t)(pin));
  
  //delay(1000);
  
}

void pdmAudio::USB_UAC() {
 #ifdef ARDUINO_ARCH_MBED_RP2040
 audio= new MorseUSBAudio(48000, 2, 48000, 2);
 #endif
}

void pdmAudio::USBwrite() {
    #ifdef ARDUINO_ARCH_MBED_RP2040
     if (audio == nullptr) return;
     if (audio->read(myRawBuffer, sizeof(myRawBuffer))) {
      int16_t *lessRawBuffer = (int16_t *)myRawBuffer;
      for (int i = 0; i < 24; i++) {
        // the left value;
        int16_t outL = *lessRawBuffer;
        lessRawBuffer++;
        // the right value
        int16_t outR = *lessRawBuffer;
        lessRawBuffer++;
        //mono value
        int16_t mono = (outL + outR) / 2;
        pdmAudio::write(mono);
      }
    }
    #endif
}

void pdmAudio::write(int16_t mono) {
	multicore_fifo_push_blocking((uint32_t)(mono));
}

void pdmAudio::USBtransfer(int16_t left,int16_t right) {
  #ifdef ARDUINO_ARCH_MBED_RP2040
  if (audio == nullptr) return;
  if(nBytes>95){
    uint8_t *pcBuffer =  (uint8_t *)pcBuffer16;
    audio->write(pcBuffer, 96);
    pcCounter =0;
    nBytes =0;
  }
  pcBuffer16[pcCounter]=left;
  pcCounter++;
  nBytes+=2;
  
  pcBuffer16[pcCounter]=right;
  pcCounter++;
  nBytes+=2;
  #endif
}

int16_t pdmAudio::sine_lu(uint32_t freq){
  step = freq >> 3;
  currentStep = currentStep +step;
  if(currentStep>5999){
   currentStep = currentStep-6000;
  }
  int16_t val =  sina[currentStep];
  return val;
}

void pdmAudio::tone(uint32_t freq, float duration){
  step = freq >> 3;
  nsamps = (uint32_t)(duration/dt);
  for (uint32_t i = 0;i<nsamps;i++){
  currentStep = currentStep +step;
  if(currentStep>5999){
    currentStep = currentStep-6000;
  }
  int16_t val =  sina[currentStep];
  write(val);
  }
}



#endif