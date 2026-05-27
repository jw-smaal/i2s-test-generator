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

typedef enum {
	WAVE_SINE,
	WAVE_SQUARE,
	WAVE_TRIANGLE,
	WAVE_SAWTOOTH,
	WAVE_WHITE_NOISE,
	WAVE_PINK_NOISE,
	WAVE_DIRAC,
	WAVE_SILENCE,
	WAVE_LR_SWAP,
	WAVE_IMD,
	WAVE_JTEST
} gen_type_t;

typedef enum {
	LEVEL_MAX,      // 0 dBFS
	LEVEL_PRO,      // +4 dBu (~ -16 dBFS)
	LEVEL_CONSUMER  // -10 dBV (~ -28 dBFS)
} gen_level_t;

#define PINK_ROWS 12

struct gen_state {
	gen_type_t type;
	gen_level_t level;
	float32_t freq;
	float32_t sample_rate;
	float32_t inv_sample_rate;
	q15_t scale;

	/* Phase accumulator */
	q31_t phase_acc;
	q31_t phase_inc;
	q31_t phase_offset;

	/* Burst state */
	bool burst_active;
	uint32_t burst_on_cycles;
	uint32_t burst_off_cycles;
	uint32_t burst_on_samples;
	uint32_t burst_off_samples;
	uint32_t burst_counter;

	/* Sweep state */
	bool sweep_active;
	float32_t sweep_start_freq;
	float32_t sweep_end_freq;
	float32_t sweep_current_freq;
	float32_t sweep_multiplier;
	uint32_t sweep_samples_total;
	uint32_t sweep_samples_current;

	/* Pink noise state */
	uint32_t pink_indices;
	q15_t pink_rows[PINK_ROWS];
	q15_t pink_running_sum;
};

void gen_init(struct gen_state *state, uint32_t sample_rate);
void gen_set_type(struct gen_state *state, gen_type_t type);
void gen_set_frequency(struct gen_state *state, float freq_hz);
void gen_set_level(struct gen_state *state, gen_level_t level);
void gen_set_phase_offset(struct gen_state *state, float phase_deg);
void gen_set_burst(struct gen_state *state, uint32_t on_cycles, uint32_t off_cycles);
void gen_stop_burst(struct gen_state *state);
void gen_set_sweep(struct gen_state *state, float start_hz, float end_hz, uint32_t duration_ms);
void gen_stop_sweep(struct gen_state *state);
void gen_fill_buffer(struct gen_state *state, int16_t *buffer, uint32_t num_samples);

#endif /* GENERATORS_H */
