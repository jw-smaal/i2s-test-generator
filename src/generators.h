/*
 * Copyright (c) 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file generators.h
 * @brief CMSIS-DSP Waveform Generators
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date 2026-05-27
 */

#ifndef GENERATORS_H
#define GENERATORS_H

#include <stdint.h>
#include <arm_math.h>
#include <stdbool.h>

/**
 * @brief Available waveform types for the generator.
 */
typedef enum {
	WAVE_SINE,          /**< Pure sine wave (CMSIS-DSP FastMath) */
	WAVE_SQUARE,        /**< Square wave (Fs/2 alias rich) */
	WAVE_TRIANGLE,      /**< Triangle wave */
	WAVE_SAWTOOTH,      /**< Sawtooth wave */
	WAVE_WHITE_NOISE,   /**< Flat spectrum white noise */
	WAVE_PINK_NOISE,    /**< -3dB/Octave pink noise (Voss-McCartney) */
	WAVE_DIRAC,         /**< Periodic Dirac impulse (comb) */
	WAVE_SILENCE,       /**< Digital silence (all zeros) */
	WAVE_LR_SWAP,       /**< Alternating L/R channel ID (1Hz toggle) */
	WAVE_IMD,           /**< SMPTE Intermodulation Distortion test (60Hz + 7kHz) */
	WAVE_JTEST          /**< Julian Dunn J-Test for jitter diagnostics */
} gen_type_t;

/**
 * @brief Standardized calibrated output levels.
 */
typedef enum {
	LEVEL_MAX,      /**< 0 dBFS (Maximum digital output) */
	LEVEL_PRO,      /**< +4 dBu (SMPTE/AES standard, ~ -16 dBFS) */
	LEVEL_CONSUMER  /**< -10 dBV (IEC consumer standard, ~ -28 dBFS) */
} gen_level_t;

/* Level scaling factors (Q15) 
 * Calibrated for 2.165 Vrms Full-Scale Output
 */
#define SCALE_MAX      Q15_MAX
#define SCALE_PRO      0x4899  /* 0.567 (+4 dBu) */
#define SCALE_CONSUMER 0x12AD  /* 0.146 (-10 dBV) */

/* Fixed-point and NCO constants */
#define NCO_RANGE           4294967296.0f /* 2^32 (Range of 32-bit accumulator) */
#define DEGREES_PER_REV     360.0f
#define Q15_HALF_VAL        0x4000
#define Q15_MID_OFFSET      32768         /* Offset to center unsigned noise */
#define Q15_LSB_BIT         0x0001

/* Shift constants for NCO to Q15 conversion */
#define PHASE_TO_Q15_SHIFT  16            /* Shift 32-bit phase to 16-bit angle */
#define TRIANGLE_Q31_SHIFT  15
#define IMD_MIX_Q31_SHIFT   15

/* Conversion constants */
#define MS_PER_SEC          1000.0f

/* Diagnostic Waveform Constants */
#define IMD_LOW_FREQ_HZ     60.0f
#define IMD_HIGH_FREQ_HZ    7000.0f
#define IMD_LOW_RATIO_Q15   26214         /* 0.8 in Q15 */
#define IMD_HIGH_RATIO_Q15  6554          /* 0.2 in Q15 */

#define JTEST_CARRIER_DIV   2             /* Fs/4 (toggles every 2 samples) */
#define JTEST_MOD_DIV       96            /* Fs/192 (toggles every 96 samples) */

#define PINK_ROWS 12

/**
 * @brief Encapsulated state context for the signal generator.
 * 
 * This structure holds all the configuration, phase accumulators, and history
 * required to generate the audio streams without relying on global variables.
 */
struct gen_state {
	gen_type_t type;            /**< Current waveform type */
	gen_level_t level;          /**< Current calibrated output level */
	float32_t freq;             /**< Base frequency in Hz */
	float32_t sample_rate;      /**< System sample rate in Hz */
	float32_t inv_sample_rate;  /**< Pre-calculated 1/Fs for performance */
	q15_t scale;                /**< Current Q15 scaling factor */

	/* Phase accumulator */
	q31_t phase_acc;            /**< 32-bit Numerically Controlled Oscillator phase */
	q31_t phase_inc;            /**< Phase step per sample */
	q31_t phase_offset;         /**< Phase offset applied to the Right channel */

	/* Burst state */
	bool burst_active;          /**< True if tone burst mode is enabled */
	uint32_t burst_on_cycles;   /**< Number of full cycles the tone is ON */
	uint32_t burst_off_cycles;  /**< Number of full cycles the tone is OFF */
	uint32_t burst_on_samples;  /**< Calculated number of ON samples */
	uint32_t burst_off_samples; /**< Calculated number of OFF samples */
	uint32_t burst_counter;     /**< Running counter for burst state machine */

	/* Sweep state */
	bool sweep_active;          /**< True if logarithmic sweep is running */
	float32_t sweep_start_freq; /**< Sweep start frequency (Hz) */
	float32_t sweep_end_freq;   /**< Sweep end frequency (Hz) */
	float32_t sweep_current_freq;/**< Current instantaneous frequency of the sweep */
	float32_t sweep_multiplier; /**< Per-sample frequency multiplier for log sweep */
	uint32_t sweep_samples_total;/**< Total number of samples in the sweep duration */
	uint32_t sweep_samples_current;/**< Current sample index in the sweep */

	/* Pink noise state */
	uint32_t pink_indices;      /**< Running index for Voss-McCartney algorithm */
	q15_t pink_rows[PINK_ROWS]; /**< State array for noise generators */
	q15_t pink_running_sum;     /**< Running sum of the noise generators */

	/* Diagnostic specific state */
	uint32_t lr_swap_timer;     /**< Timer for L/R channel swap toggle */
	bool lr_swap_left_active;   /**< State flag for L/R channel swap */
	q31_t imd_ph_low;           /**< Phase accumulator for IMD 60Hz carrier */
	q31_t imd_ph_high;          /**< Phase accumulator for IMD 7kHz modulation */
	uint32_t jtest_sample_count;/**< Running sample counter for J-Test toggling */
};

/**
 * @brief Initialize the generator state.
 * @param state Pointer to the generator state structure.
 * @param sample_rate The system sample rate in Hz (e.g., 48000, 192000).
 * @return 0 on success, or negative error code.
 */
int gen_init(struct gen_state *state, uint32_t sample_rate);

/**
 * @brief Set the active waveform type.
 * @param state Pointer to the generator state structure.
 * @param type The desired waveform type.
 */
void gen_set_type(struct gen_state *state, gen_type_t type);

/**
 * @brief Set the fundamental frequency.
 * @param state Pointer to the generator state structure.
 * @param freq_hz Frequency in Hertz.
 * @return 0 on success, or negative error code if invalid bounds.
 */
int gen_set_frequency(struct gen_state *state, float freq_hz);

/**
 * @brief Set the calibrated output level.
 * @param state Pointer to the generator state structure.
 * @param level The desired output level (MAX, PRO, CONSUMER).
 */
void gen_set_level(struct gen_state *state, gen_level_t level);

/**
 * @brief Set the phase offset for the Right channel.
 * @param state Pointer to the generator state structure.
 * @param phase_deg Phase shift in degrees (0.0 to 360.0).
 */
void gen_set_phase_offset(struct gen_state *state, float phase_deg);

/**
 * @brief Enable Tone Burst mode (typically for Sine waves).
 * @param state Pointer to the generator state structure.
 * @param on_cycles Number of complete wave cycles the signal is active.
 * @param off_cycles Number of complete wave cycles the signal is muted.
 * @return 0 on success, or negative error code if frequency is invalid.
 */
int gen_set_burst(struct gen_state *state, uint32_t on_cycles, uint32_t off_cycles);

/**
 * @brief Disable Tone Burst mode.
 * @param state Pointer to the generator state structure.
 */
void gen_stop_burst(struct gen_state *state);

/**
 * @brief Start a logarithmic frequency sweep.
 * @param state Pointer to the generator state structure.
 * @param start_hz Starting frequency in Hertz.
 * @param end_hz Ending frequency in Hertz.
 * @param duration_ms Total duration of the sweep in milliseconds.
 * @return 0 on success, or negative error code.
 */
int gen_set_sweep(struct gen_state *state, float start_hz, float end_hz, uint32_t duration_ms);

/**
 * @brief Stop the active frequency sweep.
 * @param state Pointer to the generator state structure.
 */
void gen_stop_sweep(struct gen_state *state);

/**
 * @brief Generate the next block of interleaved audio samples.
 * @param state Pointer to the generator state structure.
 * @param buffer Pointer to the destination buffer (must be large enough).
 * @param num_samples Total number of samples to generate (Left + Right).
 */
void gen_fill_buffer(struct gen_state *state, int16_t *buffer, uint32_t num_samples);

#endif /* GENERATORS_H */
