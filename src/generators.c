/**
 * @file generators.c
 * @brief CMSIS-DSP Waveform Generators Implementation (Encapsulated)
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date 2026-05-27
 */

#include "generators.h"
#include <zephyr/random/random.h>
#include <math.h>
#include <string.h>

/* Level scaling factors (Q15) 
 * Calibrated for 2.165 Vrms Full-Scale Output
 */
#define SCALE_MAX      0x7FFF  // 1.0 (2.165 Vrms)
#define SCALE_PRO      0x4899  // 0.567 (+4 dBu)
#define SCALE_CONSUMER 0x12AD  // 0.146 (-10 dBV)

void gen_init(struct gen_state *state, uint32_t sample_rate)
{
	memset(state, 0, sizeof(struct gen_state));
	state->type = WAVE_SINE;
	state->level = LEVEL_MAX;
	state->freq = 1000.0f;
	state->sample_rate = (float32_t)sample_rate;
	state->inv_sample_rate = 1.0f / state->sample_rate;
	state->scale = SCALE_MAX;
	gen_set_frequency(state, state->freq);
}

void gen_set_type(struct gen_state *state, gen_type_t type)
{
	state->type = type;
	if (type == WAVE_DIRAC) {
		state->phase_acc = 0;
	}
}

void gen_set_frequency(struct gen_state *state, float freq_hz)
{
	state->freq = freq_hz;
	state->phase_inc = (q31_t)((state->freq * state->inv_sample_rate) * 4294967296.0f);

	if (state->burst_active) {
		float32_t samples_per_cycle = state->sample_rate / state->freq;
		state->burst_on_samples = (uint32_t)(state->burst_on_cycles * samples_per_cycle);
		state->burst_off_samples = (uint32_t)(state->burst_off_cycles * samples_per_cycle);
	}
}

void gen_set_level(struct gen_state *state, gen_level_t level)
{
	state->level = level;
	switch (level) {
	case LEVEL_PRO:
		state->scale = SCALE_PRO;
		break;
	case LEVEL_CONSUMER:
		state->scale = SCALE_CONSUMER;
		break;
	case LEVEL_MAX:
	default:
		state->scale = SCALE_MAX;
		break;
	}
}

void gen_set_phase_offset(struct gen_state *state, float phase_deg)
{
	state->phase_offset = (q31_t)((phase_deg / 360.0f) * 4294967296.0f);
}

void gen_set_burst(struct gen_state *state, uint32_t on_cycles, uint32_t off_cycles)
{
	state->burst_on_cycles = on_cycles;
	state->burst_off_cycles = off_cycles;
	state->burst_active = true;
	state->burst_counter = 0;
	gen_set_frequency(state, state->freq);
}

void gen_stop_burst(struct gen_state *state)
{
	state->burst_active = false;
}

void gen_set_sweep(struct gen_state *state, float start_hz, float end_hz, uint32_t duration_ms)
{
	state->sweep_start_freq = start_hz;
	state->sweep_end_freq = end_hz;
	state->sweep_current_freq = start_hz;
	state->sweep_samples_current = 0;
	state->sweep_samples_total = (uint32_t)(duration_ms * (state->sample_rate / 1000.0f));
	
	if (state->sweep_samples_total > 0) {
		state->sweep_multiplier = powf(end_hz / start_hz, 1.0f / (float32_t)state->sweep_samples_total);
		state->sweep_active = true;
	}
}

void gen_stop_sweep(struct gen_state *state)
{
	state->sweep_active = false;
	gen_set_frequency(state, state->freq);
}

static inline void update_sweep_phase(struct gen_state *state)
{
	if (state->sweep_active) {
		state->sweep_current_freq *= state->sweep_multiplier;
		state->sweep_samples_current++;
		
		if (state->sweep_samples_current >= state->sweep_samples_total) {
			state->sweep_current_freq = state->sweep_start_freq;
			state->sweep_samples_current = 0;
		}
		
		state->phase_inc = (q31_t)((state->sweep_current_freq * state->inv_sample_rate) * 4294967296.0f);
	}
}

static void fill_sine(struct gen_state *state, int16_t *buffer, uint32_t num_samples)
{
	for (uint32_t i = 0; i < num_samples; i += 2) {
		update_sweep_phase(state);

		q15_t angle_l = (q15_t)(state->phase_acc >> 16);
		q15_t q_val_l = arm_sin_q15(angle_l);
		
		q15_t angle_r = (q15_t)((state->phase_acc + state->phase_offset) >> 16);
		q15_t q_val_r = arm_sin_q15(angle_r);
		
		arm_scale_q15(&q_val_l, state->scale, 0, &q_val_l, 1);
		arm_scale_q15(&q_val_r, state->scale, 0, &q_val_r, 1);
		
		if (state->burst_active) {
			uint32_t period = state->burst_on_samples + state->burst_off_samples;
			if (state->burst_counter >= state->burst_on_samples) {
				q_val_l = 0;
				q_val_r = 0;
			}
			state->burst_counter++;
			if (state->burst_counter >= period) {
				state->burst_counter = 0;
			}
		}

		buffer[i] = q_val_l;
		buffer[i + 1] = q_val_r;
		
		state->phase_acc += state->phase_inc;
	}
}

static void fill_white_noise(struct gen_state *state, int16_t *buffer, uint32_t num_samples)
{
	for (uint32_t i = 0; i < num_samples; i += 2) {
		uint32_t r = sys_rand32_get();
		q15_t q_val = (q15_t)((r & 0xFFFF) - 32768);
		arm_scale_q15(&q_val, state->scale, 0, &q_val, 1);
		buffer[i] = q_val;
		buffer[i + 1] = q_val;
	}
}

static void fill_pink_noise(struct gen_state *state, int16_t *buffer, uint32_t num_samples)
{
	const float32_t pink_scale = 1.0f / (float32_t)PINK_ROWS;

	for (uint32_t i = 0; i < num_samples; i += 2) {
		state->pink_indices++;
		uint32_t i_tz = __builtin_ctz(state->pink_indices);
		if (i_tz < PINK_ROWS) {
			state->pink_running_sum -= state->pink_rows[i_tz];
			uint32_t r = sys_rand32_get();
			state->pink_rows[i_tz] = (q15_t)(((float32_t)((r & 0xFFFF) - 32768)) * pink_scale);
			state->pink_running_sum += state->pink_rows[i_tz];
		}
		
		q15_t q_val = state->pink_running_sum;
		arm_scale_q15(&q_val, state->scale, 0, &q_val, 1);
		buffer[i] = q_val;
		buffer[i + 1] = q_val;
	}
}

static void fill_geometric(struct gen_state *state, int16_t *buffer, uint32_t num_samples, gen_type_t type)
{
	for (uint32_t i = 0; i < num_samples; i += 2) {
		update_sweep_phase(state);
		q15_t q_val = 0;
		if (type == WAVE_SQUARE) {
			q_val = (state->phase_acc >= 0) ? 0x7FFF : (q15_t)0x8001;
		} else if (type == WAVE_SAWTOOTH) {
			q_val = (q15_t)(state->phase_acc >> 16);
		} else if (type == WAVE_TRIANGLE) {
			q31_t temp = (state->phase_acc < 0) ? ~state->phase_acc : state->phase_acc;
			q_val = (q15_t)((temp >> 15) - 32768);
		}
		arm_scale_q15(&q_val, state->scale, 0, &q_val, 1);
		buffer[i] = q_val;
		buffer[i + 1] = q_val;
		state->phase_acc += state->phase_inc;
	}
}

static void fill_dirac(struct gen_state *state, int16_t *buffer, uint32_t num_samples)
{
	for (uint32_t i = 0; i < num_samples; i += 2) {
		q15_t q_val = 0;
		uint32_t prev_phase = (uint32_t)state->phase_acc;
		state->phase_acc += state->phase_inc;
		uint32_t curr_phase = (uint32_t)state->phase_acc;
		if (curr_phase < prev_phase) {
			q_val = 0x7FFF;
		}
		arm_scale_q15(&q_val, state->scale, 0, &q_val, 1);
		buffer[i] = q_val;
		buffer[i + 1] = q_val;
	}
}

static void fill_silence(int16_t *buffer, uint32_t num_samples)
{
	memset(buffer, 0, num_samples * sizeof(int16_t));
}

static void fill_lr_swap(struct gen_state *state, int16_t *buffer, uint32_t num_samples)
{
	static uint32_t channel_timer = 0;
	static bool left_active = true;

	for (uint32_t i = 0; i < num_samples; i += 2) {
		update_sweep_phase(state);
		q15_t angle = (q15_t)(state->phase_acc >> 16);
		q15_t q_val = arm_sin_q15(angle);
		arm_scale_q15(&q_val, state->scale, 0, &q_val, 1);
		buffer[i] = left_active ? q_val : 0;
		buffer[i + 1] = left_active ? 0 : q_val;
		state->phase_acc += state->phase_inc;
		channel_timer++;
		if (channel_timer >= state->sample_rate) {
			channel_timer = 0;
			left_active = !left_active;
		}
	}
}

static void fill_imd(struct gen_state *state, int16_t *buffer, uint32_t num_samples)
{
	static q31_t ph_low = 0, ph_high = 0;
	const q31_t inc_low = (q31_t)((60.0f * state->inv_sample_rate) * 4294967296.0f);
	const q31_t inc_high = (q31_t)((7000.0f * state->inv_sample_rate) * 4294967296.0f);

	for (uint32_t i = 0; i < num_samples; i += 2) {
		q15_t s_low = arm_sin_q15((q15_t)(ph_low >> 16));
		q15_t s_high = arm_sin_q15((q15_t)(ph_high >> 16));
		q31_t mixed = ((q31_t)s_low * 26214) + ((q31_t)s_high * 6554);
		q15_t q_val = (q15_t)(mixed >> 15);
		arm_scale_q15(&q_val, state->scale, 0, &q_val, 1);
		buffer[i] = q_val;
		buffer[i + 1] = q_val;
		ph_low += inc_low;
		ph_high += inc_high;
	}
}

static void fill_jtest(struct gen_state *state, int16_t *buffer, uint32_t num_samples)
{
	for (uint32_t i = 0; i < num_samples; i += 2) {
		static uint32_t sample_count = 0;
		q15_t q_val = ((sample_count / 2) % 2 == 0) ? 0x4000 : (q15_t)0xC000;
		if ((sample_count / 96) % 2 == 0) {
			q_val |= 0x0001;
		} else {
			q_val &= ~0x0001;
		}
		buffer[i] = q_val;
		buffer[i + 1] = q_val;
		sample_count++;
	}
}

void gen_fill_buffer(struct gen_state *state, int16_t *buffer, uint32_t num_samples)
{
	switch (state->type) {
	case WAVE_SINE:
		fill_sine(state, buffer, num_samples);
		break;
	case WAVE_WHITE_NOISE:
		fill_white_noise(state, buffer, num_samples);
		break;
	case WAVE_PINK_NOISE:
		fill_pink_noise(state, buffer, num_samples);
		break;
	case WAVE_SQUARE:
	case WAVE_TRIANGLE:
	case WAVE_SAWTOOTH:
		fill_geometric(state, buffer, num_samples, state->type);
		break;
	case WAVE_DIRAC:
		fill_dirac(state, buffer, num_samples);
		break;
	case WAVE_SILENCE:
		fill_silence(buffer, num_samples);
		break;
	case WAVE_LR_SWAP:
		fill_lr_swap(state, buffer, num_samples);
		break;
	case WAVE_IMD:
		fill_imd(state, buffer, num_samples);
		break;
	case WAVE_JTEST:
		fill_jtest(state, buffer, num_samples);
		break;
	}
}