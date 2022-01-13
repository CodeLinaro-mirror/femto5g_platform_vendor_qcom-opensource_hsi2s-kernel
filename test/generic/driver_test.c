/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Test app for hs-i2s driver
 */

#define _GNU_SOURCE

/* Headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>
#include <poll.h>
#include <sys/epoll.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include "hsi2s_common.h"

/* Macros */
#define BYTES_PER_WORD 4
#define READ_LENGTH_MB 2
#define READ_LENGTH_WORDS (READ_LENGTH_MB * 1024 * 1024) / BYTES_PER_WORD
#define READ_LIMIT 2048000 * 8 * 60 /* 60s of ramp data */
#define SRC_DIGITAL_PLL 0x500
#define BILLION 1000000000L
#define DAB_TUNER_COUNT 3
#define PG_SIZE 4096
#define SHM_SIZE (PG_SIZE * 5)
#define SHM_WRDMA_BASE 0
#define SHM_WRDMA_CURRENT 1
#define BBIQ_ALLOWED_ERROR  2
#define BBIQ_DEBUG_ERROR_MOD  1

/* Operation mode of the test utility */
enum operation_mode {
	NORMAL_RX,
	NORMAL_TX,
	INTERNAL_LB,
	EXTERNAL_LB_MASTER,
	EXTERNAL_LB_MASTER_SLAVE,
	SET_MUXMODE,
	CONFIG_M_CLK,
	CONFIG_I2S_PARAMS,
	CONFIG_PCM_PARAMS,
	CONFIG_TDM_PARAMS,
	CONFIG_LPAIF_MODE,
	CONFIG_PCM_LANE,
	TOGGLE_BIT_CLK,
	CONFIG_DAB_MRC
};

/* I2S parameters */
struct i2s_params {
	uint32_t bit_clk;
	uint32_t buffer_ms;
	uint32_t bit_depth;
	uint32_t spkr_channel_count;
	uint32_t mic_channel_count;
	uint8_t en_long_rate;
	uint32_t long_rate;
};

/* PCM parameters */
struct pcm_params {
	uint32_t bit_clk;
	uint32_t buffer_ms;
	uint8_t rate;
	uint8_t sync_src;
	uint8_t aux_mode;
	uint8_t rpcm_width;
	uint8_t tpcm_width;
};

/* TDM parameters */
struct tdm_params {
	uint8_t sync_delay;
	uint32_t tpcm_width;
	uint32_t rpcm_width;
	uint32_t rate;
	uint8_t en_diff_sample_width;
	uint32_t tpcm_sample_width;
	uint32_t rpcm_sample_width;
};

/* Thread params */
struct thread_params {
	struct pollfd pfd;
	void *mmap_ptr;
	void *mmap_read;
	void *mmap_end;
	void *sh_mem;
	FILE *fd_write_op;
	uint8_t minor;
};

/* Global variables */
unsigned long long read_limit;
long mmap_len;
uint8_t minor_num;
long ch0_error_cnt;
long ch1_error_cnt;
long total_samples;
int test_previous;
int is_ramp;

/* Prints the usage information */
void help()
{
	printf("OPERATIONAL MODES:\n\n 0 - Normal Rx\n 1 - Normal Tx*\n 2 - Internal loopback\n 3 - External loopback on master*\n"
	       " 4 - External loopback on master-slave*\n 5 - Set master/slave mode*\n 6 - Configure master clock*\n"
	       " 7 - Configure I2S params\n 8 - Configure PCM params\n 9 - Configure TDM params\n"
	       " 10 - Configure LPAIF mode\n 11 - Set PCM lane configuration\n 12 - Configure bit clock*\n"
		   " 13 - Configure DAB MRC mode**\n");
	printf("* Supported only on SA8155/SA8195\n");
	printf("** Supported only on SA6155\n\n");
	printf("USAGE:\n\n");
	printf("NORMAL Rx:\n");
	printf("hsi2s_test --op_mode=0 --dev=<> --output=<> --bit_clock_hz=<> --data_buffer_ms=<> [--dma_buffer_length=<>] [--set_cpu_affinity] [--ramp]\n\n");
	printf("NORMAL Tx:\n");
	printf("hsi2s_test --op_mode=1 --dev=<> --input=<>\n\n");
	printf("INTERNAL LOOPBACK:\n");
	printf("hsi2s_test --op_mode=2 --dev=<> --output=<> --input=<> [--dma_buffer_length=<>]\n\n");
	printf("EXTERNAL LOOPBACK ON MASTER:\n");
	printf("hsi2s_test --op_mode=3 --dev=<> --output=<> --input=<> [--dma_buffer_length=<>]\n\n");
	printf("EXTERNAL LOOPBACK BETWEEN MASTER AND SLAVE INTERFACES:\n");
	printf("hsi2s_test --op_mode=4 --dev=<> --s_dev=<> --output=<> --input=<> [--dma_buffer_length=<>]\n\n");
	printf("SET MASTER/SLAVE MODE:\n");
	printf("hsi2s_test --op_mode=5 --dev=<> --master/slave\n\n");
	printf("CONFIGURE MASTER CLOCK:\n");
	printf("hsi2s_test --op_mode=6 --dev=<> --clk_src=<> --divide_by=<>\n\n");
	printf("CONFIGURE I2S PARAMETERS:\n");
	printf("hsi2s_test --op_mode=7 --dev=<> --bit_clock_hz=<> --data_buffer_ms=<> --bit_depth=<> --spkr_ch=<> --mic_ch=<>  [--long_rate=<>]\n\n");
	printf("CONFIGURE PCM PARAMETERS:\n");
	printf("hsi2s_test --op_mode=8 --dev=<> --bit_clock_hz=<> --data_buffer_ms=<> --pcm_rate=<> --pcm_sync_int/pcm_sync_ext --pcm_sync_short/pcm_sync_long --rpcm_width=<> --tpcm_width=<>\n\n");
	printf("CONFIGURE TDM PARAMETERS:\n");
	printf("hsi2s_test --op_mode=9 --dev=<device file> --tdm_sync_delay=<> --tdm_tpcm_width=<> --tdm_rpcm_width=<> --tdm_rate=<> [--tdm_tpcm_sample_width=<> --tdm_rpcm_sample_width=<>]\n\n");
	printf("SET I2S/PCM MODE:\n");
	printf("hsi2s_test --op_mode=10 --dev=<> --i2s/pcm\n\n");
	printf("SET PCM LANE CONFIGURATION:\n");
	printf("hsi2s_test --op_mode=11 --dev=<> --lane_config=<>\n\n");
	printf("TOGGLE BIT CLOCK:\n");
	printf("hsi2s_test --op_mode=12 --dev=<>\n\n");
	printf("CONFIGURE DAB MRC MODE:\n");
	printf("hsi2s_test --op_mode=13 --output_a=<> --output_b=<> --bit_clock_hz=<> --data_buffer_ms=<> --target_type=<> [--dma_buffer_length=<>] [--set_cpu_affinity]\n\n");
	printf("OPTIONS:\n\n");
	printf("--dev \n\t Device file : /dev/hs0_i2s | /dev/hs1_i2s | /dev/hs2_i2s\n");
	printf("--s_dev \n\t Slave device file used in master-slave loopback: /dev/hs0_i2s | /dev/hs1_i2s | /dev/hs2_i2s\n");
	printf("--output \n\t Path to the output file, to store the data read from the HS-I2S interface\n");
	printf("--output_a \n\t Path to the output file, to store the data read from tuner A\n");
	printf("--output_b \n\t Path to the output file, to store the data read from tuner B\n");
	printf("--input \n\t Path to the input file, to fetch data written to the HS-I2S interface\n");
	printf("--dma_buffer_length \n\t DMA buffer length in MB (4MB by default) (Optional)\n");
	printf("--master \n\t HS-I2S master\n");
	printf("--slave \n\t HS-I2S slave\n");
	printf("--clk_src \n\t Clock source : 0 -> CXO(19.2 MHz) 1 -> DIGITAL PLL(122.88 MHz)\n");
	printf("--divide_by \n\t 0 -> Bypass, 1 -> Div-1, 2 -> Div-1.5, 3 -> Div-2, 4 -> Div-2.5, ..... 31 -> Div-16\n");
	printf("--bit_clock_hz \n\t Bit clock freqeuncy in Hertz\n");
	printf("--data_buffer_ms \n\t Periodic length of data buffer in milli seconds\n");
	printf("--bit_depth \n\t Bit depth in I2S mode : 16 | 24 | 25 | 32\n");
	printf("--spkr_ch \n\t Speaker channel count in I2S mode  : 1 | 2 | 4\n");
	printf("--mic_ch \n\t Mic channel count in I2S mode : 1 | 2 |4\n");
	printf("--long_rate \n\t New WS rate when long rate is enabled, allows WS rate to be larger than bit depth\n");
	printf("--pcm_rate \n\t Frame size : 0 -> 8 bits, 1 -> 16 bits, 2 -> 32 bits, 3 -> 64 bits, 4 -> 128 bits, 5 -> 256 bits\n");
	printf("--pcm_sync_ext \n\t External frame sync\n");
	printf("--pcm_sync_int \n\t Internal frame sync\n");
	printf("--pcm_sync_short \n\t Short frame sync (Pulse)\n");
	printf("--pcm_sync_long \n\t Long frame sync (50 percent duty cycle)\n");
	printf("--rpcm_width \n\t PCM receive slot size : 0 -> 8 bits, 1 -> 16 bits\n");
	printf("--tpcm_width \n\t PCM transmit slot size : 0 -> 8 bits, 1 -> 16 bits\n");
	printf("--tdm_sync_delay \n\t Data delay in bit cycles wrt start of frame sync : 0 -> 2 CYCLE DELAY, 1 -> 1 CYCLE DELAY, 2 -> 0 CYCLE DELAY\n");
	printf("--tdm_tpcm_width \n\t TDM transmit slot size in bits (maximum 32)\n");
	printf("--tdm_rpcm_width \n\t TDM receive slot size in bits (maximum 32)\n");
	printf("--tdm_rate \n\t TDM frame size in bits (maximum 512)\n");
	printf("--tdm_tpcm_sample_width \n\t TDM TPCM sample width in bits (maximum 32) (Optional)\n");
	printf("--tdm_rpcm_sample_width \n\t TDM RPCM sample width in bits (maximum 32) (Optional)\n");
	printf("--i2s \n\t LPAIF in HS-I2S mode\n");
	printf("--pcm \n\t LPAIF in HS-PCM mode\n");
	printf("--lane_config \n\t Data lane direction in PCM mode : 0 -> SINGLE LANE, 1 -> MULTI LANE RX, 2 -> MULTI LANE TX\n");
	printf("--set_cpu_affinity \n\t Set CPU affinity to one of the available high cores\n");
	printf("--target_type \n\t 0 -> 6155 1-> 8155/8195\n");
	printf("--ramp \n\t Trigger ramp analysis on output\n");
	printf("--normal_read \n\t Use normal read thread. By default, fast read thread is selected\n\n");
}

/* Returns the size of input file in bytes */
long get_size(FILE *fp)
{
	long n;

	/* Find end of file */
	fseek(fp, 0L, SEEK_END);

	/* Get current position */
	n = ftell(fp);

	/* Seek back */
	fseek(fp, 0L, SEEK_SET);

	/* Return the file size*/
	return n;
}

/* Function to calculate periodic interrupt length */
uint32_t get_periodic_length(uint32_t bit_clk, uint32_t interval)
{
	/*
	 * Formula to calculate
	 * Bit clock -> 'm' Hz
	 * Bits per sec  = m
	 * Bits per msec = m * (10^(-3))
	 * Bytes per msec = (m * (10^(-3))) / 8 = m / 8000
	 * Bytes per 'k' msec = k * (m / 8000)
	 */
	return (((unsigned long long)interval * bit_clk) / 8000);
}

/* Ramp analysis */
void bbiq_analyze(void *dataStart, long numBytes)
{
	int16_t* dataWordStart = dataStart;
	long numSamples = numBytes / 8;
	int16_t Ch0_I_cur = 0;
	int16_t Ch0_Q_cur = 0;
	int16_t Ch1_I_cur = 0;
	int16_t Ch1_Q_cur = 0;
	int16_t Ch0_I_prev = 0;
	int16_t Ch0_Q_prev = 0;
	int16_t Ch1_I_prev = 0;
	int16_t Ch1_Q_prev = 0;
	int16_t* tempCH0;
	int16_t* tempCH1;

	if (numBytes % 8 != 0)
	{
		printf("bbiq_analyze error.  Bad chunk size of data.  numBytes=%ld\n", numBytes);
	}

	for(long i = 1; i < numSamples; i++)
	{
		tempCH0 = (int16_t *)(&dataWordStart[4*i + 0]);
		tempCH1 = (int16_t *)(&dataWordStart[4*i + 2]);

		Ch0_I_cur = *(tempCH0);
		Ch0_Q_cur = *(tempCH0 + 1);
		Ch1_I_cur = *(tempCH1);
		Ch1_Q_cur = *(tempCH1 +1);

		Ch0_I_prev = *(tempCH0 - 4);
		Ch0_Q_prev = *(tempCH0 - 3);
		Ch1_I_prev = *(tempCH1 - 4);
		Ch1_Q_prev = *(tempCH1 - 3);

		/* Error criteria is step size over 2 */
		if ((BBIQ_ALLOWED_ERROR < abs(Ch0_I_prev-Ch0_I_cur)) || (BBIQ_ALLOWED_ERROR < abs(Ch0_Q_prev-Ch0_Q_cur))
				|| (1 < abs(Ch0_I_cur != Ch0_Q_cur)) )
		{
			ch0_error_cnt++;
		}

		/* Check for increment and  I & Q match */
		if ((BBIQ_ALLOWED_ERROR < abs(Ch1_I_prev-Ch1_I_cur)) || (BBIQ_ALLOWED_ERROR < abs(Ch1_Q_prev-Ch1_Q_cur))
				|| (1 < abs(Ch1_I_cur != Ch1_Q_cur)) )
		{
			ch1_error_cnt++;
		}
	}

	total_samples += numSamples;

	if (total_samples % 2048000 < test_previous)
	{
		printf("Testing BBIQ: CH0 errors = %8ld, CH1 errors=%8ld, Total Samples=%12ld\n",
				ch0_error_cnt , ch1_error_cnt, total_samples);
	}
	test_previous = total_samples % 2048000;
}

/* Read thread */
void *poll_read_fast(void *arg)
{
	unsigned long long r_limit = 0;
	long temp;
	int ret;
	double thread_start;
	double thread_stop;
	double delta;
	struct timespec pread_start;
	struct timespec pread_stop;
	struct pollfd pfd;
	void *mmap_ptr;
	void *mmap_read;
	void *mmap_end;
	void *sh_mem;
	FILE *fd_write_op;
	struct thread_params *params;
	uint32_t *shm;
	uint32_t base_addr_phy;
	uint32_t curr_addr_phy;
	void *prev_addr;
	void *curr_addr;
	long read_len;
	long read_len_blk;
	uint8_t minor;

	params = (struct thread_params *)arg;
	if (params) {
		pfd = params->pfd;
		mmap_ptr = params->mmap_ptr;
		mmap_read = params->mmap_read;
		mmap_end = params->mmap_end;
		sh_mem = params->sh_mem;
		shm = (uint32_t *)sh_mem;
		fd_write_op = params->fd_write_op;
		minor = params->minor;
	} else {
		printf("Thread parameters are null\n");
		return NULL;
	}

	printf("Performing poll wait for fast read...\n");

	clock_gettime(CLOCK_REALTIME, &pread_start);
	thread_start = (pread_start.tv_sec * BILLION) + pread_start.tv_nsec;
	printf("[POLL] Starttime %lf\n", thread_start);

	base_addr_phy = *((uint32_t *)(shm) + ((minor * PG_SIZE) / BYTES_PER_WORD) + SHM_WRDMA_BASE);
	prev_addr = mmap_ptr;

	while (r_limit < read_limit) {
		clock_gettime(CLOCK_REALTIME, &pread_start);
		ret = poll(&pfd, 1, -1);
		clock_gettime(CLOCK_REALTIME, &pread_stop);
		delta = ((pread_stop.tv_sec - pread_start.tv_sec) * BILLION) +
				(pread_stop.tv_nsec - pread_start.tv_nsec);
		if (!is_ramp)
			printf("[POLL] Data ready in %lf nsec\n", delta);
		if (ret < 0) {
			printf("Poll failed\n");
		} else if (pfd.revents & EPOLLIN) {
			curr_addr_phy = *((uint32_t *)(shm) + ((minor * PG_SIZE) / BYTES_PER_WORD) + SHM_WRDMA_CURRENT);
			curr_addr = mmap_ptr + (curr_addr_phy - base_addr_phy);

			if(prev_addr < curr_addr) {
				read_len = curr_addr - prev_addr;
				read_len_blk = mmap_len * (read_len/mmap_len);
				if (r_limit + read_len_blk > read_limit) {
					if (!is_ramp)
						fwrite(prev_addr,read_limit - r_limit,1,fd_write_op);
					else
						bbiq_analyze(prev_addr, read_limit - r_limit);
					prev_addr += (read_limit - r_limit);
					r_limit += (read_limit - r_limit);
				} else {
					if (!is_ramp)
						fwrite(prev_addr,read_len_blk,1,fd_write_op);
					else
						bbiq_analyze(prev_addr, read_len_blk);
					prev_addr += read_len_blk;
					r_limit += read_len_blk;
				}
			} else {
				read_len = (mmap_end - prev_addr) + (curr_addr - mmap_ptr);
				read_len_blk = mmap_len * (read_len/mmap_len);
				if (r_limit + read_len_blk > read_limit) {
					if (prev_addr + (read_limit - r_limit) > mmap_end) {
						temp = mmap_end - prev_addr;
						if (!is_ramp)
							fwrite(prev_addr,temp,1,fd_write_op);
						else
							bbiq_analyze(prev_addr, temp);
						temp = (read_limit - r_limit) - temp;
						if (!is_ramp)
							fwrite(mmap_ptr,temp,1,fd_write_op);
						else
							bbiq_analyze(mmap_ptr, temp);
						prev_addr = mmap_ptr + temp;
					} else {
						if (!is_ramp)
							fwrite(prev_addr,read_limit - r_limit,1,fd_write_op);
						else
							bbiq_analyze(prev_addr, read_limit - r_limit);
						prev_addr += (read_limit - r_limit);
					}
					r_limit += (read_limit - r_limit);
				} else {
					if (prev_addr + read_len_blk > mmap_end) {
						temp = mmap_end - prev_addr;
						if (!is_ramp)
							fwrite(prev_addr,temp,1,fd_write_op);
						else
							bbiq_analyze(prev_addr, temp);
						temp = read_len_blk - temp;
						if (!is_ramp)
							fwrite(mmap_ptr,temp,1,fd_write_op);
						else
							bbiq_analyze(mmap_ptr, temp);
						prev_addr = mmap_ptr + temp;
					} else {
						if (!is_ramp)
							fwrite(prev_addr,read_len_blk,1,fd_write_op);
						else
							bbiq_analyze(prev_addr, read_len_blk);
						prev_addr += read_len_blk;
					}
					r_limit += read_len_blk;
				}
			}

			if (prev_addr >= mmap_end)
				prev_addr = mmap_ptr;
		}
	}

	clock_gettime(CLOCK_REALTIME, &pread_stop);
	thread_stop = (pread_stop.tv_sec * BILLION) + pread_stop.tv_nsec;
	delta = (thread_stop - thread_start) / BILLION;
	printf("[POLL] Endtime %lf\n", thread_stop);
	printf("[POLL] Total thread execution time %lfs\n", delta);
	if (is_ramp) {
		printf("Finished testing BBIQ \n\tCH0 has %16ld errors (%5.2f%%) \n\tCH1 has %16ld errors (%5.2f%%) \n\tTotal Samples = %16ld\n"
			, ch0_error_cnt, 100* (float)ch0_error_cnt / ((float)total_samples)
			, ch1_error_cnt, 100* (float)ch1_error_cnt / ((float)total_samples)
			, total_samples);
	}

	return NULL;
}

void *poll_read_normal(void *arg)
{
	unsigned long long r_limit = 0;
	long temp;
	int ret;
	double thread_start;
	double thread_stop;
	double delta;
	struct timespec pread_start;
	struct timespec pread_stop;
	struct pollfd pfd;
	void *mmap_ptr;
	void *mmap_read;
	void *mmap_end;
	FILE *fd_write_op;
	struct thread_params *params;
	int i;

	params = (struct thread_params *)arg;
	if (params) {
		pfd = params->pfd;
		mmap_ptr = params->mmap_ptr;
		mmap_read = params->mmap_read;
		mmap_end = params->mmap_end;
		fd_write_op = params->fd_write_op;
	} else {
		printf("Thread parameters are null\n");
		return NULL;
	}

	printf("Performing poll wait for normal read...\n");

	clock_gettime(CLOCK_REALTIME, &pread_start);
	thread_start = (pread_start.tv_sec * BILLION) + pread_start.tv_nsec;
	printf("[POLL] Starttime %lf\n", thread_start);

	while (r_limit < read_limit) {
		clock_gettime(CLOCK_REALTIME, &pread_start);
		ret = poll(&pfd, 1, -1);
		clock_gettime(CLOCK_REALTIME, &pread_stop);
		delta = ((pread_stop.tv_sec - pread_start.tv_sec) * BILLION) +
				(pread_stop.tv_nsec - pread_start.tv_nsec);
		if (!is_ramp)
			printf("[POLL] Data ready in %lf nsec\n", delta);
		if (ret < 0) {
			printf("Poll failed\n");
		} else if (pfd.revents & EPOLLIN) {
			if (r_limit + mmap_len > read_limit) {
				if (mmap_read + (read_limit - r_limit) > mmap_end) {
					temp = mmap_end - mmap_read;
					if (!is_ramp)
						fwrite(mmap_read,temp,1,fd_write_op);
					else
						bbiq_analyze(mmap_read,temp);
					temp = (read_limit - r_limit) - temp;
					if (!is_ramp)
						fwrite(mmap_ptr,temp,1,fd_write_op);
					else
						bbiq_analyze(mmap_ptr,temp);
					mmap_read = mmap_ptr + temp;

				} else {
					if (!is_ramp)
						fwrite(mmap_read,read_limit - r_limit,1,fd_write_op);
					else
						bbiq_analyze(mmap_read,read_limit - r_limit);
					mmap_read += (read_limit - r_limit);
				}
				r_limit += (read_limit - r_limit);
			} else {
				if (mmap_read + mmap_len > mmap_end) {
					temp = mmap_end - mmap_read;
					if (!is_ramp)
						fwrite(mmap_read,temp,1,fd_write_op);
					else
						bbiq_analyze(mmap_read,temp);
					temp = mmap_len - temp;
					if (!is_ramp)
						fwrite(mmap_ptr,temp,1,fd_write_op);
					else
						bbiq_analyze(mmap_ptr,temp);
					mmap_read = mmap_ptr + temp;
				} else {
					if (!is_ramp)
						fwrite(mmap_read,mmap_len,1,fd_write_op);
					else
						bbiq_analyze(mmap_read,mmap_len);
					mmap_read += mmap_len;
				}
				r_limit += mmap_len;
			}
			if (mmap_read >= mmap_end)
				mmap_read = mmap_ptr;
		}
	}

	clock_gettime(CLOCK_REALTIME, &pread_stop);
	thread_stop = (pread_stop.tv_sec * BILLION) + pread_stop.tv_nsec;
	delta = (thread_stop - thread_start) / BILLION;
	printf("[POLL] Endtime %lf\n", thread_stop);
	printf("[POLL] Total thread execution time %lfs\n", delta);
	if (is_ramp) {
		printf("Finished testing BBIQ \n\tCH0 has %16ld errors (%5.2f%%) \n\tCH1 has %16ld errors (%5.2f%%) \n\tTotal Samples = %16ld\n"
			, ch0_error_cnt, 100* (float)ch0_error_cnt / ((float)total_samples)
			, ch1_error_cnt, 100* (float)ch1_error_cnt / ((float)total_samples)
			, total_samples);
	}

	return NULL;
}

int main(int argc, char **argv)
{
	int fd_master = -1;
	int fd_slave = -1;
	int fd_core = -1;
	FILE *fd_read_ip = NULL;
	FILE *fd_write_op = NULL;
	long read_length_bytes = 0;
	long read_length_words = 0;
	enum operation_mode mode = 0;
	struct i2s_params *i_params = NULL;
	struct pcm_params *p_params = NULL;
	struct tdm_params *t_params = NULL;
	long wav_samples = 0;
	long no_words = 0;
	long w_len;
	int32_t *wav_data = NULL;
	int32_t temp_data;
	int i;
	int slave = 0;
	uint32_t reg_val;
	pthread_t tid;
	uint8_t mux_mode = 0;
	uint8_t clk_source = 0;
	uint32_t divide_by = 0;
	uint32_t bit_clk = 0;
	uint32_t buffer_ms = 10;
	uint32_t bit_depth = 0;
	uint8_t spkr_channel_count = 0;
	uint8_t mic_channel_count = 0;
	uint32_t long_rate = 0;
	uint8_t pcm_rate = 0;
	uint8_t pcm_sync_src = 1;
	uint8_t pcm_aux_mode = 0;
	uint8_t pcm_tpcm_width = 0;
	uint8_t pcm_rpcm_width = 0;
	uint8_t tdm_sync_delay = 1;
	uint32_t tdm_tpcm_width = 0;
	uint32_t tdm_rpcm_width = 0;
	uint32_t tdm_rate = 0;
	uint32_t tdm_tpcm_sample_width = 0;
	uint32_t tdm_rpcm_sample_width = 0;
	uint8_t lpaif_mode = 0;
	uint8_t lane_config = 0;
	uint8_t set_affinity = 0;
	int ret = 0;
	int opt;
	const char *short_opt = ":a:b:c:d:e:f:g:h:ijk:l:m:n:o:p:qrstu:v:w:x:y:z:A:B:CDE:FGH:I:J:KLM";
	cpu_set_t cpuset;
	uint8_t target_type = 0;
	char *hs_dev[DAB_TUNER_COUNT] = {"/dev/hs0_i2s","/dev/hs1_i2s"};
	int fd_dab[DAB_TUNER_COUNT] = {-1, -1};
	pthread_t tid_dab[DAB_TUNER_COUNT];
	struct thread_params *dab_params[DAB_TUNER_COUNT] = {NULL};
	FILE *fd_out_a = NULL;
	FILE *fd_out_b = NULL;
	struct thread_params *params;
	int use_normal_read = 0;

	struct option   long_opt[] =
	{
		{"op_mode", required_argument, NULL, 'a'},
		{"dev", required_argument, NULL, 'b'},
		{"s_dev", required_argument, NULL, 'c'},
		{"output", required_argument, NULL, 'd'},
		{"input", required_argument, NULL, 'e'},
		{"bit_clock_hz", required_argument, NULL, 'f'},
		{"data_buffer_ms", required_argument, NULL, 'g'},
		{"dma_buffer_length", required_argument, NULL, 'h'},
		{"master", no_argument, NULL, 'i'},
		{"slave", no_argument, NULL, 'j'},
		{"clk_src", required_argument, NULL, 'k'},
		{"divide_by", required_argument, NULL, 'l'},
		{"bit_depth", required_argument, NULL, 'm'},
		{"spkr_ch", required_argument, NULL, 'n'},
		{"mic_ch", required_argument, NULL, 'o'},
		{"long_rate", required_argument, NULL, 'p'},
		{"pcm_rate", required_argument, NULL, 'q'},
		{"pcm_sync_int", no_argument, NULL, 'r'},
		{"pcm_sync_ext", no_argument, NULL, 's'},
		{"pcm_sync_long", no_argument, NULL, 't'},
		{"pcm_sync_short", no_argument, NULL, 'u'},
		{"tpcm_width", required_argument, NULL, 'v'},
		{"rpcm_width", required_argument, NULL, 'w'},
		{"tdm_sync_delay", required_argument, NULL, 'x'},
		{"tdm_tpcm_width", required_argument, NULL, 'y'},
		{"tdm_rpcm_width", required_argument, NULL, 'z'},
		{"tdm_rate", required_argument, NULL, 'A'},
		{"tdm_tpcm_sample_width", required_argument, NULL, 'B'},
		{"tdm_rpcm_sample_width", required_argument, NULL, 'C'},
		{"i2s", no_argument, NULL, 'D'},
		{"pcm", no_argument, NULL, 'E'},
		{"lane_config", required_argument, NULL, 'F'},
		{"help", no_argument, NULL, 'G'},
		{"set_cpu_affinity", no_argument, NULL, 'H'},
		{"target_type", required_argument, NULL, 'I'},
		{"output_a", required_argument, NULL, 'J'},
		{"output_b", required_argument, NULL, 'K'},
		{"ramp", no_argument, NULL, 'L'},
		{"normal_read", no_argument, NULL, 'M'},
		{NULL, 0, NULL, 0}
	};

	/* Initializing to default macros and use the macros if no size specified by user */
	read_length_bytes = READ_LENGTH_MB * 1024 * 1024;
	read_length_words = READ_LENGTH_WORDS;
	mmap_len = read_length_bytes;

	/*Allocate thread params structure */
	params = (struct thread_params *) malloc(sizeof(struct thread_params));
	if (!params) {
		printf("Unable to allocate thread parameter structure\n");
	}

	while((opt = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)
	{
		switch(opt)
		{
			case 'a':
				/* Operational mode */
				mode = atoi(optarg);
				if (mode > CONFIG_DAB_MRC) {
					printf("Undefined mode\n");
					help();
					ret = -1;
					goto exit_app;
				}
				break;
			case 'b':
				/* Device file */
				if(fd_master < 0) {
					fd_master = open(optarg, O_RDWR);
					if(fd_master < 0) {
						printf("Cannot open device file\n");
						help();
						ret = -1;
						goto exit_app;
					}
					minor_num = optarg[7] - '0';
				}
				break;
			case 'c':
				/* Slave device file */
				if(fd_slave < 0) {
					fd_slave = open(optarg, O_RDWR);
					if(fd_slave < 0) {
						printf("Cannot open device file\n");
						help();
						ret = -1;
						goto exit_app;
					}
					slave = optarg[7] - '0';
				}
				break;
			case 'd':
				/* Output file */
				if (params) {
					if (fd_write_op == NULL) {
						fd_write_op = fopen(optarg, "w");
						if (fd_write_op == NULL) {
							printf("Cannot open output file\n");
							help();
							ret = -1;
							goto exit_app;
						} else {
							params->fd_write_op = fd_write_op;
						}
					}
				} else {
					printf("Thread parameter structure is null\n");
					ret = -1;
					goto exit_app;
				}
				break;
			case 'e':
				/* Input file */
				if (fd_read_ip == NULL) {
					fd_read_ip = fopen(optarg, "r");
					if (fd_read_ip == NULL) {
						printf("Cannot open input file\n");
						help();
						ret = -1;
						goto exit_app;
					}
					printf("Calculating i/p file size...\n");
					wav_samples = get_size(fd_read_ip);
					no_words = wav_samples/BYTES_PER_WORD;
					printf("Input file size in bytes: %ld\n", wav_samples);
					read_limit = wav_samples;
				}
				break;
			case 'f':
				/* Bit clock rate */
				bit_clk = atoi(optarg);
				break;
			case 'g':
				/* Interrupt interval */
				buffer_ms = atoi(optarg);
				break;
			case 'h':
				/* DMA buffer length */
				read_length_bytes = (atoi(optarg) * 1024 * 1024) / 2;
				read_length_words = read_length_bytes/BYTES_PER_WORD;
				mmap_len = read_length_bytes;
				break;
			case 'i':
				/* Master mode */
				mux_mode = 0;
				break;
			case 'j':
				/* Slave mode */
				mux_mode = 1;
				break;
			case 'k':
				/* Clock source */
				clk_source = atoi(optarg);
				if (clk_source > 1) {
					printf("Undefined source\n");
					help();
					ret = -1;
					goto exit_app;
				}
				break;
			case 'l':
				/* Clock division factor */
				divide_by = atoi(optarg);
				if (divide_by > 31) {
					printf("Undefined division\n");
					help();
					ret = -1;
					goto exit_app;
				}
				break;
			case 'm':
				/* Bit depth */
				bit_depth = atoi(optarg);
				break;
			case 'n':
				/* Speaker channel count */
				spkr_channel_count = atoi(optarg);
				break;
			case 'o':
				/* Mic channel count */
				mic_channel_count = atoi(optarg);
				break;
			case 'p':
				/* Long rate */
				long_rate = atoi(optarg);
				break;
			case 'q':
				/* PCM frame size */
				pcm_rate = atoi(optarg);
				break;
			case 'r':
				/* PCM internal frame sync */
				pcm_sync_src = 1;
				break;
			case 's':
				/* PCM external frame sync */
				pcm_sync_src = 0;
				break;
			case 't':
				/* PCM long frame sync */
				pcm_aux_mode = 1;
				break;
			case 'u':
				/* PCM short frame sync */
				pcm_aux_mode = 0;
				break;
			case 'v':
				/* PCM Tx slot size */
				pcm_tpcm_width = atoi(optarg);
				break;
			case 'w':
				/* PCM Rx slot size */
				pcm_rpcm_width = atoi(optarg);
				break;
			case 'x':
				/* TDM sync delay */
				tdm_sync_delay = atoi(optarg);
				break;
			case 'y':
				/* TDM Tx slot size */
				tdm_tpcm_width = atoi(optarg);
				break;
			case 'z':
				/* TDM Rx slot size */
				tdm_rpcm_width = atoi(optarg);
				break;
			case 'A':
				/* TDM frame size */
				tdm_rate = atoi(optarg);
				break;
			case 'B':
				/* TDM Tx sample width */
				tdm_tpcm_sample_width = atoi(optarg);
				break;
			case 'C':
				/* TDM Rx sample width */
				tdm_rpcm_sample_width = atoi(optarg);
				break;
			case 'D':
				/* HS-I2S mode */
				lpaif_mode = 0;
				break;
			case 'E':
				/* HS-PCM mode */
				lpaif_mode = 1;
				break;
			case 'F':
				/* PCM data lane configuration */
				lane_config = atoi(optarg);
				break;
			case 'G':
				/* Print usage */
				help();
				goto exit_app;
			case 'H':
				/* Set CPU affinity */
				set_affinity = 1;
				break;
			case 'I':
				/* Check target type */
				target_type = atoi(optarg);
				break;
			case 'J':
				/* Output file a */
				if (fd_out_a == NULL) {
					fd_out_a = fopen(optarg, "w");
					if (fd_out_a == NULL) {
						printf("Cannot open output file a\n");
						help();
						ret = -1;
						goto exit_app;
					}
				}
				break;
			case 'K':
				/* Output file b */
				if (fd_out_b == NULL) {
					fd_out_b = fopen(optarg, "w");
					if (fd_out_b == NULL) {
						printf("Cannot open output file b\n");
						help();
						ret = -1;
						goto exit_app;
					}
				}
				break;
			case 'L':
				/* Trigger ramp analysis */
				is_ramp = 1;
				break;
			case 'M':
				/* Use normal read */
				use_normal_read = 1;
				break;
			case ':':
				/* Value missing for option */
				printf("Option needs a value. Check --help for usage.\n");
				goto exit_app;
			case '?':
				/* Unknown option */
				printf("Unknown option entered. Check --help for usage.\n");
				break;
			default:
				break;
		}
	}

	/* optind is for the extra arguments which are not parsed */
	for(; optind < argc; optind++) {
		printf("Extra arguments: %s\n", argv[optind]);
	}

	switch (mode)
	{
		case NORMAL_RX:
			/* Operation mode : Normal mode data reception */
			if (argc < 6) {
				help();
				ret = -1;
				break;
			}
			/* Check for valid inputs */
			if (!params) {
				printf("Thread params structure is null, exiting...\n");
				ret = -1;
				break;
			}
			/* Set CPU affinity to one of the available high cores */
			if (set_affinity) {
				printf("Setting CPU affinity \n");
				CPU_ZERO(&cpuset);
				for (i = 6; i < 8; i++) {
					CPU_SET(i, &cpuset);
					ret = sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
					if (ret) {
						printf("Cannot set CPU affinity on CPU[%d]\n", i);
						CPU_ZERO(&cpuset);
						continue;
					}
					printf("CPU affinity set on CPU[%d]\n", i);
					break;
				}
			}
			/* Device file */
			fd_core = open("/dev/hsi2s_reginfo", O_RDWR);
			if(fd_core < 0) {
				printf("Cannot open core device file\n");
				ret = -1;
				break;
			}
			printf("Setting normal mode \n");
			ret = ioctl(fd_master, LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s device\n");
				break;
			}
			ret = ioctl(fd_master, LPAIF_NORMAL_MODE);
			if (ret < 0) {
				printf("Failed to configure normal operation on the hsi2s device\n");
				break;
			}

			read_limit = READ_LIMIT;
			mmap_len = get_periodic_length(bit_clk, buffer_ms);
			printf("Periodic length set to %ld bytes\n", mmap_len);

			/* Map the device write DMA buffer */
			params->pfd.fd = fd_master;
			params->pfd.events = EPOLLIN | EPOLLRDNORM;
			printf("Mapping userspace memory with kernel memory for device\n");
			params->mmap_ptr = mmap(NULL, read_length_bytes * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd_master, 0);
			if (params->mmap_ptr == MAP_FAILED) {
				printf("mmap failed for device\n");
				ret = -1;
				break;
			} else {
				params->mmap_read = params->mmap_ptr;
				params->mmap_end = params->mmap_ptr + (read_length_bytes * 2);
			}
			/* Map the register info memory */
			printf("Mapping userspace memory with kernel memory for core\n");
			params->sh_mem = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_core, 0);
			if (params->sh_mem == MAP_FAILED) {
				printf("mmap failed for core\n");
				ret = -1;
				break;
			}
			params->minor = minor_num;

			printf("Using mmap mode...\n");

			/* Create thread to read the received data */
			if (use_normal_read) {
				ret = pthread_create(&tid, NULL, poll_read_normal, params);
			} else {
				ret = pthread_create(&tid, NULL, poll_read_fast, params);
			}
			if (ret) {
				printf("Error creating poll thread\n");
				break;
			}

			printf("Joining threads\n");
			pthread_join(tid,NULL);
			printf("Threads joined \n");
			break;

		case NORMAL_TX:
			/* Operation mode : Normal Tx */
			if (argc < 4) {
				help();
				ret = -1;
				break;
			}
			/* Check for valid inputs */
			if (!fd_read_ip) {
				printf("Unable to open input file, exiting...\n");
				ret = -1;
				break;
			}
			printf("Setting Tx on master\n");
			ret = ioctl(fd_master, LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s device\n");
				break;
			}
			ret = ioctl(fd_master, LPAIF_MUXMODE, 0);
			if (ret < 0) {
				printf("Failed to set master mode\n");
				break;
			}
			ret = ioctl(fd_master, LPAIF_SPEAKER);
			if (ret < 0) {
				printf("Failed to configure speaker\n");
				break;
			}

			printf("Reading i/p file...\n");
			wav_data = (int32_t *) malloc(no_words * sizeof(int32_t));
			if (!wav_data) {
				printf("Failed to allocate transmit data buffer\n");
				ret = -ENOMEM;
				break;
			}

			/* Copy data from wav file into memory */
			for (i = 0; i < no_words; i++) {
				fread(&temp_data, BYTES_PER_WORD, 1,fd_read_ip);
				*(wav_data + i) = (int32_t)temp_data;
			}

			w_len = wav_samples;

			ret = write(fd_master, wav_data, wav_samples);
			if (ret < 0) {
				printf("Failed to write to the hsi2s device. Check if write length exceeds DMA limit of 4MB\n");
				break;
			}

			/* Disable transmission */
			printf("Disabling Tx on read DMA channel\n");
			ret = ioctl(fd_master, LPAIF_DEINIT_TX);
			if (ret < 0) {
				printf("Failed to stop Tx on hsi2s device\n");
				break;
			}
			break;

		case INTERNAL_LB:
			/* Operation mode : Internal loopback */
			if (argc < 5) {
				help();
				ret = -1;
				break;
			}
			/* Check for valid inputs */
			if (!params) {
				printf("Thread params structure is null, exiting...\n");
				ret = -1;
				break;
			}
			if (!fd_read_ip) {
				printf("Unable to open input file, exiting...\n");
				ret = -1;
				break;
			}
			/* Device file */
			fd_core = open("/dev/hsi2s_reginfo", O_RDWR);
			if(fd_core < 0) {
				printf("Cannot open core device file\n");
				ret = -1;
				break;
			}
			printf("Setting internal loopback operation \n");
			ret = ioctl(fd_master, LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s device\n");
				break;
			}
			ret = ioctl(fd_master, LPAIF_INTERNAL_LOOPBACK);
			if (ret < 0) {
				printf("Failed to trigger internal loopback\n");
				break;
			}
			/* Map the device write DMA buffer */
			params->pfd.fd = fd_master;
			params->pfd.events = EPOLLIN | EPOLLRDNORM;
			printf("Mapping userspace memory with kernel memory for device\n");
			params->mmap_ptr = mmap(NULL, read_length_bytes * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd_master, 0);
			if (params->mmap_ptr == MAP_FAILED) {
				printf("mmap failed for device\n");
				ret = -1;
				break;
			} else {
				params->mmap_read = params->mmap_ptr;
				params->mmap_end = params->mmap_ptr + (read_length_bytes * 2);
			}
			/* Map the register info memory */
			printf("Mapping userspace memory with kernel memory for core\n");
			params->sh_mem = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_core, 0);
			if (params->sh_mem == MAP_FAILED) {
				printf("mmap failed for core\n");
				ret = -1;
				break;
			}
			params->minor = minor_num;

			printf("Using mmap mode...\n");

			/* Create thread to read the received data */
			if (use_normal_read) {
				ret = pthread_create(&tid, NULL, poll_read_normal, params);
			} else {
				ret = pthread_create(&tid, NULL, poll_read_fast, params);
			}
			if (ret) {
				printf("Error creating poll thread\n");
				break;
			}

			printf("Reading i/p file...\n");
			wav_data = (int32_t *) malloc(no_words * sizeof(int32_t));
			if (!wav_data) {
				printf("Failed to allocate transmit data buffer\n");
				ret = -ENOMEM;
				break;
			}

			/* Copy data from wav file into memory */
			for (i = 0; i < no_words; i++) {
				fread(&temp_data, BYTES_PER_WORD, 1,fd_read_ip);
				*(wav_data + i) = (int32_t)temp_data;
			}

			w_len = wav_samples;

			ret = write(fd_master, wav_data, wav_samples);
			if (ret < 0) {
				printf("Failed to write to the hsi2s device. Check if write length exceeds DMA limit of 4MB\n");
				break;
			}

			printf("Joining threads\n");
			pthread_join(tid,NULL);
			printf("Threads joined \n");

			/* Disable transmission */
			printf("Disabling Tx on read DMA channel\n");
			ret = ioctl(fd_master, LPAIF_DEINIT_TX);
			if (ret < 0) {
				printf("Failed to stop Tx on hsi2s device\n");
				break;
			}
			break;

		case EXTERNAL_LB_MASTER:
			/* Operation mode : External loopback on master interface */
			if (argc < 5) {
				help();
				ret = -1;
				break;
			}
			/* Check for valid inputs */
			if (!params) {
				printf("Thread params structure is null, exiting...\n");
				ret = -1;
				break;
			}
			if (!fd_read_ip) {
				printf("Unable to open input file, exiting...\n");
				ret = -1;
				break;
			}
			/* Device file */
			fd_core = open("/dev/hsi2s_reginfo", O_RDWR);
			if(fd_core < 0) {
				printf("Cannot open core device file\n");
				ret = -1;
				break;
			}
			printf("Setting external loopback on master \n");
			ret = ioctl(fd_master, LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s device\n");
				break;
			}
			ret = ioctl(fd_master, LPAIF_EXTERNAL_LOOPBACK);
			if (ret < 0) {
				printf("Failed to trigger external loopback\n");
				break;
			}

			/* Map the device write DMA buffer */
			params->pfd.fd = fd_master;
			params->pfd.events = EPOLLIN | EPOLLRDNORM;
			printf("Mapping userspace memory with kernel memory for device\n");
			params->mmap_ptr = mmap(NULL, read_length_bytes * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd_master, 0);
			if (params->mmap_ptr == MAP_FAILED) {
				printf("mmap failed for device\n");
				ret = -1;
				break;
			} else {
				params->mmap_read = params->mmap_ptr;
				params->mmap_end = params->mmap_ptr + (read_length_bytes * 2);
			}
			/* Map the register info memory */
			printf("Mapping userspace memory with kernel memory for core\n");
			params->sh_mem = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_core, 0);
			if (params->sh_mem == MAP_FAILED) {
				printf("mmap failed for core\n");
				ret = -1;
				break;
			}
			params->minor = minor_num;

			printf("Using mmap mode...\n");

			/* Create thread to read the received data */
			if (use_normal_read) {
				ret = pthread_create(&tid, NULL, poll_read_normal, params);
			} else {
				ret = pthread_create(&tid, NULL, poll_read_fast, params);
			}
			if (ret) {
				printf("Error creating poll thread\n");
				break;
			}

			printf("Reading i/p file...\n");
			wav_data = (int32_t *) malloc(no_words * sizeof(int32_t));
			if (!wav_data) {
				printf("Failed to allocate transmit data buffer\n");
				ret = -ENOMEM;
				break;
			}

			/* Copy data from wav file into memory */
			for (i = 0; i < no_words; i++) {
				fread(&temp_data, BYTES_PER_WORD, 1,fd_read_ip);
				*(wav_data + i) = (int32_t)temp_data;
			}

			w_len = wav_samples;

			ret = write(fd_master, wav_data, wav_samples);
			if (ret < 0) {
				printf("Failed to write to the hsi2s device. Check if write length exceeds DMA limit of 4MB\n");
				break;
			}

			printf("Joining threads\n");
			pthread_join(tid,NULL);
			printf("Threads joined \n");

			/* Disable transmission */
			printf("Disabling Tx on read DMA channel\n");
			ret = ioctl(fd_master, LPAIF_DEINIT_TX);
			if (ret < 0) {
				printf("Failed to stop Tx on hsi2s device\n");
				break;
			}
			break;

		case EXTERNAL_LB_MASTER_SLAVE:
			/* Operation mode : External loopback between master and slave interfaces */
			if (argc < 6) {
				help();
				ret = -1;
				break;
			}
			/* Check for valid inputs */
			if (!params) {
				printf("Thread params structure is null, exiting...\n");
				ret = -1;
				break;
			}
			if (!fd_read_ip) {
				printf("Unable to open input file, exiting...\n");
				ret = -1;
				break;
			}
			printf("Slave node is hs%d_i2s\n",slave);
			/* Device file */
			fd_core = open("/dev/hsi2s_reginfo", O_RDWR);
			if(fd_core < 0) {
				printf("Cannot open core device file\n");
				ret = -1;
				break;
			}
			printf("Setting external loopback on master/slave \n");
			ret = ioctl(fd_master, LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s master\n");
				break;
			}
			ret = ioctl(fd_slave, LPAIF_RESET);
			if (ret < 0) {
				printf("Failed to reset the hsi2s slave\n");
				break;
			}
			ret = ioctl(fd_master, LPAIF_MUXMODE, 0);
			if (ret < 0) {
				printf("Failed to set master mode\n");
				break;
			}
			ret = ioctl(fd_slave, LPAIF_MUXMODE, 1);
			if (ret < 0) {
				printf("Failed to set slave mode\n");
				break;
			}
			ret = ioctl(fd_master, LPAIF_SET_SLAVE, slave);
			if (ret < 0) {
				printf("Failed to set slave for the master\n");
				break;
			}
			ret = ioctl(fd_slave, LPAIF_MIC);
			if (ret < 0) {
				printf("Failed to configure mic\n");
				break;
			}
			ret = ioctl(fd_master, LPAIF_SPEAKER);
			if (ret < 0) {
				printf("Failed to configure speaker\n");
				break;
			}

			/* Set the minor number */
			minor_num = slave;

			/* Map the slave device write DMA buffer */
			params->pfd.fd = fd_slave;
			params->pfd.events = EPOLLIN | EPOLLRDNORM;
			printf("Mapping userspace memory with kernel memory for device\n");
			params->mmap_ptr = mmap(NULL, read_length_bytes * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd_slave, 0);
			if (params->mmap_ptr == MAP_FAILED) {
				printf("mmap failed for device\n");
				ret = -1;
				break;
			} else {
				params->mmap_read = params->mmap_ptr;
				params->mmap_end = params->mmap_ptr + (read_length_bytes * 2);
			}
			/* Map the register info memory */
			printf("Mapping userspace memory with kernel memory for core\n");
			params->sh_mem = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_core, 0);
			if (params->sh_mem == MAP_FAILED) {
				printf("mmap failed for core\n");
				ret = -1;
				break;
			}
			params->minor = minor_num;

			printf("Using mmap mode...\n");

			/* Create thread to read the received data */
			if (use_normal_read) {
				ret = pthread_create(&tid, NULL, poll_read_normal, params);
			} else {
				ret = pthread_create(&tid, NULL, poll_read_fast, params);
			}
			if (ret) {
				printf("Error creating poll thread\n");
				break;
			}

			printf("Reading i/p file...\n");
			wav_data = (int32_t *) malloc(no_words * sizeof(int32_t));
			if (!wav_data) {
				printf("Failed to allocate transmit data buffer\n");
				ret = -ENOMEM;
				break;
			}

			/* Copy data from wav file into memory */
			for (i = 0; i < no_words; i++) {
				fread(&temp_data, BYTES_PER_WORD, 1,fd_read_ip);
				*(wav_data + i) = (int32_t)temp_data;
			}

			w_len = wav_samples;

			ret = write(fd_master, wav_data, wav_samples);
			if (ret < 0) {
				printf("Failed to write to the hsi2s device. Check if write length exceeds DMA limit of 4MB\n");
				break;
			}

			printf("Joining threads\n");
			pthread_join(tid,NULL);
			printf("Threads joined \n");

			/* Disable transmission */
			printf("Disabling Tx on read DMA channel\n");
			ret = ioctl(fd_master, LPAIF_DEINIT_TX);
			if (ret < 0) {
				printf("Failed to stop Tx on hsi2s device\n");
				break;
			}
			break;

		case SET_MUXMODE:
			/* Operation mode : Set I2S interface as master/slave */
			if (argc < 4) {
				help();
				ret = -1;
				break;
			}

			printf("Setting master/slave mode...\n");
			ret = ioctl(fd_master, LPAIF_MUXMODE, mux_mode);
			if (ret < 0) {
				printf("Failed to set master/slave configuration on target\n");
			}
			break;
		case CONFIG_M_CLK:
			/* Operation mode : Configure master clock */
			if (argc < 5) {
				help();
				ret = -1;
				break;
			}
			if (!clk_source) {
				printf("Clock source is CXO\n");
				reg_val = divide_by;
				ret = ioctl(fd_master, LPAIF_SET_CLOCK, reg_val);
				if (ret < 0) {
					printf("Failed to set master clock on target\n");
					break;
				}
			} else {
				printf("Clock source is Digital PLL\n");
				reg_val = SRC_DIGITAL_PLL | divide_by;
				ret = ioctl(fd_master, LPAIF_SET_CLOCK, reg_val);
				if (ret < 0) {
					printf("Failed to set master clock on target\n");
					break;
				}
			}
			printf("Successfully configured master clock\n\n");
			break;
		case CONFIG_I2S_PARAMS:
			/* Operation mode : Configure I2S parameters */
			if (argc < 8) {
				help();
				ret = -1;
				break;
			}
			printf("Configuring I2S parameters...\n");
			i_params = (struct i2s_params *) malloc(sizeof(struct i2s_params));
			if (!i_params) {
				printf("Failed to allocate I2S param structure\n");
				ret = -ENOMEM;
				break;
			}
			i_params->bit_clk = bit_clk;
			i_params->buffer_ms = buffer_ms;
			i_params->bit_depth = bit_depth;
			i_params->spkr_channel_count = spkr_channel_count;
			i_params->mic_channel_count = mic_channel_count;
			if (long_rate) {
				i_params->en_long_rate = 1;
				i_params->long_rate = long_rate;
			}
			ret = ioctl(fd_master, I2S_CONFIG_PARAMS, i_params);
			if (ret < 0) {
				printf("Failed to configure I2S parameters on target\n");
			}
			free(i_params);
			break;
		case CONFIG_PCM_PARAMS:
			/* Operation mode : Configure PCM parameters */
			printf("Configuring PCM parameters...\n");
			if (argc < 10) {
				help();
				ret = -1;
				break;
			}
			p_params = (struct pcm_params *) malloc(sizeof(struct pcm_params));
			if (!p_params) {
				printf("Failed to allocate PCM param structure\n");
				ret = -ENOMEM;
				break;
			}
			p_params->bit_clk = bit_clk;
			p_params->buffer_ms = buffer_ms;
			p_params->rate = pcm_rate;
			p_params->sync_src = pcm_sync_src;
			p_params->aux_mode = pcm_aux_mode;
			p_params->rpcm_width = pcm_rpcm_width;
			p_params->tpcm_width = pcm_tpcm_width;
			ret = ioctl(fd_master, PCM_CONFIG_PARAMS, p_params);
			if (ret < 0) {
				printf("Failed to configure PCM parameters on target\n");
			}
			free(p_params);
			break;
		case CONFIG_TDM_PARAMS:
			/* Operation mode : Configure TDM parameters */
			printf("Configuring TDM parameters...\n");
			if (argc < 7) {
				help();
				ret = -1;
				break;
			}
			t_params = (struct tdm_params *) malloc(sizeof(struct tdm_params));
			if (!t_params) {
				printf("Failed to allocate TDM param structure\n");
				ret = -ENOMEM;
				break;
			}
			t_params->sync_delay = tdm_sync_delay;
			t_params->tpcm_width = tdm_tpcm_width;
			t_params->rpcm_width = tdm_rpcm_width;
			t_params->rate = tdm_rate;
			if (tdm_tpcm_sample_width || tdm_rpcm_sample_width) {
				t_params->en_diff_sample_width = 1;
				t_params->tpcm_sample_width = tdm_tpcm_sample_width;
				t_params->rpcm_sample_width = tdm_rpcm_sample_width;
			}
			ret = ioctl(fd_master, TDM_CONFIG_PARAMS, t_params);
			if (ret < 0) {
				printf("Failed to configure TDM parameters on target\n");
			}
			free(t_params);
			break;
		case CONFIG_LPAIF_MODE:
			/* Operation mode : Set LPAIF interface in I2S/PCM mode */
			if (argc < 4) {
				help();
				ret = -1;
				break;
			}
			printf("Setting LPAIF mode...\n");
			ret = ioctl(fd_master, LPAIF_MODE, lpaif_mode);
			if (ret < 0) {
				printf("Failed to set I2S/PCM configuration on target\n");
			}
			break;
		case CONFIG_PCM_LANE:
			/* Operation mode : Set PCM lane configuration */
			if (argc < 4) {
				help();
				ret = -1;
				break;
			}
			printf("Setting PCM lane configuration...\n");
			ret = ioctl(fd_master, PCM_CONFIG_LANE, lane_config);
			if (ret < 0) {
				printf("Failed to set PCM lane configuration on target\n");
			}
			break;
		case TOGGLE_BIT_CLK:
			/* Operation mode : Toggle bit clock */
			if (argc < 3) {
				help();
				ret = -1;
				break;
			}
			printf("Toggling bit clock direction...\n");
			ret = ioctl(fd_master, LPAIF_INVERT_BIT_CLOCK);
			if (ret < 0) {
				printf("Failed to toggle bit clock on target\n");
			}
			break;
		case CONFIG_DAB_MRC:
			if (argc < 7) {
				help();
				ret = -1;
				break;
			}
			/* Check for valid inputs */
			if (!fd_out_a || !fd_out_b) {
				printf("Unable to open output files, exiting...\n");
				ret = -1;
				break;
			}
			if (target_type) {
				printf("DAB MRC support is not available on SA8155/SA8195 targets\n");
				ret = -1;
			} else {
				/* Set CPU affinity to one of the available high cores */
				if (set_affinity) {
					printf("Setting CPU affinity \n");
					CPU_ZERO(&cpuset);
					for (i = 6; i < 8; i++) {
						CPU_SET(i, &cpuset);
						ret = sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
						if (ret) {
							printf("Cannot set CPU affinity on CPU[%d]\n", i);
							CPU_ZERO(&cpuset);
							continue;
						}
						printf("CPU affinity set on CPU[%d]\n", i);
						break;
					}
				}

				/* Open HS-I2S device files */
				printf("Opening i2s device files...\n");
				for (i = 0; i < 2; i++) {
					fd_dab[i] = open(hs_dev[i], O_RDWR);
					if(fd_dab[i] < 0) {
						printf("Cannot open hs%d device file\n", i);
						help();
						ret = -1;
						goto exit_app;
					}
				}

				/* Open core device file */
				fd_core = open("/dev/hsi2s_reginfo", O_RDWR);
				if(fd_core < 0) {
					printf("Cannot open core device file\n");
					ret = -1;
					goto exit_app;
				}

				/* Allocate the thread parameters */
				for (i = 0; i < 2; i++) {
					dab_params[i] = (struct thread_params *) malloc(sizeof(struct thread_params));
					if (dab_params[i]  == NULL) {
						printf("Unable to allocate thread params for hs%d\n", i);
						ret = -1;
						goto exit_app;
					}
				}

				/* Store the output file descriptors */
				dab_params[0]->fd_write_op = fd_out_a;
				dab_params[1]->fd_write_op = fd_out_b;

				/* Set the read limit and periodic length */
				read_limit = READ_LIMIT;
				mmap_len = get_periodic_length(bit_clk, buffer_ms);
				printf("Periodic length set to %ld bytes\n", mmap_len);

				/* Reset the HS-I2S instances */
				for (i = 0; i < 2; i++) {
					if (ioctl(fd_dab[i], LPAIF_RESET) < 0) {
						printf("Failed to reset hs%d instance\n", i);
						ret = -1;
						goto exit_app;
					}
				}

				/* Set DAB MRC configuration */
				printf("Setting DAB MRC configuration...\n");
				if (ioctl(fd_dab[0], CONFIGURE_DAB_MRC) < 0) {
					printf("Failed to set DAB MRC configuration on target\n");
					ret = -1;
					goto exit_app;
				}

				/* Map the write DMA buffers */
				for (i = 0; i < 2; i++) {
					dab_params[i]->pfd.fd = fd_dab[i];
					dab_params[i]->pfd.events = EPOLLIN | EPOLLRDNORM;
					printf("Mapping userspace memory with kernel memory for device\n");
					dab_params[i]->mmap_ptr = mmap(NULL, read_length_bytes * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd_dab[i], 0);
					if (dab_params[i]->mmap_ptr == MAP_FAILED) {
						printf("mmap failed for device\n");
						ret = -1;
						goto exit_app;
					} else {
					dab_params[i]->mmap_read = dab_params[i]->mmap_ptr;
					dab_params[i]->mmap_end = dab_params[i]->mmap_ptr + (read_length_bytes * 2);
					}
					dab_params[i]->minor = i;
				}

				/* Map the register info memory */
				printf("Mapping userspace memory with kernel memory for core\n");
				dab_params[0]->sh_mem = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_core, 0);
				if (dab_params[0]->sh_mem == MAP_FAILED) {
					printf("mmap failed for core\n");
					ret = -1;
					goto exit_app;
				}
				dab_params[1]->sh_mem = dab_params[0]->sh_mem;

				/* Create thread to read the received data */
				for (i = 0; i < 2; i++) {
					printf("Creating thread to read the received data\n");
					if (use_normal_read) {
						if (pthread_create(&tid_dab[i], NULL, poll_read_normal, dab_params[i]) != 0) {
							printf("Error creating poll thread\n");
						}
					} else {
						if (pthread_create(&tid_dab[i], NULL, poll_read_fast, dab_params[i]) != 0) {
							printf("Error creating poll thread\n");
						}
					}
				}

				/* Wait for the threads to join */
				printf("Joining threads\n");
				for (i = 0; i < 2; i++) {
					pthread_join(tid_dab[i], NULL);
				}
				printf("Threads joined \n");
			}
			break;
		default:
			printf("Undefined operation mode\n");
			ret = -1;
			break;
	};

exit_app:
	printf("Clean-up in progress...\n");
	if (wav_data) {
		free(wav_data);
		wav_data = NULL;
	}
	if (fd_core >= 0) {
		close(fd_core);
		fd_core = -1;
	}
	if (fd_read_ip) {
		fclose(fd_read_ip);
		fd_read_ip = NULL;
	}
	if (fd_write_op) {
		fclose(fd_write_op);
		fd_write_op = NULL;
	}
	if (params) {
		free(params);
		params = NULL;
	}
	for (i = 0; i < 2; i++) {
		if (fd_dab[i] >= 0) {
			close(fd_dab[i]);
			fd_dab[i] = -1;
		}
		if (dab_params[i]) {
			free(dab_params[i]);
			dab_params[i] = NULL;
		}
	}
	if (fd_out_a) {
		fclose(fd_out_a);
		fd_out_a = NULL;
	}
	if (fd_out_b) {
		fclose(fd_out_b);
		fd_out_b = NULL;
	}
	if (fd_slave >= 0) {
		close(fd_slave);
		fd_slave = -1;
	}
	if (fd_master >= 0) {
		close(fd_master);
		fd_master = -1;
	}
	printf("Exiting...\n");

	return ret;
}
