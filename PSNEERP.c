//                           P.S.N.E.E.R.P. 0.1

// ==========================================
// LE COMMUTATEUR CENTRAL (1: Avant compilation / 0: Par les Pins)
// ==========================================
#define CONFIG_MODE_STATIC 0 

#if CONFIG_MODE_STATIC
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
// #define SCPH_xxx2  //  PAL         | Europ.
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

//#define REQUEST_INJECT_TRIGGER 20 // Now coupled with REQUEST_INJECT_GAP; allows for higher trigger
/*
 * TRIGGER CALIBRATION:
 * - Lower values (<5): Possible, but not beneficial.
 * - The value of 11 for REQUEST_INJECT_TRIGGER must not be exceeded on Japanese models.
 *   This causes problems with disks that have anti-mod protection; it's less noticeable on other models.
 * - Higher values (15-20-25-30): Possible for older or weak CD-ROM laser units.
 */
#endif


 #define REQUEST_INJECT_GAP 6      // Stealth interval (must be 4-8 AND < REQUEST_INJECT_TRIGGER)
/*
 * NOTE: REQUEST_INJECT_GAP defines the "cool-off" period between injections.
 * - Optimal range: 4 to 8 (for natural CD timing & anti-mod bypass).
 * - Constraint: Must ALWAYS be lower than REQUEST_INJECT_TRIGGER.
 */

// #define PATCH_SWITCHE    // This allows the user to disable the BIOS patch on-the-fly.
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
#include <stdint.h>
#include "settings.h"
#include "psneerp.pio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h" 
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h" 


uint offsetPATCH;  



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


// INSTANCES MATÉRIELLES RP2040
PIO pio = pio0;
uint sm = 0;

//--- bois patch variables ---
static bool monitoring_active = false;
static uint64_t global_window_end_us = 0; 
static uint64_t inactivity_start_us = 0;  
static int last_known_state_D2 = 0;

// --- Variables globales de mode post-compilation ---
volatile uint8_t BIOS_PATCH_MODE = 0; // 0=Désactivé, 1=Patch standard, 2=SCPH-5903, 3=Réserve
volatile bool scph_5903_active = false; // Flag spécifique pour la logique Video-CD


// --- CONFIGURATION DU SIGNAL HORLOGE ---
volatile uint8_t wfck_mode = 0; // 0 = Standard / Détection automatique

// Variables de contrôle d'injection d'origine
volatile uint32_t request_counter = 0;
extern volatile bool scph_5903_active;

#if !CONFIG_MODE_STATIC
volatile uint32_t INJECT_SCEx = 3;
volatile uint32_t REQUEST_INJECT_TRIGGER = 20;
#endif


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

#define PIO_LED  pio1
#define SM_LED   0  // Fixé statiquement : évite pio_claim_unused_sm (YAGNI)

void NeoPixel_Init(void) {
    // Chargement et initialisation directe sur pio1, state machine 0
    uint offset = pio_add_program(PIO_LED, &ws2812_program);
    ws2812_program_init(PIO_LED, SM_LED, offset, LED_PIN, 800000.0f, false);
}

/**
 * Envoie la couleur directement au PIO.
 * Format attendu : 0xGGRRBB00. L'intensité doit être pré-calculée dans vos constantes.
 */


void SetLEDDynamic(uint32_t color, uint8_t intensity) {
    // Calcul direct et exact des proportions
    uint32_t g = (((color >> 24) & 0xFF) * intensity) >> 8;
    uint32_t r = (((color >> 16) & 0xFF) * intensity) >> 8;
    uint32_t b = (((color >> 8)  & 0xFF) * intensity) >> 8;

    uint32_t final_color = (g << 24) | (r << 16) | (b << 8);
    pio_sm_put_blocking(PIO_LED, SM_LED, final_color);
}



#define LED_OFF     0x00000000
#define LED_RED     0x00FF0000
#define LED_GREEN   0xFF000000
#define LED_BLUE    0x0000FF00
#define LED_MAGENTA 0x00FFFF00
#define LED_CYAN    0xFF00FF00
#define LED_YELLOW  0xFFFF0000
#define LED_WHITE   0xFFFFFF00
#define LED_ORANGE  0x80FF0000
#define LED_PURPLE  0x0080FF00


/****************************************************************************************
 * FUNCTION    : Bios_Patching()
 *

 **************************************************************************************/



void BIOS_Patch(void) {

    if (!monitoring_active) return;
    
    uint64_t now = to_us_since_boot(get_absolute_time());

    // Étape 1 : Fin de la fenêtre globale
    if (now >= global_window_end_us) {
        monitoring_active = false;
        return;
    }
    
    SetLEDDynamic(LED_WHITE, 200);

    // ============================================================================
    // Étape 2 : ÉCHANTILLONNAGE ULTRA-RAPIDE ANTI-STROBOSCOPIQUE (CONTOURNE LE PWM 99%)
    // ============================================================================
    bool activite_detectee = false;
    int state_initial = gpio_get(PIN_D2);
    
    // On observe la broche de manière intensive pendant 40 microsecondes.
    // Si le PWM de 30 µs passe pendant ce laps de temps, on le capture à coup sûr.
    uint64_t start_sample = to_us_since_boot(get_absolute_time());
    while ((to_us_since_boot(get_absolute_time()) - start_sample) < 40) {
        if (gpio_get(PIN_D2) != state_initial) {
            activite_detectee = true;
            break; 
        }
        tight_loop_contents();
    }

    // Gestion du chronomètre basée sur l'observation intensive
    if (activite_detectee) {
        last_known_state_D2 = gpio_get(PIN_D2);
        inactivity_start_us = now; // Reset du chrono : il y a de l'activité réelle !
    } 
    else {
        // Seuil à 550ms validé si le signal est resté totalement plat
        if ((now - inactivity_start_us) >= 550000) {
            printf("a-----\n");
            monitoring_active = false;

            // ------------------------------------------------------------------------
            // Étape 3 : Attente bloquante HAUTE PRIORITÉ du prochain front sur PIN_CE
            // ------------------------------------------------------------------------
            int wait_state = gpio_get(PIN_CE); 
            uint64_t wait_start = to_us_since_boot(get_absolute_time());
            bool change_detected = false;
            
            while (true) {
                if (gpio_get(PIN_CE) != wait_state) { 
                    change_detected = true;
                    break; // Synchronisation immédiate et chirurgicale au premier front
                }

                // Timeout de sécurité si CE reste muet
                if ((to_us_since_boot(get_absolute_time()) - wait_start) >= 500000) {
                    break; 
                }

                tight_loop_contents(); 
            }

            // ------------------------------------------------------------------------
            // Étape 4 : Action bloquante HAUTE PRIORITÉ -> Impulsion de 0V sur PIN_D2
            // ------------------------------------------------------------------------
            if (change_detected) {
                gpio_put(PIN_D2, 0);   
                gpio_set_dir(PIN_D2, GPIO_OUT); // Court-circuit franc à la masse (0 Ω)
                
                busy_wait_us(900); // Maintien strict de vos 900 µs à l'oscilloscope
                
                gpio_set_dir(PIN_D2, GPIO_IN); // Libération immédiate (Haute impédance)
                gpio_disable_pulls(PIN_D2);

                printf("b-----\n");
            }
        }
    }
}




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

__attribute__((optimize("O3"))) void BoardDetection(void) {
    wfck_mode = 0;                    // Default: Mode 0 (Legacy/GATE)
    uint8_t pulse_hits = 25;          // Required oscillations to confirm Mode 1 (Frequency)
    uint32_t detectionWindow = 250000; 

    #if !defined(BIOS_PATCH)
        //printf("Sleep 300ms...\n");
        sleep_ms(300);          
    #endif 
    
    SetLEDDynamic(LED_BLUE, 100); 

    // Main detection loop
    while (detectionWindow > 0) {
        detectionWindow--; 

        if (!gpio_get(PIN_WFCK)) { 
            pulse_hits--;        

            if (pulse_hits == 0) {
                wfck_mode = 1;        // Confirmed: Mode 1 (Frequency)
                SetLEDDynamic(LED_MAGENTA, 100);
                break;         
            }

            while (!gpio_get(PIN_WFCK) && detectionWindow > 0) {
                detectionWindow--;
            }
        }
    }
    
    //if (wfck_mode == 0) {
    //    gpio_put(PIN_WFCK, 1);
    //}

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
__attribute__((optimize("O3"))) void CaptureSUBQ(void) {
    uint32_t now = time_us_32(); 

    // --- STEP 1: TIMEOUT VALIDATION ---
    if (last_pulse_time != 0 && (now - last_pulse_time > 1500)) {
        last_pulse_time = 0;
        pio_sm_exec(pioSUBQ, smSUBQ, pio_encode_push(false, true)); 
        busy_wait_us_32(2); // OPTIMISATION 1 : Utilisation du timer hardware natif (pas de scheduler de sleep)
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

    // OPTIMISATION 2 : Règle 4 appliquée sur un masque binaire léger (évite 3 comparaisons lourdes)
    if ((SUBQBuffer[0] | SUBQBuffer[1] | SUBQBuffer[2]) == 0) {
        pio_sm_clear_fifos(pioSUBQ, smSUBQ);
        return;
    }

    // --- STEP 5: FILTRE ANTI-FREEZE NATUREL ---
    uint8_t adr = SUBQBuffer[0] & 0x0F;
    uint8_t track = (SUBQBuffer[0] >> 8) & 0xFF;
    
    bool crc_valid = (adr == 0x01) && (track < 100);

    subq_new_frame_ready = true; 
    
    #if defined(DEBUG_SERIAL_MONITOR)
    CaptureSUBQLog(crc_valid); 
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
__attribute__((optimize("O3"))) void FilterSUBQSamples(void) {
    // BARRIÈRE : Empêche le traitement répétitif sur la même trame
    if (!subq_new_frame_ready) return;
    subq_new_frame_ready = false; 

    // OPTIMISATION RAM/CPU : Copie immédiate dans les registres internes 32 bits du Cortex-M0+
    uint32_t reg0 = SUBQBuffer[0];
    uint32_t reg1 = SUBQBuffer[1];

    // SÉCURITÉ ABSOLUE : Test ultra-rapide sur les registres
    if ((reg0 | reg1) == 0) {
        return; 
    }

    // --- STEP 0: Data/TOC Validation ---
    uint32_t isDataSector = ((reg0 & 0x000000D0) == 0x00000040);

    // --- STEP 1: SUBQ Frame Synchronization ---
    if (((reg0 & 0x0000FF00) == 0) && ((reg1 & 0x00FF0000) == 0)) {

        // Condition A : Mode DATA + Analyse de la zone TOC
        if (isDataSector) {
            // INDEX >= A0
            if ((reg0 & 0x00FF0000) >= 0x00A00000) {
                
                // SIGNALEMENT : SPÉCIFICITÉ SCPH-5903 GÉRÉE DYNAMIQUEMENT
                // Justification : Remplace le #ifdef pour s'adapter à la volée selon les pins du PCB
                if (scph_5903_active && ((reg0 & 0xFF000000) == 0x02000000)) {
                    // C'est un flux VCD détecté sur une machine VCD.
                    // On ne fait rien ici pour sauter l'incrémentation et laisser le STEP 2 diminuer le compteur (Decay).
                } else {
                    // Modèles standards OU disque de jeu standard sur 5903
                    request_counter++;
                    return;
                }
            }
            
            // INDEX == 01 ET calcul wrap-around d'origine strict sur 32 bits
            if (((reg0 & 0x00FF0000) == 0x00010000) && 
                (((((reg0 & 0xFF000000) - 0x03000000) & 0xFF000000) >= 0xF5000000))) {
                request_counter++;
                return;
            }
        }

        // Condition B : Maintien du Tracking Lock
        if (request_counter > 0) {
            if ((reg0 & 0x000000FF) == 0x00000001 || isDataSector) {
                request_counter++;
                return;
            }
        }
    }

    // --- STEP 2: Signal Decay ---
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
  // OPTIMISATION : Tableau aligné sur la largeur native des registres ARM 32-bit
  static const uint32_t allRegionsSCEx[3][2] = {
      { 0x5D4BC959, 0x000002DA }, // SCEI (Jap)
      { 0x5D4BC959, 0x000002FA }, // SCEA (USA)
      { 0x5D4BC959, 0x000002EA }  // SCEE (PAL)
  };

  const uint32_t BIT_DELAY = 4000;

  for (uint32_t regionCycle = 0; regionCycle < 3; regionCycle++) {
    uint32_t regionIndex = (injectSCEx == 3) ? regionCycle : (uint32_t)injectSCEx;

    // OPTIMISATION CHIRURGICALE : On extrait les deux mots de la région dans des registres CPU locaux
    uint32_t w0 = allRegionsSCEx[regionIndex][0];
    uint32_t w1 = allRegionsSCEx[regionIndex][1];

    for (uint32_t bitPosition = 0; bitPosition < 44; bitPosition++) {
      // Extraction ultra-rapide sans aucune division, reste 100% synchrone
      uint32_t currentBit = (bitPosition < 32) ? ((w0 >> bitPosition) & 0x01) 
                                               : ((w1 >> (bitPosition - 32)) & 0x01);

      if (wfck_mode) {
        /* METHOD 1: PULSE COUNTING (WFCK SYNC) */
        for (uint32_t count = 30; count > 0; count--) {
          while (gpio_get(PIN_WFCK));
          gpio_put(PIN_DATA, 0); 

          while (!gpio_get(PIN_WFCK));
          if (currentBit) {
            gpio_put(PIN_DATA, 1); 
          }
        }
      } 
      else {
        /* METHOD 2: TIME REFERENCE (FIXED DELAY) */
        gpio_set_dir(PIN_WFCK, GPIO_OUT);
        gpio_put(PIN_WFCK, 1);
        if (currentBit == 0) {
          gpio_put(PIN_DATA, 0);
        } else {
          gpio_put(PIN_DATA, 1);
        }
        busy_wait_us_32(BIT_DELAY); 
      }
    }

    if (injectSCEx == 3) {
        sleep_ms(90);
        gpio_put(PIN_DATA, 0);
    }

    if (injectSCEx != 3) {
        gpio_set_dir(PIN_WFCK, GPIO_IN);
        gpio_disable_pulls(PIN_WFCK);

        // Stabilisation électrique de la ligne après forçage
        busy_wait_us_32(20); 

        if (BIOS_PATCH_MODE == 1) {
            uint64_t now = to_us_since_boot(get_absolute_time());
            global_window_end_us = now + 2800000; 
            inactivity_start_us = now;
            last_known_state_D2 = gpio_get(PIN_D2);
            
            monitoring_active = true;
        }
        break; 
    }

    if (regionCycle == 2) {
        gpio_set_dir(PIN_DATA, GPIO_IN);
        gpio_disable_pulls(PIN_DATA);
        gpio_set_dir(PIN_WFCK, GPIO_IN);
        gpio_disable_pulls(PIN_WFCK);

        // Stabilisation électrique de la ligne après forçage
        busy_wait_us_32(20); 

        if (BIOS_PATCH_MODE == 1) {
            uint64_t now = to_us_since_boot(get_absolute_time());
            global_window_end_us = now + 2800000; 
            inactivity_start_us = now;
            last_known_state_D2 = gpio_get(PIN_D2);
            
            monitoring_active = true;
        }
        break; 
    }
  }

  #if defined(DEBUG_SERIAL_MONITOR)
    InjectLog();
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
    gpio_init(PIN_CE);
    gpio_init(PIN_D2);
    gpio_init(PIN_BIOS_A);
    gpio_init(PIN_BIOS_B);
    gpio_init(PIN_TRIGGER_A);
    gpio_init(PIN_TRIGGER_B);
    gpio_init(PIN_REGION_A);
    gpio_init(PIN_REGION_B);

    // Disable internal pulls to avoid interference with the console's logic levels
    gpio_disable_pulls(PIN_DATA);
    gpio_disable_pulls(PIN_WFCK);
    gpio_disable_pulls(PIN_SQCK);
    gpio_disable_pulls(PIN_SUBQ);
    gpio_disable_pulls(PIN_CE);
    gpio_disable_pulls(PIN_D2);

    gpio_pull_down(PIN_BIOS_A);
    gpio_pull_down(PIN_BIOS_B);
    gpio_pull_down(PIN_TRIGGER_A);
    gpio_pull_down(PIN_TRIGGER_B);
    gpio_pull_down(PIN_REGION_A);
    gpio_pull_down(PIN_REGION_B);

    // Set directions
    gpio_set_dir(PIN_WFCK, GPIO_IN);
    gpio_set_dir(PIN_SQCK, GPIO_IN);
    gpio_set_dir(PIN_SUBQ, GPIO_IN);
    gpio_set_dir(PIN_CE, GPIO_IN);
    gpio_set_dir(PIN_D2, GPIO_IN);
    gpio_set_dir(PIN_BIOS_A, GPIO_IN);
    gpio_set_dir(PIN_BIOS_B, GPIO_IN);
    gpio_set_dir(PIN_TRIGGER_A, GPIO_IN);
    gpio_set_dir(PIN_TRIGGER_B, GPIO_IN);
    gpio_set_dir(PIN_REGION_A, GPIO_IN);
    gpio_set_dir(PIN_REGION_B, GPIO_IN);
    


    // 3. PIO Program Loading for SUBQ Capture
   offsetSUBQ = pio_add_program(pioSUBQ, &subq_capture_program);
    
    // Initialize SUBQ state machine with explicit global variables
    
    subq_capture_program_init(pioSUBQ, smSUBQ, offsetSUBQ, PIN_SUBQ, PIN_SQCK);
    
    // Initial visual feedback: Dim White (System Ready)
    SetLEDDynamic(LED_WHITE, 50);

    stdio_init_all();
    uart_init(uart0, 115200);
    gpio_set_function(0, GPIO_FUNC_UART);

    // =========================================================================
    // DÉTECTION DYNAMIQUE DE LA RÉGION (Si CONFIG_MODE_STATIC vaut 0)
    // =========================================================================
    #if !CONFIG_MODE_STATIC
    // 1. Lecture des états matériels (0 ou 1) via le SDK Pico
    uint32_t REGION_val_B = gpio_get(PIN_REGION_B);
    uint32_t REGION_val_A = gpio_get(PIN_REGION_A);
    
    // 2. Alignement binaire (B = bit 1, A = bit 0)
    uint32_t REGION_index_BA = (REGION_val_B << 1) | REGION_val_A;
    
    // 3. Table de correspondance statique (LUT) 
    // index_BA: 00->3 (Multi), 01->0 (Jap), 10->1 (USA), 11->2 (PAL)
    static const uint8_t REGION_LUT_INJECT[] = {3, 0, 1, 2};
    
    // 4. Assignation directe à votre variable globale volatile
    INJECT_SCEx = REGION_LUT_INJECT[REGION_index_BA];

    // 1. Lecture des états matériels (0 ou 1) via le SDK Pico
    uint32_t TRIGGER_val_B = gpio_get(PIN_TRIGGER_B);
    uint32_t TRIGGER_val_A = gpio_get(PIN_TRIGGER_A);
    
    // 2. Alignement binaire (B = bit 1, A = bit 0)
    uint32_t TRIGGER_index_BA = (TRIGGER_val_B << 1) | TRIGGER_val_A;
    
    // 3. Table de correspondance statique (LUT) 
    
    static const uint8_t TRIGGER_LUT_INJECT[] = {10, 15, 20, 25};
    
    // 4. Assignation directe à votre variable globale volatile
    REQUEST_INJECT_TRIGGER = TRIGGER_LUT_INJECT[TRIGGER_index_BA];

    // 5. Affichage simple de la variable
    printf("REQUEST_INJECT_TRIGGER: %d\n", REQUEST_INJECT_TRIGGER);

    // =========================================================================
    // CONFIGURATION DYNAMIQUE DU MODE BIOS (Post-compilation)
    // =========================================================================
    // 1. Lecture des états matériels des broches BIOS
    uint32_t BIOS_val_B = gpio_get(PIN_BIOS_B);
    uint32_t BIOS_val_A = gpio_get(PIN_BIOS_A);

    // 2. Alignement binaire (B = bit 1, A = bit 0)
    uint32_t BIOS_index_BA = (BIOS_val_B << 1) | BIOS_val_A;

    // 3. Assignation directe à la variable globale (00=0, 01=1, 10=2, 11=3)
    BIOS_PATCH_MODE = BIOS_index_BA;

    // 4. Traitement des cas spécifiques (YAGNI : Logique immédiate)
    if (BIOS_PATCH_MODE == 2) {
        scph_5903_active = true;
        // Optionnel : Vous pouvez ajuster ici d'autres timings spécifiques à la SCPH-5903 si nécessaire
    } else {
        scph_5903_active = false;
    }

    printf("BIOS_PATCH_MODE configuré sur : %d (SCPH-5903 active: %s)\n", 
           BIOS_PATCH_MODE, scph_5903_active ? "OUI" : "NON");
    
    #endif
}


int main() {
    stdio_init_all();

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

        gpio_set_dir(PIN_DATA, GPIO_OUT);  
        gpio_put(PIN_DATA, 0);   
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
            SetLEDDynamic(LED_BLUE, 120); 
            
            // Execute the SCEx injection burst
            PerformInjectionSequence(INJECT_SCEx);      

            // STEALTH GAP: Mimics natural CD behavior after successful injection
            request_counter = (REQUEST_INJECT_TRIGGER - REQUEST_INJECT_GAP);
            SetLEDDynamic(LED_GREEN, request_counter * 7);
        }

        if (BIOS_PATCH_MODE == 1){

         BIOS_Patch();
        }
    }
    return 0;


}





