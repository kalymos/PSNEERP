#pragma once

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"    // Indispensable
#include "hardware/clocks.h" // Indispensable pour SetLED
#include "psneerp.pio.h"     // UNIQUEMENT le .h généré, JAMAIS le .pio
 


// Define pin numbers for the RP2040 - adjust these to match your wiring
#define PIN_WFCK                    2 
#define PIN_DATA                    3 
#define PIN_SUBQ                    4 
#define PIN_SQCK                    5   
#define PIN_CE                      6 
#define PIN_D2                      7 

#define PIN_BIOS_A                  14
#define PIN_BIOS_B                  15
#define PIN_TRIGGER_A               26
#define PIN_TRIGGER_B               27
#define PIN_REGION_A                28
#define PIN_REGION_B                29
  
#define LED_PIN                     16  // Default for Waveshare Pico Zero





  #if defined(SCPH_102)       || \
      defined(SCPH_100)       || \
      defined(SCPH_7000_7500_9000) || \
      defined(SCPH_7000)      || \
      defined(SCPH_3500_5000_5500) || \
      defined(SCPH_3000)      || \
      defined(SCPH_1000)

      #define PIN_AX                      0    // GP6
      #define PIN_AY                      1    // GP7
      #define PIN_DX                      2    // GP8

    // --- Switch de désactivation (Bypass) ---
    #ifdef PATCH_SWITCHE
        #define PIN_SWITCH                9    // GP9
    #endif

#endif 
