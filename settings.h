//#pragma once

/*
 *
 *
 */

/*------------------------------------------------------------------------------------------------
           Specific parameter section for BIOS patches
------------------------------------------------------------------------------------------------*/



  // ------ SCPH 100 / 102 ------
  #if defined(SCPH_100) || \
      defined(SCPH_102)
    #define BIOS_PATCH
    #define SILENCE_THRESHOLD      15000
    #define CONFIRM_COUNTER_TARGET 8
    #define PULSE_COUNT_PIO        47 
    #define BIT_OFFSET_VAL         187 
    #define OVERRIDE_VAL           11       
  #endif

  // -------- SCPH 7000 / 7500 / 9000 --------
  #ifdef SCPH_7000_7500_9000
    #define BIOS_PATCH
  #define SILENCE_THRESHOLD        15000
  #define CONFIRM_COUNTER_TARGET   1
  #define PULSE_COUNT_PIO          14    // (15 - 1)
  #define BIT_OFFSET_VAL           187   // (47 * 4) - 1
  #define OVERRIDE_VAL             11    // (3 * 4) - 1     
  #endif

  // ----- SCPH 3500 / 5000 / 5500 -----
  #ifdef SCPH_3500_5000_5500
    #define BIOS_PATCH
  #define SILENCE_THRESHOLD        320000 // (32000 * 10)
  #define CONFIRM_COUNTER_TARGET   1
  #define PULSE_COUNT_PIO          83     // (84 - 1)
  #define BIT_OFFSET_VAL           187    // (47 * 4) - 1
  #define OVERRIDE_VAL             11     // (3 * 4) - 1
  #endif

  // -------- SCPH 3000 --------
  #ifdef SCPH_3000
    #define BIOS_PATCH
  #define PHASE_TWO_PATCH
  #define SILENCE_THRESHOLD        15000
  #define CONFIRM_COUNTER_TARGET   9
  #define PULSE_COUNT_PIO          58     // (59 - 1)
  #define BIT_OFFSET_VAL           179    // (45 * 4) - 1
  #define OVERRIDE_VAL             11     // (3 * 4) - 1
  #define CONFIRM_COUNTER_TARGET_2 206 
  #define PULSE_COUNT_2_PIO        41     // (42 - 1)
  #define BIT_OFFSET_2_VAL         191    // (48 * 4) - 1
  #define OVERRIDE_2_VAL           11     // (3 * 4) - 1
  #endif


  // -------- SCPH 1000 --------
  #ifdef SCPH_1000
  #define BIOS_PATCH
  #define PHASE_TWO_PATCH
  #define SILENCE_THRESHOLD        15000
  #define CONFIRM_COUNTER_TARGET   9
  #define PULSE_COUNT_PIO          90     // (91 - 1)
  #define BIT_OFFSET_VAL           179    // (45 * 4) - 1
  #define OVERRIDE_VAL             11     // (3 * 4) - 1
  #define CONFIRM_COUNTER_TARGET_2 222   
  #define PULSE_COUNT_2_PIO        69     // (70 - 1)
  #define BIT_OFFSET_2_VAL         191    // (48 * 4) - 1
  #define OVERRIDE_2_VAL           11     // (3 * 4) - 1
  #endif


/*------------------------------------------------------------------------------------------------
                  Region Settings Section
------------------------------------------------------------------------------------------------*/

#if defined(SCPH_100)            || \
    defined(SCPH_7000_7500_9000) || \
    defined(SCPH_3500_5000_5500) || \
    defined(SCPH_3000)           || \
    defined(SCPH_1000)           || \
    defined(SCPH_xxx3)           || \
    defined(SCPH_5903)

  #define INJECT_SCEx 0   // NTSC-J

#elif defined(SCPH_xxx1)

  #define INJECT_SCEx 1   // NTSC-U/C 

#elif defined(SCPH_xxx2) || \
      defined(SCPH_102)

  #define INJECT_SCEx 2   // PAL 
    
#elif defined(SCPH_xxxx)

  #define INJECT_SCEx 3   // Universal: NTSC-J -> NTSC-U/C -> PAL

#endif

/*------------------------------------------------------------------------------------------------
               serial debug section
------------------------------------------------------------------------------------------------*/

#if defined(DEBUG_SERIAL_MONITOR)
#include <stdio.h>
#include "hardware/clocks.h"


// Variable externe
extern volatile uint32_t request_counter;

void BoardDetectionLog(uint32_t window_result, uint8_t Wfck_mode, uint8_t region) {
    static const char* regionNames[] = {
        "NTSC-J",    // 0
        "NTSC-U/C",  // 1
        "PAL",       // 2
        "Universal"  // 3
    };

    printf("\n");
    printf(" CPU Speed: %lu MHz\n", clock_get_hz(clk_sys) / 1000000L);
    printf(" Sync Window: %lu\n", window_result); 
    printf(" WFCK Mode: %u\n", Wfck_mode);
    printf(" Region ID: %s\n", regionNames[region]);
    printf("\n");
}

void CaptureSUBQLog(uint32_t *dataBuffer32) {
    static uint32_t errorCount = 0;

    // --- FILTRAGE D'ERREUR (32-bit rapide) ---
    // Vérifie si les 3 mots (96 bits) sont à zéro en seulement 3 tests
    if (dataBuffer32[0] == 0 && dataBuffer32[1] == 0 && dataBuffer32[2] == 0) {
        errorCount++;
        return;
    }

    // --- AFFICHAGE HEXADÉCIMAL ---
    // On itère sur les 3 mots de 32 bits
    for (uint8_t i = 0; i < 3; i++) {
        uint32_t word = dataBuffer32[i];
        // Extraction et affichage des 4 octets de chaque mot (LSB first)
        printf("%02X %02X %02X %02X ", 
               (uint8_t)(word & 0xFF), 
               (uint8_t)((word >> 8) & 0xFF), 
               (uint8_t)((word >> 16) & 0xFF), 
               (uint8_t)((word >> 24) & 0xFF));
    }

    // Affichage du compteur global
    printf("| Hyst: %lu\n", request_counter);
}

void InjectLog() {     
    printf("           INJECT ! \n");
}

#endif

#endif

// /*------------------------------------------------------------------------------------------------
//                Compilation message
// -----------------------------------------------------------------------------------------------*/


#ifdef LED_RUN
  #pragma message "Feature: Status LED ENABLED"
#endif

// SECURITY CHECK: Ensure only one console is selected
// If you get "not portable" warnings here, it's only because multiple models are active.
#if (defined(SCPH_1000) + defined(SCPH_3000) + defined(SCPH_3500_5000_5500) + \
     defined(SCPH_7000_7500_9000) + defined(SCPH_100) + \
     defined(SCPH_102) + defined(SCPH_xxx1) + defined(SCPH_xxx2) + \
     defined(SCPH_xxx3) + defined(SCPH_5903) + defined(SCPH_xxxx)) > 1
  #error "Too many consoles selected! Please uncomment ONLY ONE model."
#endif

// Show target console.
#if defined(SCPH_1000)
  #pragma message "Target Console: SCPH-1000 (NTSC-J)"
#elif defined(SCPH_3000)
  #pragma message "Target Console: SCPH-3000 (NTSC-J)"
#elif defined(SCPH_3500_5000_5500)
  #pragma message "Target Console: SCPH-3500/5000/5500 (NTSC-J)"
#elif defined(SCPH_7000_7500_9000)
  #pragma message "Target Console: SCPH-7500/9000 (NTSC-J)"
#elif defined(SCPH_100)
  #pragma message "Target Console: SCPH-100 (NTSC-J)"
#elif defined(SCPH_102)
  #pragma message "Target Console: SCPH-102 (PAL)"
#elif defined(SCPH_xxx1)
  #pragma message "Target Console: SCPH_xxx1 Generic NTSC-U/C"
#elif defined(SCPH_xxx2)
  #pragma message "Target Console: SCPH_xxx2 Generic PAL"
#elif defined(SCPH_xxx3)
  #pragma message "Target Console: SCPH_xxx3 Generic NTSC-J"
#elif defined(SCPH_5903)
  #pragma message "Target Console: SCPH-5903 (Video CD Dual-Interface)"
#elif defined(SCPH_xxxx)
  #pragma message "Target Console: SCPH_xxxx Universal Region Mode"
#else
  // Error if no console is uncommented
  #error "Console not selected! Please uncomment one SCPH model."
#endif

// --- Feature Status ---

#ifdef DEBUG_SERIAL_MONITOR
  #pragma message "Feature: Serial Debug Monitor ENABLED"
#endif


