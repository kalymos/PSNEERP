//                           P.S.N.E.E.R.P. 0.1
/*********************************************************************************************************************
 *   CONSOLE MODEL SELECTION (SCPH Hardware Configuration)
 *********************************************************************************************************************/
/*--------------------------------------------------------------------------------------------------------------------
 * Models below do not require BIOS patching. 
 * Standard USB injection is supported.
 *
 *  SCPH model number //  region code | region
 *--------------------------------------------------------------------------------------------------------------------*/
// #define SCPH_xxx1  //  NTSC U/C    | America.
 #define SCPH_xxx2  //  PAL         | Europ.
// #define SCPH_xxx3  //  NTSC J      | Asia.
// #define SCPH_xxxx  //  Universal

// #define SCPH_5903  //  NTSC J      | Asia VCD:


/*------------------------------------------------------------------------------------------------------------------
 * WARNING: These models REQUIRE a BIOS patch.
 *
 * ISP programming is MANDATORY. 
 * The Arduino bootloader introduces a delay that prevents the BIOS patch injection.
 * Using an ISP programmer eliminates this delay.
 * 
 * Note: BIOS version is more critical than the SCPH number for patch success.
 *-------------------------------------------------------------------------------------------------------------------
 *
 *                              // Data pin |                Adres pin            |
 *   SCPH model number          //          |    32-pin BIOS   |   40-pin BIOS    | BIOS version
 *-------------------------------------------------------------------------------------------------------------------*/
// #define SCPH_102             // DX - D0  | AX - A7          |                  | 4.4e - CRC 0BAD7EA9, 4.5e -CRC 76B880E5
// #define SCPH_100             // DX - D0  | AX - A7          |                  | 4.3j - CRC F2AF798B
// #define SCPH_7000_7500_9000  // DX - D0  | AX - A7          |                  | 4.0j - CRC EC541CD0
// #define SCPH_3500_5000_5500  // DX - D0  | AX - A16         | AX - A15         | 3.0j - CRC FF3EEB8C, 2.2j - CRC 24FC7E17, 2.1j - CRC BC190209 
// #define SCPH_3000            // DX - D5  | AX - A7, AY - A8 | AX - A6, AY - A7 | 1.1j - CRC 3539DEF6 
// #define SCPH_1000            // DX - D5  | AX - A7, AY - A8 | AX - A6, AY - A7 | 1.0j - CRC 3B601FC8

/*******************************************************************************************************************
 *                           Options
 *******************************************************************************************************************/

 // --- Hardware Pins ---
#define LED_PIN  16  // Default for Waveshare Pico Zero

#define REQUEST_INJECT_TRIGGER 500 // Now coupled with REQUEST_INJECT_GAP; allows for higher trigger
/*
 * TRIGGER CALIBRATION:
 * - Lower values (<5): Possible, but not beneficial.
 * - The value of 11 for REQUEST_INJECT_TRIGGER must not be exceeded on Japanese models.
 *   This causes problems with disks that have anti-mod protection; it's less noticeable on other models.
 * - Higher values (15-20-25-30): Possible for older or weak CD-ROM laser units.
 */

 #define REQUEST_INJECT_GAP 5      // Stealth interval (must be 4-8 AND < REQUEST_INJECT_TRIGGER)
/*
 * NOTE: REQUEST_INJECT_GAP defines the "cool-off" period between injections.
 * - Optimal range: 4 to 8 (for natural CD timing & anti-mod bypass).
 * - Constraint: Must ALWAYS be lower than REQUEST_INJECT_TRIGGER.
 */

 #define PATCH_SWITCHE    // This allows the user to disable the BIOS patch on-the-fly.
/*
 * This allows you to bypass the memory card blocking problems on the SCPH-7000.
 * - Configure Pin D5 as Input.
 * - Enable internal Pull-up.
 * - Exit immediately the patch BIOS if the switch pulls the pin to GND
 */

 #define DEBUG_SERIAL_MONITOR  // Enables serial monitor output. 
/*

 *
 */

/******************************************************************************************************************
 *           Summary of practical information. Fuses. Pinout
 *******************************************************************************************************************

 *******************************************************************************************************************/

/*******************************************************************************************************************
 *                        pointer and variable section
 *******************************************************************************************************************/

#include "MCU.h"
#include "settings.h"
#include "psneerp.pio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h" 

uint offsetPATCH;  


// --- PROTOTYPES DES FONCTIONS ---
// --- PROTOTYPES DES FONCTIONS ---
void BoardDetection(void); // Votre fonction principale de détection (sans paramètre)
void BoardDetectionLog(uint32_t window, uint8_t mode, uint32_t inject); // La vraie fonction de log à 3 paramètres
void CaptureSUBQ(void);
void FilterSUBQSamples(void);
void CaptureSUBQLog(bool crc_valid);
volatile bool subq_new_frame_ready = false;


// --- VARIABLES GLOBALES DU PIPELINE SUBQ (SANS DOUBLONS) ---
volatile uint32_t SUBQBuffer[3] = {0, 0, 0}; 
#define SUBQBuffer32 SUBQBuffer

volatile uint32_t last_pulse_time = 0;
volatile bool pio_glitch_detected = false;

// Note : wfck_mode et request_counter sont déjà définis aux lignes 108/109.
// Ne rajoutez aucune autre ligne les mentionnant ici pour éviter les "redefinition of".

// INSTANCES MATÉRIELLES RP2040
PIO pio = pio0;
uint sm = 0;


// --- CONFIGURATION DU SIGNAL HORLOGE ---
volatile uint8_t wfck_mode = 0; // 0 = Standard / Détection automatique

// Variables de contrôle d'injection d'origine
volatile uint32_t request_counter = 0;



 #if defined(DEBUG_SERIAL_MONITOR)
   uint16_t global_window = 0; // Stores the remaining cycles from the detection window
 #endif

/*******************************************************************************************************************
 *                         Code section
 ********************************************************************************************************************/



/*******************************************************************************************************************
*          NEOPIXEL / WS2812 LED CONTROL SECTION
*          Handles visual feedback for system status and SUBQ stability.
*********************************************************************************************************************/

// Global variables for the LED PIO
PIO pioLED = pio1; // Use the second PIO block to avoid interference with SUBQ/BIOS
uint smLED = 0;

void NeoPixel_Init() {
    // 1. Charger le code dans le PIO
    uint offset = pio_add_program(pioLED, &ws2812_program);
    
    // 2. Réserver une State Machine
    smLED = pio_claim_unused_sm(pioLED, true);
    
    // 3. Configurer tout le reste (GPIO, Clock, FIFO, Shift) via ton .h
    ws2812_program_init(pioLED, smLED, offset, LED_PIN, 800000.0f, false);
}


/**
 * Set LED color at full brightness (standard format 0xGGRRBB00)
 */
void SetLED(uint32_t color) {
    // On décale de 8 bits vers la gauche pour aligner GGRRBB sur le haut du registre 32 bits
    pio_sm_put_blocking(pioLED, smLED, color << 8u);
}


/**
 * Set LED color with direct intensity (0-255)
 */
void SetLEDDynamic(uint32_t color, uint8_t intensity) {
    if (intensity == 0) {
        pio_sm_put_blocking(pioLED, smLED, 0);
        return;
    }

    // Extract color components and apply the pre-calculated intensity
    uint32_t g = (((color >> 24) & 0xFF) * intensity) >> 8;
    uint32_t r = (((color >> 16) & 0xFF) * intensity) >> 8;
    uint32_t b = (((color >> 8) & 0xFF) * intensity) >> 8;

    // Reconstruct and shift for PIO
    uint32_t final_color = (g << 16) | (r << 8) | b;
    pio_sm_put_blocking(pioLED, smLED, final_color << 8u);
}


/****************************************************************************************
 * FUNCTION    : Bios_Patching()
 *
 * OPERATION   : Real-time Data Bus (DX) override via Address Bus (AX / AY)
 *
 * KEY PHASES:
 *    1. STABILIZATION & ALIGNMENT (AX): 
 *       Synchronizes the execution pointer with the AX rising edge to establish 
 *        a deterministic hardware timing reference.
 *
 *    2. SILENCE DETECTION (BOOT STAGE): 
 *       Validates consecutive silent windows (SILENCE_THRESHOLD) to identify 
 *       the specific boot stage before the target address call.
 *
 *    3. HARDWARE COUNTING & OVERDRIVE (AX): 
 *       Engages INT0 to count AX pulses. On the final pulse, triggers a 
 *       bit-aligned delay to force a custom state on the DX line.
 *
 *    4. SECONDARY SILENCE (GAP DETECTION): 
 *       If PHASE_TWO_PATCH is active, monitors for a secondary silent gap 
 *       (CONFIRM_COUNTER_TARGET_2) between patching windows.
 *
 *    5. SECONDARY OVERDRIVE (AY): 
 *       Engages INT1 (AY) for the final injection stage, synchronizing the 
 *       patch with the secondary memory address cycles.
 *
 *  CRITICAL TIMING & TIMER-LESS ALIGNMENT:
 *    - DETERMINISTIC SILENCE: Uses cycle-accurate polling to filter boot jitter 
 *      and PS1 initialization noise, replacing unstable hardware timers.
 *
 *    - CYCLE STABILIZATION (16MHz LIMIT): Uses '__builtin_avr_delay_cycles' to 
 *      prevent compiler reordering. At 16MHz, the CPU has zero margin; a single 
 *      instruction displacement would break high-speed bus alignment.
 *
 **************************************************************************************/


#ifdef BIOS_PATCH

volatile uint8_t patch_stage = 0; // Track patching progress
uint smPATCH = 0;                 // PIO state machine ID for BIOS patching

/**
 * PIO IRQ Handler: Increments patch stage when the PIO sends an interrupt
 */
void on_pio_patch_irq() {
    // Check if PIO0 generated interrupt 0
    if (pio_interrupt_get(pio0, 0)) {
        patch_stage++; 
        pio_interrupt_clear(pio0, 0);
    }
}

void Bios_Patching(void) {
    // --- 1. HARDWARE BYPASS CHECK ---
    #if defined(PATCH_SWITCHE)
        gpio_pull_up(PIN_SWITCH);
        sleep_us(10);
        if (gpio_get(PIN_SWITCH) == 0) return; 
    #endif

    uint8_t current_confirms = 0;
    patch_stage = 0; // Reset sync flag for IRQ

    // --- 2. PHASE 1: STABILIZATION & ALIGNMENT (AX) ---
    // Establish a deterministic hardware timing reference.
    if (gpio_get(PIN_AX) != 0) {
        while (gpio_get(PIN_AX) != 0); // Wait for AX Falling
        while (gpio_get(PIN_AX) == 0); // Wait for AX Rising
    } else {
        while (gpio_get(PIN_AX) == 0); // Wait for AX Rising
    }

    // --- 3. PHASE 2: SILENCE DETECTION ---
    // Validate the boot stage by identifying the required silence windows.
    SetLEDDynamic(LED_RED, 100); 
    while (current_confirms < CONFIRM_COUNTER_TARGET) {
        uint32_t count = SILENCE_THRESHOLD; 
        while (count > 0) {
            if (gpio_get(PIN_AX) != 0) {
                while (gpio_get(PIN_AX) != 0); // Reset on activity
                break; 
            }
            count--;
        }
        if (count == 0) current_confirms++; // One silence window validated
    }

    // --- 4. PHASE 3: AX INJECTION (HAND OVER TO PIO) ---
    // Initialize PIO for AX monitoring and DX injection
    bios_patch_program_init(pio0, smPATCH, offsetPATCH, PIN_DX, PIN_AX);
    
    // Load parameters (Pulse count, timing offset, override duration)
    pio_sm_put_blocking(pio0, smPATCH, PULSE_COUNT); 
    pio_sm_put_blocking(pio0, smPATCH, BIT_OFFSET);
    pio_sm_put_blocking(pio0, smPATCH, OVERRIDE);

    // Busy-wait for PIO IRQ (Phase 3 completion)
    while (patch_stage < 1) tight_loop_contents();

    // --- 5. PHASE 4 & 5: SECONDARY PATCHING SEQUENCE ---
    #ifdef PHASE_TWO_PATCH
        current_confirms = 0;

        // Monitor for the specific silent gap before the second patch window
        while (current_confirms < CONFIRM_COUNTER_TARGET_2) {
            uint32_t count = SILENCE_THRESHOLD;
            while (count > 0) {
                if (gpio_get(PIN_AX) != 0) { // Still monitoring AX for silence
                    while (gpio_get(PIN_AX) != 0);
                    break;
                }
                count--;
            }
            if (count == 0) current_confirms++;
        }

        // --- PHASE 5: AY INJECTION ---
        SetLEDDynamic(LED_ORANGE, 100);
        
        // Switch PIO source from AX to AY
        // Ajoute offsetPATCH comme 3ème argument
        bios_patch_switch_source(pio0, smPATCH, offsetPATCH, PIN_AY);
        
        // Send secondary parameters to PIO
        pio_sm_put_blocking(pio0, smPATCH, PULSE_COUNT_2);
        pio_sm_put_blocking(pio0, smPATCH, BIT_OFFSET_2);
        pio_sm_put_blocking(pio0, smPATCH, OVERRIDE_2);

        // Wait for final completion
        while (patch_stage < 2) tight_loop_contents();
    #endif
}


#endif

/*******************************************************************************************************************
 * FUNCTION    : BoardDetection
 * 
 * DESCRIPTION : 
 *    Distinguishes motherboard generations (PU-7 through PU-18) by analyzing 
 *    the behavior of the WFCK signal.
 *
 * SIGNAL MODES & LED INDICATORS:
 *    - Mode 0 (Legacy - PU-7 to PU-20): WFCK stays HIGH (Static GATE).
 *      LED: MAGENTA (Solid) - Indicates successful legacy board detection.
 * 
 *    - Mode 1 (Modern - PU-22 or newer): WFCK oscillates (Frequency-based).
 *      LED: CYAN (Solid) - Indicates successful modern board detection.
 * 
 *    - Mode 2 (Error/Ground): WFCK is stuck LOW (Short circuit or wiring issue).
 *      LED: BLUE (Blinks 3 times, then solid) - Indicates a critical signal error.
 * 
 * 
 * WFCK: __-----------------------  // CONTINUOUS (PU-7 .. PU-20)(GATE)
 *
 * WFCK: __-_-_-_-_-_-_-_-_-_-_-_-  // FREQUENCY  (PU-22 or newer)
 *
 * 
 * HISTORICAL CONTEXT:
 *    Traditionally, WFCK was referred to as the "GATE" signal. On early models, 
 *    modchips functioned as a synchronized gate, pulling the signal LOW 
 *    precisely when the region-lock data was being processed.
 * 
 * FREQUENCY DATA:
 *    - Initial/Protection Phase: ~7.3 kHz.
 *    - Standard Data Reading: ~14.6 kHz.
 *
 *******************************************************************************************************************/

void BoardDetection(void) {
    
    wfck_mode = 0;                    // Default: Mode 0 (Legacy/GATE)
    uint8_t pulse_hits = 25;          // Required oscillations to confirm Mode 1 (Frequency)
    uint32_t detectionWindow = 1000000; 

    // Wait for the  processor to stabilize if not in BIOS patch mode
    #if !defined(BIOS_PATCH)
        printf("Sleep 300ms...\n");
        sleep_ms(300);          
    #endif        

    // Main detection loop
    while (detectionWindow > 0) {
        detectionWindow--; 

        // Monitor for LOW transitions (only occurs in Frequency mode or Error state)
        if (!gpio_get(PIN_WFCK)) { 
            pulse_hits--;        

            if (pulse_hits == 0) {
                wfck_mode = 1;        // Confirmed: Mode 1 (Frequency)
                break;         
            }

            // Sync: Wait for signal to return HIGH to avoid double counting
            // decrement detectionWindow to prevent infinite hangs if signal is stuck LOW
            while (!gpio_get(PIN_WFCK) && detectionWindow > 0) {
                detectionWindow--;
            }
        }
    }
    
    // --- Final Diagnosis ---
    // If the window expired and we saw little to no activity while signal is LOW
    if (detectionWindow == 0 && pulse_hits > 15) {
        if (!gpio_get(PIN_WFCK)) {
            wfck_mode = 2;              // Confirmed: Mode 2 (Error - Stuck at Ground)
            
            // Visual Error feedback: Blinking Blue (per user preference)
            for(int i=0; i<3; i++) {
                SetLEDDynamic(LED_BLUE, 100);
                sleep_ms(100);
                SetLEDDynamic(0, 0); 
                sleep_ms(100);
            }
            SetLEDDynamic(LED_BLUE, 100); // Laisse allumé en rouge à la fin
        }
    }

    // // Set LED to Magenta if we remain in Mode 0 (Legacy/High)
    if (wfck_mode == 0) {
        SetLEDDynamic(LED_MAGENTA, 100);
    }

    //gpio_disable_pulls(PIN_WFCK);

    #if defined(DEBUG_SERIAL_MONITOR)
        BoardDetectionLog(detectionWindow, wfck_mode, INJECT_SCEx);
    #endif
}


/******************************************************************************************************************
 * FUNCTION    : CaptureSUBQ
 * 
 * DESCRIPTION : 
 *    Captures a complete 12-byte (96 bits) SUBQ frame.
 *    Data is shifted in from the PIO State Machine, which synchronizes 
 *    with the SQCK (SUBQ Clock) signal.
 * 
 * IMPLEMENTATION: 
 *    Uses 32-bit words for efficiency. Synchronous Serial, LSB first.
 ******************************************************************************************************************/
PIO pioSUBQ = pio0;         // Instance PIO (pio0 ou pio1)
uint smSUBQ = 0;            // Index de la State Machine (0 à 3)
uint offsetSUBQ;            // Offset du programme dans la mémoire PIO
void CaptureSUBQ(void) {
    uint32_t now = time_us_32(); 

    // --- STEP 1: TIMEOUT VALIDATION ---
    if (now - last_pulse_time > 1500 && last_pulse_time != 0) {
        last_pulse_time = 0;
        pio_sm_exec(pioSUBQ, smSUBQ, pio_encode_push(false, true)); 
        sleep_us(2);
        if (pio_sm_get_rx_fifo_level(pioSUBQ, smSUBQ) < 3) {
            pio_sm_clear_fifos(pioSUBQ, smSUBQ); 
            return; 
        }
    }

    // --- STEP 2: SIGNAL GLITCH FLUSH ---
    if (pio_glitch_detected) {
        pio_glitch_detected = false;
        last_pulse_time = 0;
        SUBQBuffer[0] = 0; SUBQBuffer[1] = 0; SUBQBuffer[2] = 0;
        pio_sm_restart(pioSUBQ, smSUBQ); 
        pio_sm_clear_fifos(pioSUBQ, smSUBQ);
        return;
    }

    // --- STEP 3: FIFO CHECK ---
    if (pio_sm_get_rx_fifo_level(pioSUBQ, smSUBQ) < 3) {
        return; 
    }

    last_pulse_time = now; 

    // AFFECTATION DIRECTE DEPUIS LE SILICIUM PIO
    SUBQBuffer[0] = pio_sm_get(pioSUBQ, smSUBQ);
    SUBQBuffer[1] = pio_sm_get(pioSUBQ, smSUBQ);
    SUBQBuffer[2] = pio_sm_get(pioSUBQ, smSUBQ);

    if (SUBQBuffer[0] == 0 && SUBQBuffer[1] == 0 && SUBQBuffer[2] == 0) {
        pio_sm_clear_fifos(pioSUBQ, smSUBQ);
        return;
    }

    // --- STEP 5: FILTRE ANTI-FREEZE NATUREL ---
    // CTRL/ADR est sur l'octet le plus bas de SUBQBuffer[0] (bits 0-7)
    uint8_t adr = SUBQBuffer[0] & 0x0F;
    // TRACK est sur l'octet juste au-dessus (bits 8-15)
    uint8_t track = (SUBQBuffer[0] >> 8) & 0xFF;
    
    bool crc_valid = (adr == 0x01) && (track < 100);

    // AJOUT IMPORTANT : On exécute le filtre d'abord pour qu'il calcule le Counter
    subq_new_frame_ready = true; // Donne le feu vert au filtre
    #if defined(DEBUG_SERIAL_MONITOR)
    CaptureSUBQLog(crc_valid); // Le log s'affiche ensuite
    #endif


    if (!crc_valid) {
        SUBQBuffer[0] = 0; SUBQBuffer[1] = 0; SUBQBuffer[2] = 0;
        pio_sm_clear_fifos(pioSUBQ, smSUBQ); 
        return; 
    }

}




/******************************************************************************************
 * FUNCTION    : FilterSUBQSamples
 * DESCRIPTION : Decodes, parses, and validates the incoming SQSO (Subcode Q Serial Output) 
 *               serial bitstream. This protocol carries the structural and temporal 
 *               geometry of the Compact Disc, outputting a complete 96-bit sector frame 
 *               every 6.67 milliseconds under native PlayStation 2x CD-ROM playback speed 
 *               (dropping to 13.3ms only during standard 1x Audio playback).
 * 
 *               THE RAW PHYSICAL SQSO FRAME STRUCTURE:
 *               __________________________________________________________________________
 *              |           |           |           |                              |           |
 *              | SYNC (2b) | CTRL (4b) | ADR (4b)  |      DATA PAYLOAD (72b)      | CRC (16b) |
 *              |___________|___________|___________|______________________________|___________|
 * 
 *               THE SQSO TIMING AND PACKET LAYOUT DECONSTRUCTION:
 *               The 96 bits are pushed MSB-First into the hardware registers and are 
 *               subdivided into four distinct logical sections based on the Red Book standard:
 * 
 *               1. TRACK CONTROL (CTRL) & ADDRESS (ADR) [Upper 8-Bits of current_word1]
 *                  - CTRL (4 bits): Defines track properties. Games utilize data tracks,
 *                    requiring Bit 2 to be set while Bit 3 (audio) is cleared (Value: 0x04).
 *                  - ADR (4 bits): Specifies payload mapping mode. Mode 1 (0x01) signals 
 *                    standard position, track indexing, and temporal metadata layout.
 * 
 *               2. TRACK NUMBER / TNO [Bits 16-23 of current_word1]
 *                  - TNO (8 bits): Encoded in Binary-Coded Decimal (BCD). In the physical 
 *                    outer track boundary known as the Lead-In zone, the CD decoder 
 *                    strictly forces TNO to 0x00. This acts as the filter's gatekeeper.
 * 
 *               3. TABLE OF CONTENTS INDEX / INDEX [Bits 8-15 of current_word1]
 *                  - INDEX (8 bits): Inside the Lead-In zone (TNO=0x00), this space changes 
 *                    its function to output critical TOC descriptors (A0, A1, A2). The 
 *                    filter utilizes a subtraction check to target these pointers.
 * 
 *               4. MINUTES / MIN [Bits 0-7 of current_word1]
 *                  - MIN (8 bits): Tracks relative minutes. On specific multimedia models 
 *                    like the SCPH-5903, Video CD (VCD) metadata layout formats inject 
 *                    item descriptors (0x02) into this location. The filter monitors 
 *                    and drops these patterns to block interference during movie streams.
 * 
 *               5. SECONDS / SEC [Bits 24-31 of current_word2]
 *                  - SEC (8 bits): Tracks relative seconds. At the exact intersection where 
 *                    the Lead-In spiral cross-fades into Track 01, an intentional integer 
 *                    underflow check isolates the early startup window (0.0s to 2.0s).
 * 
 *               6. HARDWARE GUARD BYTE / ZERO [Bits 8-15 of current_word2]
 *                  - ZERO (8 bits): Mandatory hardware synchronization spacer byte. Sony 
 *                    specifications demand this field reads 0x00 to prove zero bus drift.
 * 
 *               7. INTEGRATION HYSTERESIS MANAGEMENT
 *                  - Successfully authenticated patterns step up a capped 'request_counter' 
 *                    to open the injection window, while corrupted frames or out-of-bounds 
 *                    seeking steps it down to filter line glitches and motor noise.
 ******************************************************************************************/
void FilterSUBQSamples(void) {
    // BARRIÈRE : Empêche le traitement répétitif sur la même trame
    if (!subq_new_frame_ready) return;
    subq_new_frame_ready = false; 

    // SÉCURITÉ ABSOLUE : Si le tampon est nettoyé à 0 suite à une erreur,
    // on sort IMMÉDIATEMENT sans appliquer le decay pour ne pas fausser le compteur sur du bruit.
    if (SUBQBuffer[0] == 0 && SUBQBuffer[1] == 0) {
        return; 
    }

    // --- STEP 1: FILTRE DE SYNCHRONISATION & MATRICE DE HIT UNIQUE (Version Aplatie) ---
    if (
        // Condition A : On doit impérativement être dans la TOC (bits 8-15 == 00) 
        // ET le secteur doit être DATA (bits 0-7 & 0xD0 == 0x40). On fusionne ces deux masques en 0x0000FFD0 == 0x00000040.
        (((SUBQBuffer[0] & 0x0000FFD0) == 0x00000040) && (
            // INDEX >= A0 (bits 16-23)
            ((SUBQBuffer[0] & 0x00FF0000) >= 0x00A00000) || 
            // INDEX == 01 (bits 16-23) ET calcul wrap-around sur REL_MIN (bits 24-31)
            (((SUBQBuffer[0] & 0x00FF0000) == 0x00010000) && (((((SUBQBuffer[0] & 0xFF000000) - 0x03000000) & 0xFF000000) >= 0xF5000000)))
        ))
        || 
        // Condition B : Maintien du Tracking Lock à l'intérieur de la TOC (Si le compteur a démarré)
        // Pour éviter que le compteur ne continue à monter pendant le jeu, on force aussi la barrière TRACK == 00 ici
        (request_counter > 0 && ((SUBQBuffer[0] & 0x0000FF00) == 0x00000000) && (
            ((SUBQBuffer[0] & 0x000000FF) == 0x00000001) || // Secteur Audio 01
            ((SUBQBuffer[0] & 0x000000D0) == 0x00000040)    // Secteur Data
        ))
    ) 
    {
        request_counter++; 
        return;
    }


    // --- STEP 2: SIGNAL DECAY (Décrémentation directe) ---
    // Si on arrive ici, c'est que la trame est valide (CRC OK) mais qu'elle provient 
    // de la zone de jeu (Track 01 active). Le compteur va descendre jusqu'à 0 pour couper l'injection.
    if (request_counter > 0) {
        request_counter--; 
    }
}





/*********************************************************************************************
 * FUNCTION    : PerformInjectionSequence
 * 
 * DESCRIPTION :
 *    Executes the SCEx injection sequence to bypass the CD-ROM regional lockout.
 *    Supports two hardware-specific injection methods:
 * 
 *    1. Legacy Gate Mode (PU-7 to PU-20): Modchip acts as a logic gate to pull 
 *       the signal down. This mode relies on internal chip timing.
 *
 *    2. WFCK SYNC MODE (PU-22 and later): 
 *       Synchronizes data injection with the console's hardware WFCK clock.
 *       The signal is modulated on every clock edge to ensure perfect 
 *       alignment with the CD-ROM controller's internal sampling.
 * 
 * NOTE: WFCK frequency is approx. 7.3 kHz during region check, but doubles 
 *       to 14.6 kHz during data reading. The PIO modulation loop handles 
 *       both speeds as it syncs directly to the physical WFCK edges.
 *********************************************************************************************/

void PerformInjectionSequence(uint8_t injectSCEx) {
    // SCEx codes converted to 64-bit (44 effective bits used)
    static const uint64_t scex_patterns[] = {
        0x000002EAD5B24D99ULL, // SCEI (NTSC-J)
        0x000002FAD5B24D99ULL, // SCEA (NTSC-U/C)
        0x000002DAD5B24D99ULL  // SCEE (PAL)
    };

    // Note: Legacy Gate Mode (wfck_mode 0) is typically handled by a secondary 
    // PIO program or a dedicated delay loop. This function focuses on 
    // WFCK-based modulation (PU-22+).
    if (!wfck_mode) {
        // Implementation for Legacy/Gate mode can be added here if needed
    }

    // Region cycle loop (handles Universal mode if injectSCEx == 3)
    for (uint8_t regionCycle = 0; regionCycle < 3; regionCycle++) {
        uint8_t regionIndex = (injectSCEx == 3) ? regionCycle : injectSCEx;
        uint64_t pattern = scex_patterns[regionIndex];

        // Send 44 bits to the PIO FIFO
        // The PIO consumes bits one-by-one and handles the 30 WFCK pulses per bit
        for (int i = 0; i < 44; i++) {
            uint32_t bit = (pattern >> i) & 0x01;
            pio_sm_put_blocking(pioSUBQ, smSUBQ, bit);
        }

        // If not in Universal mode, exit after the first successful pattern
        if (injectSCEx != 3) break;
        
        // Inter-region delay for Universal mode
        sleep_ms(90); 
    }

    // Release the data bus
    gpio_disable_pulls(PIN_DATA); 

    #ifdef LED_RUN
        PIN_LED_OFF;
    #endif
}



/**
 * System Initialization
 * Sets up GPIOs, PIO state machines, and initial LED status.
 */
void Init() {
    // 1. Initialize RGB LED
    NeoPixel_Init();
    
    // 2. GPIO Initialization (Reset to High-Z/Input state for SIO controller)
    gpio_init(PIN_DATA);
    gpio_init(PIN_WFCK);
    gpio_init(PIN_SQCK);
    gpio_init(PIN_SUBQ);

    // Disable internal pulls to avoid interference with the console's logic levels
    gpio_disable_pulls(PIN_DATA);
    gpio_disable_pulls(PIN_WFCK);
    gpio_disable_pulls(PIN_SQCK);
    gpio_disable_pulls(PIN_SUBQ);

    // Set directions
    gpio_set_dir(PIN_WFCK, GPIO_IN);
    gpio_set_dir(PIN_SQCK, GPIO_IN);
    gpio_set_dir(PIN_SUBQ, GPIO_IN);
    
    #ifdef BIOS_PATCH
        // Hardware Patching Pins setup
        gpio_init(PIN_AX);
        gpio_init(PIN_AY);
        gpio_init(PIN_DX);
        gpio_disable_pulls(PIN_AX);
        gpio_disable_pulls(PIN_AY);
        gpio_disable_pulls(PIN_DX);

        // Load and initialize BIOS Patch PIO program
        uint offsetPATCH = pio_add_program(pio0, &bios_patch_program);
        smPATCH = pio_claim_unused_sm(pio0, true);
        // bios_patch_pio_init would be called here if you have a custom init helper
        #ifdef PATCH_SWITCHE
            gpio_init(PIN_SWITCH);
            gpio_set_dir(PIN_SWITCH, GPIO_IN);
            gpio_pull_up(PIN_SWITCH);
            
        #endif
    #endif

    // 3. PIO Program Loading for SUBQ Capture
   offsetSUBQ = pio_add_program(pioSUBQ, &subq_capture_program);
    
    // Initialize SUBQ state machine with explicit global variables
    
    subq_capture_program_init(pioSUBQ, smSUBQ, offsetSUBQ, PIN_SUBQ, PIN_SQCK);
    
    // Initial visual feedback: Dim White (System Ready)
    SetLEDDynamic(LED_WHITE, 50);

    stdio_init_all();
    uart_init(uart0, 115200);
    gpio_set_function(0, GPIO_FUNC_UART);
}


int main() {
    stdio_init_all();
    //init_debug();
    Init();

    // --- Critical Boot Patching ---
    #ifdef BIOS_PATCH
        // Execute BIOS patching sequence
        Bios_Patching();
    #endif

    // Identify board revision (PU-7 to PU-22+) to set correct injection timings
    BoardDetection();

    // #if defined(DEBUG_SERIAL_MONITOR)
    //     // Display initial board detection results (Window remaining & WFCK mode)
    //     BoardDetectionLog(global_window, wfck_mode, INJECT_SCEx);
    // #endif
    pio_sm_set_enabled(pioSUBQ, smSUBQ, false);  // Coupe le PIO
    pio_sm_clear_fifos(pioSUBQ, smSUBQ);          // Supprime les octets parasites du boot
    pio_sm_restart(pioSUBQ, smSUBQ);              // Reset les compteurs de bits internes
    pio_sm_set_enabled(pioSUBQ, smSUBQ, true);   // Relance le PIO propre pour le direct
    
    while (true) {
        // Timing Sync: Prevent reading the tail end of the previous SUBQ packet
        //sleep_ms(1);        

        // DATA ACQUISITION: Capture the 12-byte SUBQ stream
        CaptureSUBQ();       

        // FILTERING: Analyze buffer and update request_counter
        FilterSUBQSamples();

        /** 
         * VISUAL FEEDBACK LOGIC
         * Transitions from dim Yellow (Searching) to brightening Green (Locking).
         * Fires a Blue flash during SCEx injection.
         */
        if (request_counter == 0) {
            // Idle/Searching state: Very dim Yellow
            SetLEDDynamic(LED_YELLOW, 30); 
        } 
        else if (request_counter < REQUEST_INJECT_TRIGGER) {
            // Stable reading: Green intensity scales with the counter (Loading effect)
            SetLEDDynamic(LED_GREEN, request_counter * 7); 
        } 
        else { // request_counter >= REQUEST_INJECT_TRIGGER
            // Injection state: Solid Blue flash
            SetLED(LED_BLUE); 
            
            // Execute the SCEx injection burst
            PerformInjectionSequence(INJECT_SCEx);      

            // STEALTH GAP: Mimics natural CD behavior after successful injection
            request_counter = (REQUEST_INJECT_TRIGGER - REQUEST_INJECT_GAP);
            SetLEDDynamic(LED_GREEN, request_counter * 7);
        }
    }
    return 0;


}





