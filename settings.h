#pragma once

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
      #define PULSE_COUNT            47 
      #define BIT_OFFSET             187 
      #define OVERRIDE               11       
  #endif

  // -------- SCPH 7000 / 7500 / 9000 --------
  #ifdef SCPH_7000_7500_9000
    #define BIOS_PATCH
    #define SILENCE_THRESHOLD        15000
    #define CONFIRM_COUNTER_TARGET   1
    #define PULSE_COUNT              14    // (15 - 1)
    #define BIT_OFFSET               187   // (47 * 4) - 1
    #define OVERRIDE                 11    // (3 * 4) - 1     
  #endif

  // ----- SCPH 3500 / 5000 / 5500 -----
  #ifdef SCPH_3500_5000_5500
    #define BIOS_PATCH
    #define SILENCE_THRESHOLD        320000 // (32000 * 10)
    #define CONFIRM_COUNTER_TARGET   1
    #define PULSE_COUNT              83     // (84 - 1)
    #define BIT_OFFSET               187    // (47 * 4) - 1
    #define OVERRIDE                 11     // (3 * 4) - 1
  #endif

  // -------- SCPH 3000 --------
  #ifdef SCPH_3000
    #define BIOS_PATCH
    #define PHASE_TWO_PATCH
    #define SILENCE_THRESHOLD        15000
    #define CONFIRM_COUNTER_TARGET   9
    #define PULSE_COUNT              58     // (59 - 1)
    #define BIT_OFFSET               179    // (45 * 4) - 1
    #define OVERRIDE                 11     // (3 * 4) - 1
    #define CONFIRM_COUNTER_TARGET_2 206 
    #define PULSE_COUNT_2            41     // (42 - 1)
    #define BIT_OFFSET_2             191    // (48 * 4) - 1
    #define OVERRIDE_2               11     // (3 * 4) - 1
  #endif


  // -------- SCPH 1000 --------
  #ifdef SCPH_1000
    #define BIOS_PATCH
    #define PHASE_TWO_PATCH
    #define SILENCE_THRESHOLD        15000
    #define CONFIRM_COUNTER_TARGET   9
    #define PULSE_COUNT              90
    #define BIT_OFFSET               179
    #define OVERRIDE                 11
    #define CONFIRM_COUNTER_TARGET_2 222   
    #define PULSE_COUNT_2            69
    #define BIT_OFFSET_2             191
    #define OVERRIDE_2               11
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
#include "hardware/clocks.h"
#include <stdio.h>
#include <stdint.h>

// External request counter for SUB-Q tracking
extern volatile uint32_t request_counter;
extern volatile uint32_t SUBQBuffer[3];
/******************************************************************************************
 * FUNCTION    : CaptureSUBQLog
 * DESCRIPTION : Matrix display leveraging the global SUBQBuffer array.
 *               Outputs chronological hardware traces alongside a dynamic logical block
 *               that adapts layout formatting based on frame status (TOC vs DATA vs EROR).
 ******************************************************************************************/
void CaptureSUBQLog(bool crc_valid) {
    static bool header_printed = false;
    if (!header_printed) {
        header_printed = true;
        printf(" %-57s | %-10s\n", "RAW", "ASCII");
        printf(" %-4s | %-3s | %-20s | %-4s | %-6s | %-9s | %-8s | %-8s\n", 
           "CTRL", "ADR", "DATA", "CRC", "MODE", "TNO INDEX", "REL_TIME", "ABS_TIME");
        printf("------------------------------------------------------------------------------------\n");
    }

    // 1. PRINT FIXED HEX PAYLOAD BLOCK DIRECTLY FROM 32-BIT REGISTERS
    printf(" 0x%X  | 0x%X | %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X | %04X | ",
           (SUBQBuffer[0] >> 28) & 0x0F,                        // CTRL
           (SUBQBuffer[0] >> 24) & 0x0F,                        // ADR
           (SUBQBuffer[0] >> 24) & 0xFF,                        // DATA Byte 1
           (SUBQBuffer[0] >> 16) & 0xFF,                        // DATA Byte 2
           (SUBQBuffer[0] >> 8)  & 0xFF,                        // DATA Byte 3
           SUBQBuffer[0] & 0xFF,                                // DATA Byte 4
           (SUBQBuffer[1] >> 24) & 0xFF,                        // DATA Byte 5
           (SUBQBuffer[1] >> 16) & 0xFF,                        // DATA Byte 6
           (SUBQBuffer[1] >> 8)  & 0xFF,                        // DATA Byte 7
           SUBQBuffer[1] & 0xFF,                                // DATA Byte 8
           (SUBQBuffer[2] >> 24) & 0xFF,                        // DATA Byte 9
           (SUBQBuffer[2] >> 16) & 0xFF,                        // DATA Byte 10
           SUBQBuffer[2] & 0xFFFF                               // CRC
    );

    // 2. STATE INTERPOLATION MATRIX DE TON PROTOTYPE
    if (!crc_valid) {
        printf("                          ERROR");
    } 
    else if (((SUBQBuffer[0] >> 16) & 0xFF) == 0x00) {          // Mode Lead-In TOC (TNO == 00h)
        printf("TOC   |  00   %02X  | %02X:%02X:%02X | %02X:%02X:%02X",  
               (SUBQBuffer[0] >> 8) & 0xFF,                     // INDEX / POINT
               SUBQBuffer[0] & 0xFF,                            // r_min
               (SUBQBuffer[1] >> 24) & 0xFF,                    // r_sec
               (SUBQBuffer[1] >> 16) & 0xFF,                    // r_frm
               SUBQBuffer[1] & 0xFF,                            // a_min
               (SUBQBuffer[2] >> 24) & 0xFF,                    // a_sec
               (SUBQBuffer[2] >> 16) & 0xFF                     // a_frm
        );
    } 
    else { // DATA region active
        printf("DATA  |  %02X   %02X  | %02X:%02X:%02X | %02X:%02X:%02X", 
               (SUBQBuffer[0] >> 16) & 0xFF,                    // TNO
               (SUBQBuffer[0] >> 8) & 0xFF,                     // INDEX
               SUBQBuffer[0] & 0xFF, (SUBQBuffer[1] >> 24) & 0xFF, (SUBQBuffer[1] >> 16) & 0xFF, // REL TIME
               SUBQBuffer[1] & 0xFF, (SUBQBuffer[2] >> 24) & 0xFF, (SUBQBuffer[2] >> 16) & 0xFF  // ABS TIME
        );
    }

    printf("\n");
}


/******************************************************************************************
 * FUNCTION    : BoardDetectionLog
 * DESCRIPTION : Outputs the hardware diagnostics for the WFCK clock lines over Serial.
 ******************************************************************************************/
void BoardDetectionLog(uint32_t window, uint8_t mode, uint32_t inject) {
    printf("\n[WFCK DIAGNOSIS]\n");
    printf(" -> Remaining Window : %u\n", window);
    printf(" -> Detected Mode    : ");
    
    if (mode == 0)      printf("0 (Legacy / GATE High)\n");
    else if (mode == 1) printf("1 (Frequency Active)\n");
    else                printf("2 (Error / Stuck Low)\n");
    
    printf(" -> Inject Trigger   : %u\n", inject);
    printf("---------------------------------------\n\n");
}



void InjectLog() {     
    printf(" >> INJECT!\n");
}

#endif


// /*------------------------------------------------------------------------------------------------
//                Compilation message
// -----------------------------------------------------------------------------------------------*/


// --- Feature Status ---
#define STRING2(x) #x
#define STRING(x) STRING2(x)

#pragma message("Led pin: " STRING(LED_PIN))
#pragma message("Inject Trigger: " STRING(REQUEST_INJECT_TRIGGER))
#pragma message("Inject Gap: " STRING(REQUEST_INJECT_GAP))


#ifdef DEBUG_SERIAL_MONITOR
  #pragma message "Feature: Serial Debug Monitor ENABLED"
#endif

#ifdef PATCH_SWITCHE
  #pragma message "PATCH_SWITCHE selected"
#endif


// Show target console.
#if defined(SCPH_1000)
  #pragma message "SCPH_1000 NTSC-J"
#elif defined(SCPH_3000)
  #pragma message "SCPH_3000 NTSC-J"
#elif defined(SCPH_3500_5000_5500)
  #pragma message "SCPH_3500_5000_5500 NTSC-J"
#elif defined(SCPH_7000_7500_9000)
  #pragma message "SCPH_7000_7500_9000 NTSC-J"
#elif defined(SCPH_100)
  #pragma message "SCPH_100 NTSC-J"
#elif defined(SCPH_102)
  #pragma message "SCPH_102 PAL"
#elif defined(SCPH_xxx1)
  #pragma message "SCPH_xxx1 Generic NTSC-U/C"
#elif defined(SCPH_xxx2)
  #pragma message "SCPH_xxx2 Generic PAL"
#elif defined(SCPH_xxx3)
  #pragma message "SCPH_xxx3 Generic NTSC-J"
#elif defined(SCPH_5903)
  #pragma message "SCPH-5903 Video CD Dual-Interface"
#elif defined(SCPH_xxxx)
  #pragma message "SCPH_xxxx Universal Region Mode"
#else
  // Error if no console is uncommented
  #error "Console not selected! Please uncomment one SCPH model."
#endif



// SECURITY CHECK: Ensure only one console is selected
// If you get "not portable" warnings here, it's only because multiple models are active.
#if (defined(SCPH_1000) + defined(SCPH_3000) + defined(SCPH_3500_5000_5500) + \
     defined(SCPH_7000_7500_9000) + defined(SCPH_100) + \
     defined(SCPH_102) + defined(SCPH_xxx1) + defined(SCPH_xxx2) + \
     defined(SCPH_xxx3) + defined(SCPH_5903) + defined(SCPH_xxxx)) > 1
  #error "Too many consoles selected! Please uncomment ONLY ONE model."
#endif



