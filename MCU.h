

#pragma once

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "ws2812.pio.h"


#define PIN_NEOPIXEL 16

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
      



// Variable globale pour le PIO de la LED
PIO pioLED = pio1; // Utilise le second bloc PIO pour ne pas gêner SUBQ/BIOS
uint smLED = 0;


// Define pin numbers for the RP2040 - adjust these to match your wiring
#define PIN_DATA                    2    // GP2
#define PIN_WFCK                    3    // GP3
#define PIN_SQCK                    4    // GP4  
#define PIN_SUBQ                    5    // GP5

// 2. Initialisation (Reset vers contrôleur SIO)
#define PIN_DATA_INIT               gpio_init(PIN_DATA)
#define PIN_WFCK_INIT               gpio_init(PIN_WFCK)
#define PIN_SQCK_INIT               gpio_init(PIN_SQCK)
#define PIN_SUBQ_INIT               gpio_init(PIN_SUBQ)

// Float configuration
#define PIN_DATA_FLOAT              gpio_disable_pulls(PIN_DATA)
#define PIN_WFCK_FLOAT              gpio_disable_pulls(PIN_WFCK)
#define PIN_SQCK_FLOAT              gpio_disable_pulls(PIN_SQCK)
#define PIN_SUBQ_FLOAT              gpio_disable_pulls(PIN_SUBQ)

// Input configuration
#define PIN_DATA_INPUT              gpio_set_dir(PIN_DATA, GPIO_IN)
#define PIN_WFCK_INPUT              gpio_set_dir(PIN_WFCK, GPIO_IN)
#define PIN_SQCK_INPUT              gpio_set_dir(PIN_SQCK, GPIO_IN)
#define PIN_SUBQ_INPUT              gpio_set_dir(PIN_SUBQ, GPIO_IN)
                          
// Output configuration
#define PIN_DATA_OUTPUT             gpio_set_dir(PIN_DATA, GPIO_OUT)
#define PIN_WFCK_OUTPUT             gpio_set_dir(PIN_WFCK, GPIO_OUT)
           
// Setting pins high
#define PIN_DATA_SET                gpio_put(PIN_DATA, true)
             
// Setting pins low
#define PIN_DATA_CLEAR              gpio_put(PIN_DATA, false)
#define PIN_WFCK_CLEAR              gpio_put(PIN_WFCK, false)
                         
// Reading pin states
#define PIN_SQCK_READ               gpio_get(PIN_SQCK)
#define PIN_SUBQ_READ               gpio_get(PIN_SUBQ)
#define PIN_WFCK_READ               gpio_get(PIN_WFCK)


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


    // --- Initialisation (Reset SIO) ---
    #define PIN_AX_INIT                 gpio_init(PIN_AX)
    #define PIN_AY_INIT                 gpio_init(PIN_AY)
    #define PIN_DX_INIT                 gpio_init(PIN_DX)

    // --- Configuration Direction ---
    #define PIN_AX_INPUT                gpio_set_dir(PIN_AX, GPIO_IN)
    #define PIN_AY_INPUT                gpio_set_dir(PIN_AY, GPIO_IN)
    #define PIN_DX_INPUT                gpio_set_dir(PIN_DX, GPIO_IN)
    #define PIN_DX_OUTPUT               gpio_set_dir(PIN_DX, GPIO_OUT)

    // --- Configuration Électrique ---
    #define PIN_AX_FLOAT                gpio_disable_pulls(PIN_AX)
    #define PIN_AY_FLOAT                gpio_disable_pulls(PIN_AY)
    #define PIN_DX_FLOAT                gpio_disable_pulls(PIN_DX)

    // --- État des sorties ---
    #define PIN_DX_SET                  gpio_put(PIN_DX, true)
    #define PIN_DX_CLEAR                gpio_put(PIN_DX, false)

    // --- Lecture des entrées ---
    #define PIN_AX_READ                 gpio_get(PIN_AX)
    #define PIN_AY_READ                 gpio_get(PIN_AY)

    // --- Switch de désactivation (Bypass) ---
    #ifdef PATCH_SWITCHE
        #define PIN_SWITCH              9    // GP9
        #define PIN_SWITCH_INIT         gpio_init(PIN_SWITCH)
        #define PIN_SWITCH_INPUT        { gpio_set_dir(PIN_SWITCH, GPIO_IN); gpio_pull_up(PIN_SWITCH); }
        #define PIN_SWITCH_READ         gpio_get(PIN_SWITCH)
    #endif

#endif 
