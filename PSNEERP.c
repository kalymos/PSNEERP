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

#define REQUEST_INJECT_TRIGGER 15 // Now coupled with REQUEST_INJECT_GAP; allows for higher trigger
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

uint offsetPATCH;  

volatile int wfck_mode = 0;  //Flag initializing for automatic console generation selection 0 = old, 1 = pu-22 end  ++
volatile uint32_t SUBQBuffer32[3]; // Global buffer to store the 12-byte SUBQ channel data

volatile uint32_t request_counter = 0;

 #if defined(DEBUG_SERIAL_MONITOR)
   uint16_t global_window = 0; // Stores the remaining cycles from the detection window
 #endif

/*******************************************************************************************************************
 *                         Code section
 ********************************************************************************************************************/
/*******************************************************************************************************************
#          UART 
*********************************************************************************************************************/

// --- Déclarations globales (en haut du fichier) ---
static uint sm_debug; // On la déclare ici pour qu'elle soit visible partout

void init_pio_debug() {
    uint offset = pio_add_program(pio1, &uart_tx_program);  
    sm_debug = pio_claim_unused_sm(pio1, true); // On utilise la variable globale
    
    // Correction du nom ici :
    uart_tx_program_init(pio1, sm_debug, offset, PIN_DEBUG_TX, 115200);;
}

void pio_putc(char c) {
    // On utilise la variable globale sm_debug
    pio_sm_put_blocking(pio1, sm_debug, (uint32_t)c);
}

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
    int pulse_hits = 25;     // Required oscillations to confirm FREQUENCY mode
    int detectionWindow = 1000000; 

    // If we are not patching the BIOS, we wait for the CD processor 
    // to wake up and stabilize WFCK before starting detection.
    #if !defined(BIOS_PATCH)
        sleep_ms(300);          
    #endif        

    while (--detectionWindow) {
        /**
         * If WFCK is "CONTINUOUS" (Legacy), it stays HIGH. gpio_get(PIN_WFCK) will always be 1.
         * If WFCK is "FREQUENCY" (Modern), it will hit 0 (LOW) periodically.
         */
        if (!gpio_get(PIN_WFCK)) {  // Detect a LOW state (only possible in FREQUENCY mode)

            pulse_hits--;            // Register one oscillation hit

            if (pulse_hits == 0) {
            wfck_mode = 1;     // Confirmed: FREQUENCY mode (PU-22 or newer)
            SetLEDDynamic(LED_CYAN, 100);
            #if defined(DEBUG_SERIAL_MONITOR)
            global_window = detectionWindow;
            #endif
            gpio_disable_pulls(PIN_WFCK);
            return;            // Exit as soon as we are sure
        }

            /**
             * SYNC: Wait for the signal to return HIGH.
             * Ensures each pulse of the FREQUENCY signal is counted exactly once.
             * Includes safety timeout to prevent infinite hangs.
             */
            while (!gpio_get(PIN_WFCK) && detectionWindow > 0) {
                detectionWindow--;
                if(detectionWindow == 0) {
                SetLEDDynamic(LED_MAGENTA, 100);
                return;            // Exit as soon as we are sure
            }
        }
    }
  }
  // Cleanup pulls if window expires (Legacy mode)
  gpio_disable_pulls(PIN_WFCK);

  // If the window expires without seeing enough LOW pulses, it remains wfck_mode = 0 (GATE)
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

PIO pioSUBQ = pio0;       // PIO Block Instance (pio0 or pio1)
uint smSUBQ = 0;          // State Machine Index (0 to 3)
uint offsetSUBQ;          // Program offset in PIO instruction memory

/**
 * Capture Function: Retrieves 96 bits of data from the PIO FIFO
 */
void CaptureSUBQ(void) {
    // Clear FIFOs and ISR to ensure we start with fresh data
    pio_sm_clear_fifos(pioSUBQ, smSUBQ);
    pio_sm_exec(pioSUBQ, smSUBQ, pio_encode_mov(pio_isr, pio_null));

    // Direct capture of 3 words (32-bit each), total 96 bits
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
 *    Parses and filters the raw serial data stream from the SUBQ buffer.
 *    Increments a hit counter (request_counter) when specific patterns are identified,
 *    confirming the laser is reading the region-check (Lead-in) area.
 * 
 * LOGIC:
 *    1. RAW BUS FILTERING: Validates SUBQ framing by checking sync markers (Byte 1 & 6).
 *    2. SECTOR ANALYSIS: Filters based on Control/ADR bits (Byte 0).
 *    3. PATTERN MATCHING: Detects Lead-In TOC (A0-A2) or Track 01 at the spiral start.
 *    4. SIGNAL DECAY: Decrements the counter if the sample is invalid or mismatched.
 ******************************************************************************************/

void FilterSUBQSamples(void) {
    // Load the first two 32-bit words for fast parsing
    uint32_t word0 = SUBQBuffer32[0]; // Bytes 0, 1, 2, 3
    uint32_t word1 = SUBQBuffer32[1]; // Bytes 4, 5, 6, 7
    
    // --- STEP 1: Control Byte Validation (Byte 0) ---
    // Isolate Control bits and check for Data Sector (0x40) using mask (0xD0)
    bool isDataSector = ((word0 & 0xD0) == 0x40);

    // --- STEP 2: Sync Check (Simultaneous check for Byte 1 and Byte 6) ---
    // Verifies that Byte 1 (bits 8-15 of word0) AND Byte 6 (bits 16-23 of word1) are zero
    if (!((word0 & 0xFF00) | (word1 & 0xFF0000))) {

        // Extract Track (Byte 2) and Index (Byte 3) using bit shifts
        uint8_t track = (uint8_t)((word0 >> 16) & 0xFF); 
        uint8_t index = (uint8_t)((word0 >> 24) & 0xFF); 

        // Condition A: Lead-in / Track 01 Detection
        bool conditionA = (isDataSector && (track >= 0xA0 || 
                          (track == 0x01 && ((uint8_t)(index - 0x03) >= 0xF5))));
        
        // Condition B: Tracking Lock Persistence
        // (word0 & 0xFF) isolates Byte 0 (Control/ADR)
        bool conditionB = (request_counter > 0 && ((word0 & 0xFF) == 0x01 || isDataSector));

        if (conditionA || conditionB) {
            // Pattern matched: increment counter and update LED
            request_counter++;
            SetLEDDynamic(LED_GREEN, request_counter * 7);
            return;
        }
    }

    // --- STEP 3: Signal Decay ---
    // Decrement counter if the signal is lost or invalid to prevent false triggers
    if (request_counter > 0) {
        request_counter--;
        SetLEDDynamic(LED_GREEN, request_counter * 7);
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

    while (true) {
        // Timing Sync: Prevent reading the tail end of the previous SUBQ packet
        sleep_ms(1);        

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





