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
 #define SCPH_102             // DX - D0  | AX - A7          |                  | 4.4e - CRC 0BAD7EA9, 4.5e -CRC 76B880E5
// #define SCPH_100             // DX - D0  | AX - A7          |                  | 4.3j - CRC F2AF798B
// #define SCPH_7000_7500_9000  // DX - D0  | AX - A7          |                  | 4.0j - CRC EC541CD0
// #define SCPH_3500_5000_5500  // DX - D0  | AX - A16         | AX - A15         | 3.0j - CRC FF3EEB8C, 2.2j - CRC 24FC7E17, 2.1j - CRC BC190209 
// #define SCPH_3000            // DX - D5  | AX - A7, AY - A8 | AX - A6, AY - A7 | 1.1j - CRC 3539DEF6 
// #define SCPH_1000            // DX - D5  | AX - A7, AY - A8 | AX - A6, AY - A7 | 1.0j - CRC 3B601FC8

/*******************************************************************************************************************
 *                           Options
 *******************************************************************************************************************/

//#define REQUEST_INJECT_TRIGGER 10 // Now coupled with REQUEST_INJECT_GAP; allows for higher trigger
/*
 * TRIGGER CALIBRATION:
 * - Lower values (<5): Possible, but not beneficial.
 * - The value of 11 for REQUEST_INJECT_TRIGGER must not be exceeded on Japanese models.
 *   This causes problems with disks that have anti-mod protection; it's less noticeable on other models.
 * - Higher values (15-20-25-30): Possible for older or weak CD-ROM laser units.
 */

//#define REQUEST_INJECT_GAP 5      // Stealth interval (must be 4-8 AND < REQUEST_INJECT_TRIGGER)
/*
 * NOTE: REQUEST_INJECT_GAP defines the "cool-off" period between injections.
 * - Optimal range: 4 to 8 (for natural CD timing & anti-mod bypass).
 * - Constraint: Must ALWAYS be lower than REQUEST_INJECT_TRIGGER.
 */

//#define PATCH_SWITCHE    // This allows the user to disable the BIOS patch on-the-fly.
/*
 * This allows you to bypass the memory card blocking problems on the SCPH-7000.
 * - Configure Pin D5 as Input.
 * - Enable internal Pull-up.
 * - Exit immediately the patch BIOS if the switch pulls the pin to GND
 */

// #define DEBUG_SERIAL_MONITOR  // Enables serial monitor output. 
/*
 *  Requires compilation with Arduino libs!
 *  For Arduino connect TXD and GND, for ATtiny PB3 (pin 2) and GND, to your serial card RXD and GND.
 *
 *   For Arduino (Uno/Nano/Pro Mini):
 *   TX (Pin 1)  ----->  RX (Serial Card)
 *   GND         ----->  GND
 *
 *   For ATtiny (25/45/85):
 *   Pin 2 (PB3) ----->  RX (Serial Card)
 *   Pin 4 (GND) ----->  GND
 *
 */

/******************************************************************************************************************
 *           Summary of practical information. Fuses. Pinout
 *******************************************************************************************************************
 * Fuses  
 * MCU        | High | Low | Extended
 * --------------------------------------------------
 * ATmega32U4 | DF   | EE  | D7 
 * ATtiny     | DF   | E2  | FF
 *
 * Pinout
 * Arduino   | PSNee     |
 * ---------------------------------------------------
 * VCC       | VCC       |
 * GND       | GND       |
 * RST       | RESET     | Only for JAP_FAT
 * D2        | BIOS AX   | Only for Bios patch
 * D3        | BIOS AY   | Only for BIOS patch SCPH_1000, SCPH_3000
 * D4        | BIOS DX   | Only for Bios patch
 * D5        | SWITCH    | Optional for disabling Bios patch
 * D6        | SQCK      |
 * D7        | SUBQ      |
 * D8        | DATA      |
 * D9        | WFCK      |
 * D13 ^ D10 | LED       | D10 only for ATmega32U4
 *
 * ATtiny | PSNee        | ISP   |
 * ---------------------------------------------------
 * Pin1   |              | RESET |
 * Pin2   | LED ^ serial |       | serial only for DEBUG_SERIAL_MONITOR
 * Pin3   | WFCK         |       |
 * Pin4   | GND          | GND   |
 * Pin5   | SQCK         | MOSI  |
 * Pin6   | SUBQ         | MISO  |
 * Pin7   | DATA         | SCK   |
 * Pin8   | VCC          | VCC   |
 *******************************************************************************************************************/

/*******************************************************************************************************************
 *                        pointer and variable section
 *******************************************************************************************************************/

#include "MCU.h"
#include "settings.h"
#include "psneerp.pio.h"

#define REQUEST_INJECT_TRIGGER  10
#define REQUEST_INJECT_GAP      5

volatile int wfck_mode = 0;  //Flag initializing for automatic console generation selection 0 = old, 1 = pu-22 end  ++
volatile uint32_t SUBQBuffer32[3]; // Global buffer to store the 12-byte SUBQ channel data

volatile uint32_t request_counter = 0;

// #if defined(DEBUG_SERIAL_MONITOR)
//   uint16_t global_window = 0; // Stores the remaining cycles from the detection window
// #endif

/*******************************************************************************************************************
 *                         Code section
 ********************************************************************************************************************/


// Variable globale pour le PIO de la LED
PIO pioLED = pio1; // Utilise le second bloc PIO pour ne pas gêner SUBQ/BIOS
uint smLED = 0;

void NeoPixel_Init() {
    uint offset = pio_add_program(pioLED, &ws2812_program);
    // Configuration 800kHz pour WS2812B
    float div = clock_get_hz(clk_sys) / (800000 * 10); 
    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, PIN_NEOPIXEL);
    sm_config_set_out_shift(&c, false, true, 24); // Shift Left, Autopush 24 bits
    pio_gpio_init(pioLED, PIN_NEOPIXEL);
    pio_sm_init(pioLED, smLED, offset, &c);
    pio_sm_set_enabled(pioLED, smLED, true);
}

void SetLED(uint32_t color) {
    pio_sm_put_blocking(pioLED, smLED, color << 8);
}

void SetLEDDynamic(uint32_t color, uint8_t counter) {
    if (counter == 0) {
        pio_sm_put_blocking(pioLED, smLED, 0);
        return;
    }

    // Protection : on plafonne le compteur à 20 pour éviter les calculs erronés
    if (counter > 20) counter = 20;

    // Calcul de l'intensité : 
    // On utilise le compteur comme multiplicateur. 
    // 20 * 12 ≈ 240 (proche du max 255)
    uint8_t intensity = counter * 12;

    uint32_t g = (((color >> 24) & 0xFF) * intensity) >> 8;
    uint32_t r = (((color >> 16) & 0xFF) * intensity) >> 8;
    uint32_t b = (((color >> 8) & 0xFF) * intensity) >> 8;

    uint32_t final_color = (g << 24) | (r << 16) | (b << 8);
    pio_sm_put_blocking(pioLED, smLED, final_color);
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

volatile uint8_t patch_stage = 0; // Pour suivre l'avancée du patch
uint smPATCH = 0;                // Le numéro de la machine d'état pour le BIOS


void on_pio_patch_irq() {
    if (pio_interrupt_get(pio0, 0)) {
        patch_stage++; 
        pio_interrupt_clear(pio0, 0);
    }
}

void Bios_Patching(void) {
    #if defined(PATCH_SWITCHE)
        PIN_SWITCH_INPUT;
        gpio_pull_up(PIN_SWITCH); 
        sleep_us(10);
        if (PIN_SWITCH_READ == 0) return;
    #endif

    irq_set_exclusive_handler(PIO0_IRQ_0, on_pio_patch_irq);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio_set_irq0_source_enabled(pio0, pis_interrupt0, true);

    patch_stage = 0;
    uint8_t current_confirms = 0;

    // --- PHASE 1 & 2 : SILENCE (AX) ---
    SetLEDDynamic(LED_RED, 15); // ROUGE pour le premier patch
    while (current_confirms < CONFIRM_COUNTER_TARGET) {
        uint32_t count = SILENCE_THRESHOLD; 
        while (count > 0) {
            if (gpio_get(PIN_AX)) {
                while (gpio_get(PIN_AX)); 
                break; 
            }
            count--;
        }
        if (count == 0) current_confirms++;
    }

    // --- PHASE 3 : INJECTION AX ---

    pio_sm_put_blocking(pio0, smPATCH, PULSE_COUNT_PIO);
    pio_sm_put_blocking(pio0, smPATCH, BIT_OFFSET_VAL);
    pio_sm_put_blocking(pio0, smPATCH, OVERRIDE_VAL);

    while (patch_stage < 1) tight_loop_contents();

    #ifdef PHASE_TWO_PATCH
        SetLEDDynamic(LED_ORANGE, 15); // ORANGE pour le second patch
        current_confirms = 0;
        // --- PHASE 4 : SILENCE (GAP) ---
        while (current_confirms < CONFIRM_COUNTER_TARGET_2) {
            uint32_t count = SILENCE_THRESHOLD;
            while (count > 0) {
                if (gpio_get(PIN_AX)) {
                    while (gpio_get(PIN_AX));
                    break;
                }
                count--;
            }
            if (count == 0) current_confirms++;
        }

        // --- PHASE 5 : INJECTION AY ---

        pio_sm_put_blocking(pio0, smPATCH, PULSE_COUNT_2_PIO);
        pio_sm_put_blocking(pio0, smPATCH, BIT_OFFSET_2_VAL);
        pio_sm_put_blocking(pio0, smPATCH, OVERRIDE_2_VAL);

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
 * SIGNAL CHARACTERISTICS:
 *    - Legacy Boards (PU-7 to PU-20): WFCK acts as a static GATE signal. 
 *      It remains HIGH.
 *    - Modern Boards (PU-22 or newer): WFCK is an oscillating clock signal 
 *      (Frequency-based).
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

void BoardDetection() {
  wfck_mode = 0;           // Default: Legacy (GATE)
  int pulse_hits = 25;     // We need to see 25 oscillations to confirm FREQUENCY mode
  int detectionWindow = 150000; 
  sleep_ms(300);          // Wait for WFCK to stabilize (High on Legacy, Oscillation on Modern)

  while (--detectionWindow) {
    /**
     * If WFCK is "CONTINUOUS" (Legacy), it stays HIGH. gpio_get(PIN_WFCK) will always be 1.
     * If WFCK is "FREQUENCY" (Modern), it will hit 0 (LOW) periodically.
     */
    if (!gpio_get(PIN_WFCK)) {  // Detect a LOW state (only possible in FREQUENCY mode)
      
      pulse_hits--;        // Record one oscillation hit

      if (pulse_hits == 0) {
        wfck_mode = 1;     // Confirmed: FREQUENCY mode (PU-22 or newer)
        SetLEDDynamic(LED_CYAN, 15);
        #if defined(DEBUG_SERIAL_MONITOR)
          global_window = detectionWindow;
        #endif
        gpio_disable_pulls(PIN_WFCK);
        return;            // Exit as soon as we are sure
      }

      /**
       * SYNC: Wait for the signal to go HIGH again.
       * This ensures we count each pulse of the "FREQUENCY" signal only once.
       */
      while (!gpio_get(PIN_WFCK) && detectionWindow > 0) {
        detectionWindow--;
        if(detectionWindow == 0) {
            SetLEDDynamic(LED_MAGENTA, 15);
            return;            // Exit as soon as we are sure
        }
      }
    }
  }
  gpio_disable_pulls(PIN_WFCK);
  // If the window expires without seeing enough LOW pulses, it remains wfck_mode = 0 (GATE)
  #if defined(DEBUG_SERIAL_MONITOR)
    BoardDetectionLog(global_window, wfck_mode, injectSCEx);
  #endif
  
}

/******************************************************************************************************************
 * FUNCTION    : CaptureSUBQ
 * 
 * DESCRIPTION : 
 *    Captures a complete 12-byte SUBQ frame.
 *    Synchronizes with the SQCK (SUBQ Clock) pin to shift in serial data.
 *    Implementation: Synchronous Serial, LSB first reconstruction.
 ******************************************************************************************************************/


PIO pioSUBQ = pio0;       // Instance du bloc PIO (pio0 ou pio1)
uint smSUBQ = 0;          // Index de la machine d'état (0 à 3)
uint offsetSUBQ;          // Adresse du programme dans la mémoire d'instruction PIO

/**
 * Initialisation du PIO avec nomenclature explicite
 */
void subq_pio_init(PIO pioSUBQ_inst, uint smSUBQ_inst, uint offsetSUBQ_inst, uint pin_subq, uint pin_sqck) {
    pio_sm_config configSUBQ = subq_capture_program_get_default_config(offsetSUBQ_inst);
    
    // Le PIO lira PIN_SUBQ (index 0) et attendra sur PIN_SQCK (index 1)
    sm_config_set_in_pins(&configSUBQ, pin_subq);
    
    // Configuration : Shift Right (LSB first), Autopush activé, Seuil 32 bits
    sm_config_set_in_shift(&configSUBQ, true, true, 32);
    
    pio_gpio_init(pioSUBQ_inst, pin_subq);
    pio_gpio_init(pioSUBQ_inst, pin_sqck);
    
    pio_sm_init(pioSUBQ_inst, smSUBQ_inst, offsetSUBQ_inst, &configSUBQ);
    pio_sm_set_enabled(pioSUBQ_inst, smSUBQ_inst, true);
}



/**
 * Fonction de capture (Version corrigée et explicite)
 */
void CaptureSUBQ(void) {
    pio_sm_clear_fifos(pioSUBQ, smSUBQ);

    // Capture directe des 3 mots de 32 bits (96 bits total)
    for (int i = 0; i < 3; i++) {
        SUBQBuffer32[i] = pio_sm_get_blocking(pioSUBQ, smSUBQ);
    }

    #if defined(DEBUG_SERIAL_MONITOR)
        CaptureSUBQLog((uint32_t*)SUBQBuffer32);
    #endif
}


/******************************************************************************************
 * FUNCTION    : Filter_SUBQ_Samples() [SCPH_5903 Dual-Interface Variant]
 * 
 * DESCRIPTION : 
 *    Parses and filters the raw serial data stream from the SUBQ pin specifically 
 *    for the SCPH-5903 model to differentiate between Game Discs and Video CDs (VCD).
 * 
 *    1. VCD EXCLUSION: Specifically filters out VCD Lead-In patterns (sub-mode 0x02) 
 *       to prevent incorrect region injection on non-game media.
 *    2. SUBQ HIT COUNTING: Increments 'request_counter' for valid PlayStation TOC (A0-A2) 
 *       markers or active game tracking.
 *    3. SIGNAL DECAY: Decrements the counter when SUBQ samples match VCD patterns 
 *       or unknown data, ensuring stable disc identification.
 * 
 * INPUT       : isDataSector (bool) - Filtered flag based on raw sector control bits.
 ******************************************************************************************/
#ifdef SCPH_5903

  void FilterSUBQSamples(uint8_t controlByte) {
      
      // --- STEP 0: Data/TOC Validation ---
      // Optimized mask (0xD0) to verify bit6 is SET while bit7 and bit4 are CLEARED.
      uint8_t isDataSector = ((controlByte & 0xD0) == 0x40);

      // --- STEP 1: SUBQ Frame Synchronization ---
      // Fast filtering: ignore raw data if sync markers (index 1 and 6) are not 0x00.
      if (SUBQBuffer[1] == 0x00 && SUBQBuffer[6] == 0x00) {

          /** 
          * HIT INCREMENT CONDITIONS:
          * A. VALID PSX LEAD-IN: Data sector AND Point A0-A2 range AND NOT VCD (sub-mode != 0x02).
          *    (uint8_t)(SUBQBuffer[2] - 0xA0) <= 2 is an optimized check for 0xA0, 0xA1, 0xA2.
          * B. TRACKING MAINTENANCE: Keeps count if already synced and reading Mode 0x01 or Data.
          */
          if ( (isDataSector && (uint8_t)(SUBQBuffer[2] - 0xA0) <= 2 && SUBQBuffer[3] != 0x02) ||
              (request_counter > 0 && (SUBQBuffer[0] == 0x01 || isDataSector)) ) 
          {
              request_counter++; // Direct increment: faster on 16MHz AVR
              return;
          }
      }

      // --- STEP 2: Signal Decay / Pattern Mismatch ---
      // Decrement the hit counter if no valid PSX pattern is detected in the SUBQ stream.
      if (request_counter > 0) {
          request_counter--; // Direct decrement: saves CPU cycles
      }
  }
#else

/******************************************************************************************
 * FUNCTION    : FilterSUBQSamples()
 * 
 * DESCRIPTION : 
 *    Parses and filters the raw serial data stream from the SUBQ pin.
 *    Increments a hit counter (request_counter) when specific patterns are identified 
 *    in the SUBQ stream, confirming the laser is reading the region-check area.
 * 
 *    1. RAW BUS FILTERING: Validates SUBQ framing by checking sync markers (index 1 & 6).
 *    2. PATTERN MATCHING: Detects Lead-In TOC (A0-A2) or Track 01 at the spiral start.
 *    3. SIGNAL DECAY: Decrements the counter if the current SUBQ sample does not 
 *       match expected PlayStation protection patterns.
 * 
 * INPUT       : isDataSector (bool) - Filtered flag based on raw sector control bits.
 ******************************************************************************************/

void FilterSUBQSamples(void) { // Plus besoin de passer controlByte
    // Chargement du premier mot (Octets 0, 1, 2, 3)
    uint32_t word0 = SUBQBuffer32[0]; 
    // Sync Check simultané (Octet 1 et Octet 6) ---
    uint32_t word1 = SUBQBuffer32[1]; // Octets 4, 5, 6, 7
    
    // --- STEP 0: Extraction et Validation du Control Byte (Octet 0) ---
    // On isole l'octet 0, puis on applique le masque (0xD0) pour vérifier (0x40)
    // En une seule opération :
    bool isDataSector = ((word0 & 0xD0) == 0x40);

    // Vérifie si Octet 1 (bits 8-15 de word0) ET Octet 6 (bits 16-23 de word1) sont à 0
    if (!((word0 & 0xFF00) | (word1 & 0xFF0000))) {

        // Extraction rapide par décalage pour les calculs de seuil
        uint8_t track = (uint8_t)((word0 >> 16) & 0xFF); // Octet 2
        uint8_t index = (uint8_t)((word0 >> 24) & 0xFF); // Octet 3

        // Condition A: Lead-in / Track 01
        bool conditionA = (isDataSector && (track >= 0xA0 || 
                          (track == 0x01 && ((uint8_t)(index - 0x03) >= 0xF5))));
        
        // Condition B: Tracking Lock
        // (word0 & 0xFF) est l'octet 0 (Control Byte / ADR)
        bool conditionB = (request_counter > 0 && ((word0 & 0xFF) == 0x01 || isDataSector));

        if (conditionA || conditionB) {
            request_counter++;
            SetLEDDynamic(LED_GREEN, request_counter);
            return;
        }
    }

    // --- STEP 2: Signal Decay ---
    if (request_counter > 0) {
        request_counter--;
        SetLEDDynamic(LED_GREEN, request_counter);
    }
}

 #endif
/*********************************************************************************************
* FUNCTION    : PerformInjectionSequence
 * 
 * DESCRIPTION :
 *    Executes the SCEx injection sequence to bypass the CD-ROM regional lockout.
 *    Supports two hardware-specific injection methods:
 * 
 *    1. Legacy Gate Mode (PU-7 to PU-20): Modchip acts as a logic gate to pull 
 *       the signal down. WFCK is simulated by the chip if necessary.
 *
 *    2. WFCK SYNC MODE (PU-22 and later): 
 *       Synchronizes data injection with the console's hardware WFCK clock.
 *       The signal is modulated on every clock edge to ensure  
 *       alignment with the CD-ROM controller's internal timing.
 * 
 * NOTE: WFCK frequency is approx. 7.3 kHz during initialization/region check, 
 *       but doubles to 14.6 kHz during normal data reading. The modulation loop 
 *       handles both speeds as it syncs directly to the signal edges.
 *********************************************************************************************/



void PerformInjectionSequence(uint8_t injectSCEx) {
  // Codes SCEx convertis en 64 bits (44 bits utiles)
  static const uint64_t scex_patterns[] = {
      0x000002EAD5B24D99ULL, // SCEE
      0x000002FAD5B24D99ULL, // SCEA
      0x000002DAD5B24D99ULL  // SCEI
  };



    
    if (!wfck_mode) {
        // Le mode Legacy (Timer) est mieux géré par un second programme PIO simplifié
        // ou une boucle de délais classiques. Ici on traite le mode WFCK (PU-22+).
    }

    for (uint8_t regionCycle = 0; regionCycle < 3; regionCycle++) {
        uint8_t regionIndex = (injectSCEx == 3) ? regionCycle : injectSCEx;
        uint64_t pattern = scex_patterns[regionIndex];

        // Envoi des 44 bits au FIFO du PIO
        // Le PIO va consommer ces bits un par un et gérer les 30 pulses WFCK par bit
        for (int i = 0; i < 44; i++) {
            uint32_t bit = (pattern >> i) & 0x01;
            pio_sm_put_blocking(pioSUBQ, smSUBQ, bit);
        }

        if (injectSCEx != 3) break;
        
        sleep_ms(90); // Délai inter-région pour mode universel
    }

    gpio_disable_pulls(PIN_DATA); // Libère le bus

    #ifdef LED_RUN
        PIN_LED_OFF;
    #endif
}


void Init() {

  
  NeoPixel_Init();

// 2. Initialisation (Reset vers contrôleur SIO)
    gpio_init(PIN_DATA);
    gpio_init(PIN_WFCK);
    gpio_init(PIN_SQCK);
    gpio_init(PIN_SUBQ);


    gpio_disable_pulls(PIN_DATA);
    gpio_disable_pulls(PIN_WFCK);
    gpio_disable_pulls(PIN_SQCK);
    gpio_disable_pulls(PIN_SUBQ);


    gpio_set_dir(PIN_WFCK, GPIO_IN);
    gpio_set_dir(PIN_SQCK, GPIO_IN);
    gpio_set_dir(PIN_SUBQ, GPIO_IN);
    
    #ifdef BIOS_PATCH
        gpio_init(PIN_AX);
        gpio_init(PIN_AY);
        gpio_init(PIN_DX);
        gpio_disable_pulls(PIN_AX);
        gpio_disable_pulls(PIN_AY);
        gpio_disable_pulls(PIN_DX);
    #endif



      // Chargement du programme
    offsetSUBQ = pio_add_program(pioSUBQ, &subq_capture_program);
    
    // Appel avec les variables globales explicites
    subq_pio_init(pioSUBQ, smSUBQ, offsetSUBQ, PIN_SUBQ, PIN_SQCK);
    


}

int main() {
    stdio_init_all();
    Init();

   // --- Critical Boot Patching ---
   #ifdef BIOS_PATCH

  // Execute BIOS patching 
   Bios_Patching();
   #endif

    // Identify board revision (PU-7 to PU-22+) to set correct injection timings
  BoardDetection();
  

  #if defined(DEBUG_SERIAL_MONITOR)
    // Display initial board detection results (Window remaining & WFCK mode)
    BoardDetectionLog( global_window, wfck_mode, INJECT_SCEx);
  #endif

  while (true) {

    sleep_ms(1);        // Timing Sync: Prevent reading the tail end of the previous SUBQ packet
    
    CaptureSUBQ();       // DATA ACQUISITION: Capture the 12-byte SUBQ stream.

     
    /** 
     * FILTRAGE : Analyse du buffer en 32 bits.
     * La mise à jour de la LED (LED_GREEN) est gérée à l'intérieur
     * de FilterSUBQSamples en fonction du request_counter.
     */
    FilterSUBQSamples();

    /** 
     * INJECTION TRIGGER: Once enough valid requests are detected, 
     * trigger the SCEx injection sequence.
     */
    if (request_counter >= REQUEST_INJECT_TRIGGER) {
      
        PerformInjectionSequence(INJECT_SCEx);      // Execute the SCEx injection burst

        // STEALTH GAP: Mimics natural CD behavior to bypass anti-mod detection.
        request_counter = (REQUEST_INJECT_TRIGGER - REQUEST_INJECT_GAP);
        SetLEDDynamic(LED_GREEN, request_counter);
           
    }
   }
   return 0;
}




