/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file shell.c
 * @brief Shell Commands for I2S Signal Generator (Encapsulated)
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date 2026-05-27
 */

#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>
#include "generators.h"

/* Forward declaration from main.c */
extern struct gen_state *get_gen_state(void);
extern void audio_stream_start(void);
extern void audio_stream_stop(void);

static int cmd_gen_start(const struct shell *sh, size_t argc, char **argv) { audio_stream_start(); return 0; }
static int cmd_gen_stop(const struct shell *sh, size_t argc, char **argv) { audio_stream_stop(); return 0; }

static int cmd_gen_wave_sine(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_SINE); return 0; }
static int cmd_gen_wave_square(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_SQUARE); return 0; }
static int cmd_gen_wave_triangle(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_TRIANGLE); return 0; }
static int cmd_gen_wave_saw(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_SAWTOOTH); return 0; }
static int cmd_gen_wave_white(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_WHITE_NOISE); return 0; }
static int cmd_gen_wave_pink(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_PINK_NOISE); return 0; }
static int cmd_gen_wave_dirac(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_DIRAC); return 0; }
static int cmd_gen_wave_silence(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_SILENCE); return 0; }
static int cmd_gen_wave_lrswap(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_LR_SWAP); return 0; }
static int cmd_gen_wave_imd(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_IMD); return 0; }
static int cmd_gen_wave_jtest(const struct shell *sh, size_t argc, char **argv) { gen_set_type(get_gen_state(), WAVE_JTEST); return 0; }

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gen_wave,
	SHELL_CMD(sine, NULL, "Sine wave", cmd_gen_wave_sine),
	SHELL_CMD(square, NULL, "Square wave", cmd_gen_wave_square),
	SHELL_CMD(triangle, NULL, "Triangle wave", cmd_gen_wave_triangle),
	SHELL_CMD(saw, NULL, "Sawtooth wave", cmd_gen_wave_saw),
	SHELL_CMD(white, NULL, "White noise", cmd_gen_wave_white),
	SHELL_CMD(pink, NULL, "Pink noise", cmd_gen_wave_pink),
	SHELL_CMD(dirac, NULL, "Dirac comb (periodic impulse)", cmd_gen_wave_dirac),
	SHELL_CMD(silence, NULL, "Digital silence", cmd_gen_wave_silence),
	SHELL_CMD(lrswap, NULL, "L/R channel ID (alternating)", cmd_gen_wave_lrswap),
	SHELL_CMD(imd, NULL, "IMD test (SMPTE 60Hz + 7kHz)", cmd_gen_wave_imd),
	SHELL_CMD(jtest, NULL, "J-Test jitter diagnostic", cmd_gen_wave_jtest),
	SHELL_SUBCMD_SET_END
);

static int cmd_gen_freq(const struct shell *sh, size_t argc, char **argv)
{
	float freq = atof(argv[1]);
	if (freq <= 0.0f) {
		shell_error(sh, "Invalid frequency: %s", argv[1]);
		return -EINVAL;
	}
	int ret = gen_set_frequency(get_gen_state(), freq);
	if (ret < 0) {
		shell_error(sh, "Frequency out of bounds (max %.1f Hz)", (double)(get_gen_state()->sample_rate / 2.0f));
		return ret;
	}
	shell_print(sh, "Frequency set to: %.2f Hz", (double)freq);
	return 0;
}

static int cmd_gen_level(const struct shell *sh, size_t argc, char **argv)
{
	struct gen_state *state = get_gen_state();
	if (strcmp(argv[1], "pro") == 0) {
		gen_set_level(state, LEVEL_PRO);
	} else if (strcmp(argv[1], "consumer") == 0) {
		gen_set_level(state, LEVEL_CONSUMER);
	} else if (strcmp(argv[1], "max") == 0) {
		gen_set_level(state, LEVEL_MAX);
	} else {
		shell_error(sh, "Unknown level type: %s (use pro, consumer, max)", argv[1]);
		return -EINVAL;
	}
	shell_print(sh, "Output level set to: %s", argv[1]);
	return 0;
}

static int cmd_gen_phase(const struct shell *sh, size_t argc, char **argv)
{
	float phase = atof(argv[1]);
	gen_set_phase_offset(get_gen_state(), phase);
	shell_print(sh, "Right channel phase offset set to: %.2f degrees", (double)phase);
	return 0;
}

static int cmd_gen_burst_start(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t on = atoi(argv[1]);
	uint32_t off = atoi(argv[2]);
	if (on == 0) {
		shell_error(sh, "On-cycles must be > 0");
		return -EINVAL;
	}
	int ret = gen_set_burst(get_gen_state(), on, off);
	if (ret < 0) {
		shell_error(sh, "Cannot start burst: invalid base frequency");
		return ret;
	}
	shell_print(sh, "Tone burst started: %u cycles on, %u cycles off", on, off);
	return 0;
}

static int cmd_gen_burst_stop(const struct shell *sh, size_t argc, char **argv)
{
	gen_stop_burst(get_gen_state());
	shell_print(sh, "Tone burst disabled.");
	return 0;
}

static int cmd_gen_sweep_start(const struct shell *sh, size_t argc, char **argv)
{
	float start_hz = atof(argv[1]);
	float end_hz = atof(argv[2]);
	uint32_t duration_ms = atoi(argv[3]);

	int ret = gen_set_sweep(get_gen_state(), start_hz, end_hz, duration_ms);
	if (ret < 0) {
		shell_error(sh, "Invalid sweep parameters or out of bounds (max %.1f Hz)", (double)(get_gen_state()->sample_rate / 2.0f));
		return ret;
	}

	shell_print(sh, "Logarithmic sweep started: %.2f Hz -> %.2f Hz over %u ms",
		    (double)start_hz, (double)end_hz, duration_ms);
	return 0;
}

static int cmd_gen_sweep_stop(const struct shell *sh, size_t argc, char **argv)
{
	gen_stop_sweep(get_gen_state());
	shell_print(sh, "Sweep stopped.");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gen_sweep,
	SHELL_CMD_ARG(start, NULL, "Start log sweep <start_hz> <end_hz> <duration_ms>", cmd_gen_sweep_start, 4, 0),
	SHELL_CMD(stop, NULL, "Stop frequency sweep", cmd_gen_sweep_stop),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gen_burst,
	SHELL_CMD_ARG(start, NULL, "Start burst <on_cycles> <off_cycles>", cmd_gen_burst_start, 3, 0),
	SHELL_CMD(stop, NULL, "Stop burst mode", cmd_gen_burst_stop),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gen,
	SHELL_CMD(start, NULL, "Start I2S stream", cmd_gen_start),
	SHELL_CMD(stop, NULL, "Stop I2S stream", cmd_gen_stop),
	SHELL_CMD(wave, &sub_gen_wave, "Select waveform type", NULL),
	SHELL_CMD_ARG(freq, NULL, "Set frequency <hz>", cmd_gen_freq, 2, 0),
	SHELL_CMD_ARG(level, NULL, "Set level <pro|consumer|max>", cmd_gen_level, 2, 0),
	SHELL_CMD_ARG(phase, NULL, "Set R-channel phase offset <degrees>", cmd_gen_phase, 2, 0),
	SHELL_CMD(burst, &sub_gen_burst, "Tone burst control", NULL),
	SHELL_CMD(sweep, &sub_gen_sweep, "Frequency sweep control", NULL),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(gen, &sub_gen, "I2S Generator commands", NULL);