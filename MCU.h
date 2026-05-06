#pragma once

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"    // Indispensable
#include "hardware/clocks.h" // Indispensable pour SetLED
#include "psneerp.pio.h"     // UNIQUEMENT le .h généré, JAMAIS le .pio
 

// Couleurs préselectionnées (Format 0xGGRRBB00)

#define LED_OFF       0x00000000

#define LED_RED       0x00FF0000
#define LED_GREEN     0xFF000000
#define LED_BLUE      0x0000FF00

// Couleurs secondaires
#define LED_YELLOW    0xFFFF0000
#define LED_CYAN      0xFF00FF00
#define LED_MAGENTA   0x00FFFF00
#define LED_WHITE     0xFFFFFF00

// Couleurs d'état avancées
#define LED_ORANGE    0x80FF0000 
#define LED_PURPLE    0x00808000 
#define LED_PINK      0x30FF6000 
#define LED_AQUA      0xFF008000  
      

// Define pin numbers for the RP2040 - adjust these to match your wiring
#define PIN_DATA                    2    // GP2
#define PIN_WFCK                    3    // GP3
#define PIN_SQCK                    4    // GP4  
#define PIN_SUBQ                    5    // GP5

#define PIN_DEBUG_TX 10

  #if defined(SCPH_102)       || \
      defined(SCPH_100)       || \
      defined(SCPH_7000_7500_9000) || \
      defined(SCPH_7000)      || \
      defined(SCPH_3500_5000_5500) || \
      defined(SCPH_3000)      || \
      defined(SCPH_1000)

      #define PIN_AX                      6    // GP6
      #define PIN_AY                      7    // GP7
      #define PIN_DX                      8    // GP8

    // --- Switch de désactivation (Bypass) ---
    #ifdef PATCH_SWITCHE
        #define PIN_SWITCH                9    // GP9
    #endif

#endif 
