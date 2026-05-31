/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief I2S Signal Generator Main Loop and Driver Integration
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date 2026-05-27
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/logging/log.h>
#include "generators.h"

LOG_MODULE_REGISTER(i2s_gen, LOG_LEVEL_INF);

#define SAMPLE_RATE CONFIG_AUDIO_SAMPLE_RATE
#define SAMPLE_BIT_WIDTH CONFIG_AUDIO_BIT_WIDTH
#define CHANNELS 2
#define SAMPLES_PER_FRAME 256
#define FRAME_SIZE (SAMPLES_PER_FRAME * CHANNELS * (SAMPLE_BIT_WIDTH / 8))
#define TX_BLOCK_COUNT 12
#define RX_BLOCK_COUNT 8

K_MEM_SLAB_DEFINE_STATIC(tx_slab, FRAME_SIZE, TX_BLOCK_COUNT, 4);
K_MEM_SLAB_DEFINE_STATIC(rx_slab, FRAME_SIZE, RX_BLOCK_COUNT, 4);

//static const struct device *const i2s_dev = DEVICE_DT_GET(DT_NODELABEL(sai1));
static const struct device *const i2s_dev = DEVICE_DT_GET(DT_ALIAS(i2s_tx));

static struct gen_state g_state;
static K_SEM_DEFINE(stream_start_sem, 0, 1);
static K_SEM_DEFINE(rx_ready_sem, 0, 1);
static bool stream_running = false;

struct gen_state *get_gen_state(void)
{
	return &g_state;
}

void audio_stream_start(void)
{
	if (!stream_running) {
		stream_running = true;
		k_sem_give(&stream_start_sem);
	}
}

void audio_stream_stop(void)
{
	if (stream_running) {
		stream_running = false;
		/* Use DROP to completely flush the TX queue and reset state */
		i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
	}
}

static void audio_thread(void *p1, void *p2, void *p3)
{
	int ret;
	void *tx_buffer;

	while (1) {
		k_sem_take(&stream_start_sem, K_FOREVER);
		
		LOG_INF("Stream started");

		/* 
		 * Prime the DMA pump: On NXP MCUX SAI drivers, we MUST queue exactly 
		 * one audio block BEFORE issuing the START command.
		 */
		ret = k_mem_slab_alloc(&tx_slab, &tx_buffer, K_NO_WAIT);
		if (ret == 0) {
			gen_fill_buffer(&g_state, (int16_t *)tx_buffer, SAMPLES_PER_FRAME * CHANNELS);
			
			/* Use a block-friendly timeout to prevent EAGAIN (-35) on fast CPUs */
			ret = i2s_write(i2s_dev, tx_buffer, FRAME_SIZE);
			if (ret < 0 && ret != -EAGAIN) {
				LOG_ERR("I2S priming write failed: %d", ret);
				k_mem_slab_free(&tx_slab, tx_buffer);
				stream_running = false;
				continue;
			}
			/* If EAGAIN, we just proceed to start, the buffer wasn't queued but we'll catch up */
		}

		/* Start the hardware now that the queue has data */
		ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
		if (ret < 0) {
			LOG_ERR("I2S TX trigger start failed: %d", ret);
			stream_running = false;
			continue;
		}

		/* Main hot loop */
		while (stream_running) {
			ret = k_mem_slab_alloc(&tx_slab, &tx_buffer, K_NO_WAIT);
			if (ret < 0) {
				k_sleep(K_MSEC(1));
				continue;
			}

			gen_fill_buffer(&g_state, (int16_t *)tx_buffer, SAMPLES_PER_FRAME * CHANNELS);

			/* 
			 * Real-time audio: If the queue is full (-EAGAIN), do not block or retry.
			 * The time window has passed, so we simply drop this block and move on.
			 */
			ret = i2s_write(i2s_dev, tx_buffer, FRAME_SIZE);
			if (ret < 0) {
				k_mem_slab_free(&tx_slab, tx_buffer);
				if (ret != -EAGAIN) {
					LOG_ERR("I2S write failed: %d", ret);
					break;
				}
			}
		}
		
		LOG_INF("Stream stopped");
		stream_running = false;
	}
}

static void rx_sink_thread(void *p1, void *p2, void *p3)
{
	void *rx_buffer;
	size_t size;
	int ret;

	/* Wait for RX to be triggered in main */
	k_sem_take(&rx_ready_sem, K_FOREVER);

	while (1) {
		ret = i2s_read(i2s_dev, &rx_buffer, &size);
		if (ret == 0) {
			k_mem_slab_free(&rx_slab, rx_buffer);
		} else {
			k_sleep(K_MSEC(1));
		}
	}
}

K_THREAD_DEFINE(audio_tid, 2048, audio_thread, NULL, NULL, NULL, -2, 0, 0);
K_THREAD_DEFINE(rx_sink_tid, 1024, rx_sink_thread, NULL, NULL, NULL, -1, 0, 0);

int main(void)
{
	struct i2s_config conf;
	int ret;

	LOG_INF("I2S Signal Generator Ready.");

	if (!device_is_ready(i2s_dev)) {
		LOG_ERR("I2S device not ready");
		return -ENODEV;
	}

	conf.word_size = SAMPLE_BIT_WIDTH;
	conf.channels = CHANNELS;
	conf.format = I2S_FMT_DATA_FORMAT_I2S;
	conf.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
	conf.frame_clk_freq = SAMPLE_RATE;
	conf.block_size = FRAME_SIZE;
	conf.timeout = 2000;

	/* Configure TX */
	conf.mem_slab = &tx_slab;
	ret = i2s_configure(i2s_dev, I2S_DIR_TX, &conf);
	if (ret < 0) {
		LOG_ERR("I2S TX configure failed: %d", ret);
		return ret;
	}

	/* Configure RX for Clocks (Dummy Slab) */
	conf.mem_slab = &rx_slab;
	ret = i2s_configure(i2s_dev, I2S_DIR_RX, &conf);
	if (ret < 0) {
		LOG_ERR("I2S RX configure failed: %d", ret);
		return ret;
	}

	gen_init(&g_state, SAMPLE_RATE);

	/* Start Clocks (via RX). */
	ret = i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);
	if (ret < 0) {
		LOG_WRN("I2S RX (Clocks) start failed: %d", ret);
	} else {
		k_sem_give(&rx_ready_sem);
	}

	LOG_INF("Use 'gen' commands in shell to control.");

	return 0;
}
