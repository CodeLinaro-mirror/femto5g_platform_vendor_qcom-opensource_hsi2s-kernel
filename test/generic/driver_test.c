/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include <time.h>
#include <errno.h>

/* IOCTL commands copied from the i2s_driver header */
/* Configures I2S/PCM and DMA registers for normal data transfer on the interface */
#define LPAIF_NORMAL_MODE _IOWR('i', 0, int)
/* Configures I2S/PCM and DMA registers for internal loopback on the interface */
#define LPAIF_INTERNAL_LOOPBACK _IOWR('i', 1, int)
/* Configures I2S/PCM and DMA registers for external loopback on the interface */
#define LPAIF_EXTERNAL_LOOPBACK _IOWR('i', 2, int)
/* Configures the interface as either master or slave */
#define LPAIF_MUXMODE _IOWR('i', 3, int)
/* Configures I2S/PCM and DMA registers for speaker operation on the interface */
#define LPAIF_SPEAKER _IOWR('i', 4, int)
/* Configures I2S/PCM and DMA registers for mic operation on the interface */
#define LPAIF_MIC _IOWR('i', 5, int)
/* Used in master-slave external loopback to set slave field of master interface data structure */
#define LPAIF_SET_SLAVE _IOWR('i', 6, int)
/* Enables read DMA channel and speaker */
#define LPAIF_INIT_TX _IOWR('i', 7, int)
/* Disables read DMA channel and speaker */
#define LPAIF_DEINIT_TX _IOWR('i', 8, int)
/* Configures the master clock on the interface */
#define LPAIF_SET_CLOCK _IOWR('i', 9, int)
/* Resets the I2S/PCM and DMA registers */
#define LPAIF_RESET _IOWR('i', 10, int)
/* Configures LPAIF to be in I2S/PCM mode */
#define LPAIF_MODE _IOWR('i', 11, int)
/* Configures I2S parameters on the interface */
#define I2S_CONFIG_PARAMS _IOWR('i', 12, int)
/* Configures PCM parameters on the interface */
#define PCM_CONFIG_PARAMS _IOWR('i', 13, int)
/* Configures TDM parameters on the interface */
#define TDM_CONFIG_PARAMS _IOWR('i', 14, int)
/* Sets PCM lane configuration */
#define PCM_CONFIG_LANE  _IOWR('i', 15, int)
/* Inverts the bit clock on the interface */
#define LPAIF_INVERT_BIT_CLOCK _IOWR('i', 16, int)

/* Macros */
#define BYTES_PER_WORD 4
#define READ_LENGTH_MB 2
#define READ_LENGTH_WORDS (READ_LENGTH_MB * 1024 * 1024) / BYTES_PER_WORD
#define READ_LIMIT 4294967926 /* 4GB */
#define SRC_DIGITAL_PLL 0x500
#define BILLION 1000000000L
#define INVERT 1
#define EXTERNAL 1
#define INVERT_INT_BIT_CLOCK 0x0
#define INVERT_EXT_BIT_CLOCK 0x1
#define DONT_INVERT_INT_BIT_CLOCK 0x2
#define DONT_INVERT_EXT_BIT_CLOCK 0x3

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
	CONFIG_BIT_CLK
};

/* Rx mode */
enum rx_mode {
	READ,
	MMAP
};

/* I2S parameters */
struct i2s_params {
	unsigned int bit_clk;
	unsigned int buffer_ms;
	unsigned int bit_depth;
	unsigned int spkr_channel_count;
	unsigned int mic_channel_count;
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

int fd_master;
int fd_slave;
FILE *fd_read_ip;
FILE *fd_write_op;
long read_length_bytes;
long read_length_words;
long long read_limit;
enum operation_mode mode;
enum rx_mode rx = MMAP;
struct i2s_params *i_params;
struct pcm_params *p_params;
struct tdm_params *t_params;
struct pollfd pfd;
void *mmap_ptr;
void *mmap_read;
void *mmap_end;
long mmap_len;
struct timespec pread_start;
struct timespec pread_stop;
struct timespec nread_start;
struct timespec nread_stop;

/* Prints the usage information */
void help()
{

	printf("Operational modes:\n 0 - Normal Rx\n 1 - Normal Tx*\n 2 - Internal loopback\n 3 - External loopback on master*\n"
	       " 4 - External loopback on master-slave*\n 5 - Set master/slave mode*\n 6 - Configure master clock*\n"
	       " 7 - Configure I2S params\n 8 - Configure PCM params\n 9 - Configure TDM params\n"
	       " 10 - Configure LPAIF mode\n 11 - Set PCM lane configuration\n 12 - Configure bit clock*\n");
	printf("* Supported only on SA8155/SA8195\n\n");
	printf("Usage for each operation mode:\n\n");
	printf("NORMAL Rx:\n");
	printf("hsi2s_test 0 <device file> <output file> <bit clock in Hz> <data buffer in ms> [<size>]\n\n");
	printf("NORMAL Tx:\n");
	printf("hsi2s_test 1 <device file> <input file>\n\n");
	printf("INTERNAL LOOPBACK:\n");
	printf("hsi2s_test 2 <device file> <output file> <input file> [<size>]\n\n");
	printf("EXTERNAL LOOPBACK ON MASTER:\n");
	printf("hsi2s_test 3 <device file> <output file> <input file> [<size>]\n\n");
	printf("EXTERNAL LOOPBACK BETWEEN MASTER AND SLAVE INTERFACES:\n");
	printf("hsi2s_test 4 <master device file> <slave device file> <output file> <input file> [<size>]\n\n");
	printf("SET MASTER/SLAVE MODE:\n");
	printf("hsi2s_test 5 <device file> <muxmode>\n\n");
	printf("CONFIGURE MASTER CLOCK:\n");
	printf("hsi2s_test 6 <device file> <clock-source> <divide-by>\n\n");
	printf("CONFIGURE I2S PARAMETERS:\n");
	printf("hsi2s_test 7 <device file> <bit clock in hertz> <data buffer in ms> <bit depth> <speaker channel count> <mic channel count>\n\n");
	printf("CONFIGURE PCM PARAMETERS:\n");
	printf("hsi2s_test 8 <device file> <bit clock in hertz> <data buffer in ms> <pcm_rate> <sync_src> <aux_mode> <rpcm_width> <tpcm_width>\n\n");
	printf("CONFIGURE TDM PARAMETERS:\n");
	printf("hsi2s_test 9 <device file> <sync_delay> <tdm_tpcm_width> <tdm_rpcm_width> <tdm_rate> <en_diff_sample_width> [<tpcm_sample_width>] [<rpcm_sample_width>]\n\n");
	printf("SET I2S/PCM MODE:\n");
	printf("hsi2s_test 10 <device file> <lpaif mode>\n\n");
	printf("SET PCM LANE CONFIGURATION:\n");
	printf("hsi2s_test 11 <device file> <lane config>\n\n");
	printf("CONFIGURE BIT CLOCK:\n");
	printf("hsi2s_test 12 <device file> <invert/dont-invert> <bit clock type>\n\n");
	printf("Argument details:\n");
	printf("<device file> : /dev/hs0_i2s | /dev/hs1_i2s | /dev/hs2_i2s\n");
	printf("<muxmode> : 0 -> MASTER 1 -> SLAVE\n");
	printf("<output file> : To store the data read from the device file\n");
	printf("<input file> : To be written to the HS-I2S interface via device file\n");
	printf("<size> : DMA buffer length in MB (4MB by default)\n");
	printf("<bit clock in hertz> : Bit clock freqeuncy in Hertz\n");
	printf("<data buffer in ms> : Periodic length of data buffer in milli seconds\n");
	printf("<bit depth> : 16 | 24 | 25 | 32\n");
	printf("<speaker channel count> : 1 | 2 | 4\n");
	printf("<mic channel count> : 1 | 2 |4\n");
	printf("<clock-source> : 0 -> CXO(19.2 MHz) 1 -> DIGITAL PLL(122.88 MHz)\n");
	printf("<divide-by> : 0 -> Bypass, 1 -> Div-1, 2 -> Div-1.5, 3 -> Div-2, 4 -> Div-2.5, ..... 31 -> Div-16\n");
	printf("<pcm_rate> : Frame size : 0 -> 8 bits, 1 -> 16 bits, 2 -> 32 bits, 3 -> 64 bits, 4 -> 128 bits, 5 -> 256 bits\n");
	printf("<sync_src> : 0 -> EXTERNAL SYNC 1 -> INTERNAL SYNC\n");
	printf("<aux_mode> : 0 -> PCM(SHORT SYNC) 1 -> AUX(LONG SYNC)\n");
	printf("<rpcm_width> : PCM receive slot size : 0 -> 8 bits, 1 -> 16 bits\n");
	printf("<tpcm_width> : PCM transmit slot size : 0 -> 8 bits, 1 -> 16 bits\n");
	printf("<sync_delay> : 0 -> 2 CYCLE DELAY, 1 -> 1 CYCLE DELAY, 2 -> 0 CYCLE DELAY\n");
	printf("<tdm_tpcm_width> : TDM transmit slot size in bits (maximum 32)\n");
	printf("<tdm_rpcm_width> : TDM receive slot size in bits (maximum 32)\n");
	printf("<tdm_rate> : TDM frame size in bits (maximum 512)\n");
	printf("<en_diff_sample_width> : To be enabled if sample width(number of useful bits) is different from slot size\n");
	printf("<tpcm_sample_width> : TDM TPCM sample width in bits (maximum 32)\n");
	printf("<rpcm_sample_width> : TDM RPCM sample width in bits (maximum 32)\n");
	printf("<lpaif mode> : 0 -> HS-I2S mode 1 -> HS-PCM mode\n");
	printf("<lane config> : 0 -> SINGLE LANE, 1 -> MULTI LANE RX, 2 -> MULTI LANE TX\n");
	printf("<invert/dont-invert> : 0 - Do not invert bit clock(default) 1 - Invert bit clock\n");
	printf("<bit clock type> : 0 - Internal(Slave mode) 1 - External(Master mode)\n\n");
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

/* Read thread */
void *poll_read(void *arg)
{
	long long r_limit = 0;
	long temp;
	int ret;
	double thread_start;
	double thread_stop;
	double delta;

	printf("Performing poll wait...\n");

	clock_gettime(CLOCK_REALTIME, &pread_start);
	thread_start = (pread_start.tv_sec * BILLION) + pread_start.tv_nsec;
	printf("[POLL] Starttime %lf\n", thread_start);

	while (r_limit < read_limit) {
		clock_gettime(CLOCK_REALTIME, &pread_start);
		ret = poll(&pfd, 1, -1);
		clock_gettime(CLOCK_REALTIME, &pread_stop);
		delta = ((pread_stop.tv_sec - pread_start.tv_sec) * BILLION) +
				(pread_stop.tv_nsec - pread_start.tv_nsec);
		printf("[POLL] Data ready in %lf nsec\n", delta);
		if (ret < 0) {
			printf("Poll failed\n");
		} else if (pfd.revents & POLLIN) {
			if (r_limit + mmap_len > read_limit) {
				if (mmap_read + (read_limit - r_limit) > mmap_end) {
					temp = mmap_end - mmap_read;
					fwrite(mmap_read,temp,1,fd_write_op);
					temp = (read_limit - r_limit) - temp;
					fwrite(mmap_ptr,temp,1,fd_write_op);
					mmap_read = mmap_ptr + temp;

				} else {
					fwrite(mmap_read,read_limit - r_limit,1,fd_write_op);
					mmap_read += (read_limit - r_limit);
				}
				r_limit += (read_limit - r_limit);
			} else {
				if (mmap_read + mmap_len > mmap_end) {
					temp = mmap_end - mmap_read;
					fwrite(mmap_read,temp,1,fd_write_op);
					temp = mmap_len - temp;
					fwrite(mmap_ptr,temp,1,fd_write_op);
					mmap_read = mmap_ptr + temp;
				} else {
					fwrite(mmap_read,mmap_len,1,fd_write_op);
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

	return NULL;
}

/* Read thread */
void *user_read(void *arg)
{
	int i;
	size_t transfer_length;
	int32_t *received_data;
	int cnt = 0;
	long long r_limit = 0;
	int boundary_read;
	double thread_start;
	double thread_stop;
	double delta;

	printf("Performing data read...\n");

	received_data = (int32_t *) malloc(read_length_words * sizeof(int32_t));
	if (!received_data) {
		printf("Failed to allocate receive data buffer\n");
		return NULL;
	}

	clock_gettime(CLOCK_REALTIME, &nread_start);
	thread_start = (nread_start.tv_sec * BILLION) + nread_start.tv_nsec;
	printf("[READ] Starttime %lf\n", thread_start);

	while (r_limit < read_limit) {
		if (mode != EXTERNAL_LB_MASTER_SLAVE) {
			clock_gettime(CLOCK_REALTIME, &nread_start);
			transfer_length = read(fd_master, received_data, read_length_bytes);
			clock_gettime(CLOCK_REALTIME, &nread_stop);
		} else {
			clock_gettime(CLOCK_REALTIME, &nread_start);
			transfer_length = read(fd_slave, received_data, read_length_bytes);
			clock_gettime(CLOCK_REALTIME, &nread_stop);
		}
		delta = ((nread_stop.tv_sec - nread_start.tv_sec) * BILLION) +
			(nread_stop.tv_nsec - nread_start.tv_nsec);

		printf("Bytes read: %zd in %lf nsecs\n", transfer_length, delta);

		if (transfer_length < 0) {
			printf("Error in reading data from the driver\n");
			break;
		}

		if ((r_limit + transfer_length) > read_limit) {
			boundary_read = (read_limit - r_limit);
			fwrite(received_data,boundary_read,1,fd_write_op);
		}
		else
			fwrite(received_data,BYTES_PER_WORD,read_length_words,fd_write_op);

		r_limit = r_limit + transfer_length;
	}

	clock_gettime(CLOCK_REALTIME, &nread_stop);
	thread_stop = (nread_stop.tv_sec * BILLION) + nread_stop.tv_nsec;
	delta = (thread_stop - thread_start) / BILLION;
	printf("[READ] Endtime %lf\n", thread_stop);
	printf("[READ] Total thread execution time %lfs\n", delta);

	free(received_data);

	return NULL;
}

int main(int argc, char **argv)
{
	long wav_samples;
	long no_words;
	long w_len;
	int32_t *wav_data = NULL;
	int32_t temp_data;
	int i;
	int mux;
	int arg = 1;
	int slave;
	int clk_source;
	uint32_t divide_by;
	uint32_t reg_val;
	pthread_t tid;
	int ret = 0;

	if (argc < 4) {
		help();
		exit(0);
	}

	printf("Reading operation mode...\n");
	mode = atoi(argv[arg++]);
	if (mode > CONFIG_BIT_CLK) {
		printf("Undefined mode\n");
		help();
		exit(0);
	}

	printf("Opening i2s device file...\n");
	fd_master = open(argv[arg++], O_RDWR);
	if(fd_master < 0) {
		printf("Cannot open device file\n");
		help();
		exit(0);
	}

	/* Operation mode : Configure bit clock */
	if (mode == CONFIG_BIT_CLK) {
		if (argc < 5) {
			help();
			exit(0);
		}
		mux = atoi(argv[arg++]);
		clk_source  = atoi(argv[arg++]);
		if (mux == INVERT) {
			if (clk_source == EXTERNAL)
				reg_val = INVERT_EXT_BIT_CLOCK;
			else
				reg_val = INVERT_INT_BIT_CLOCK;
		} else {
			if (clk_source == EXTERNAL)
				reg_val = DONT_INVERT_EXT_BIT_CLOCK;
			else
				reg_val = DONT_INVERT_INT_BIT_CLOCK;
		}
		printf("Configuring bit clock...\n");
		if (ioctl(fd_master, LPAIF_INVERT_BIT_CLOCK, reg_val) < 0) {
			printf("Failed to configure bit clock on target\n");
		}
		exit(0);
	}

	/* Operation mode : Set PCM lane configuration */
	if (mode == CONFIG_PCM_LANE) {
		mux = atoi(argv[arg++]);
		printf("Setting PCM lane configuration...\n");
		if (ioctl(fd_master, PCM_CONFIG_LANE, mux) < 0) {
			printf("Failed to set PCM lane configuration on target\n");
			exit(0);
		}
		exit(0);
	}

	/* Operation mode : Set LPAIF interface in I2S/PCM mode */
	if (mode == CONFIG_LPAIF_MODE) {
		mux = atoi(argv[arg++]);
		printf("Setting LPAIF mode...\n");
		if (ioctl(fd_master, LPAIF_MODE, mux) < 0) {
			printf("Failed to set I2S/PCM configuration on target\n");
			exit(0);
		}
		exit(0);
	}

	/* Operation mode : Configure master clock */
	if (mode == CONFIG_M_CLK) {
		if (argc < 5) {
			help();
			exit(0);
		}

		printf("Reading clock source...\n");
		clk_source = atoi(argv[arg++]);
		if (clk_source > 1) {
			printf("Undefined source\n");
			help();
			exit(0);
		}

		divide_by = atoi(argv[arg++]);
		if (divide_by > 31) {
			printf("Undefined division\n");
			help();
			exit(0);
		}

		if (!clk_source) {
			printf("Clock source is CXO\n");
			reg_val = divide_by;
			if (ioctl(fd_master, LPAIF_SET_CLOCK, reg_val) < 0) {
				printf("Failed to set master clock on target\n");
				exit(0);
			}

		} else {
			printf("Clock source is Digital PLL\n");
			reg_val = SRC_DIGITAL_PLL | divide_by;
			if (ioctl(fd_master, LPAIF_SET_CLOCK, reg_val) < 0) {
				printf("Failed to set master clock on target\n");
				exit(0);
			}
		}

		printf("Successfully configured master clock\n\n");
		exit(0);
	}

	/* Operation mode : Configure I2S parameters */
	if (mode == CONFIG_I2S_PARAMS) {
		printf("Configuring I2S parameters...\n");
		if (argc < 8) {
			help();
			exit(0);
		}
		i_params = (struct i2s_params *) malloc(sizeof(struct i2s_params));
		if (!i_params) {
			printf("Failed to allocate I2S param structure\n");
			return -ENOMEM;
		}
		i_params->bit_clk = atoi(argv[arg++]);
		i_params->buffer_ms = atoi(argv[arg++]);
		i_params->bit_depth = atoi(argv[arg++]);
		i_params->spkr_channel_count = atoi(argv[arg++]);
		i_params->mic_channel_count = atoi(argv[arg++]);
		if (ioctl(fd_master, I2S_CONFIG_PARAMS, i_params) < 0) {
			printf("Failed to configure I2S parameters on target\n");
			free(i_params);
			exit(0);
		}
		free(i_params);
		exit(0);
	}

	/* Operation mode : Configure PCM parameters */
	if (mode == CONFIG_PCM_PARAMS) {
		printf("Configuring PCM parameters...\n");
		if (argc < 10) {
			help();
			exit(0);
		}
		p_params = (struct pcm_params *) malloc(sizeof(struct pcm_params));
		if (!p_params) {
			printf("Failed to allocate PCM param structure\n");
			return -ENOMEM;
		}
		p_params->bit_clk = atoi(argv[arg++]);
		p_params->buffer_ms = atoi(argv[arg++]);
		p_params->rate = atoi(argv[arg++]);
		p_params->sync_src = atoi(argv[arg++]);
		p_params->aux_mode = atoi(argv[arg++]);
		p_params->rpcm_width = atoi(argv[arg++]);
		p_params->tpcm_width = atoi(argv[arg++]);
		if (ioctl(fd_master, PCM_CONFIG_PARAMS, p_params) < 0) {
			printf("Failed to configure PCM parameters on target\n");
			free(p_params);
			exit(0);
		}
		free(p_params);
		exit(0);
	}

	/* Operation mode : Configure TDM parameters */
	if (mode == CONFIG_TDM_PARAMS) {
		printf("Configuring TDM parameters...\n");
		if (argc < 8) {
			help();
			exit(0);
		}
		t_params = (struct tdm_params *) malloc(sizeof(struct tdm_params));
		if (!t_params) {
			printf("Failed to allocate TDM param structure\n");
			return -ENOMEM;
		}
		t_params->sync_delay = atoi(argv[arg++]);
		t_params->tpcm_width = atoi(argv[arg++]);
		t_params->rpcm_width = atoi(argv[arg++]);
		t_params->rate = atoi(argv[arg++]);
		t_params->en_diff_sample_width = atoi(argv[arg++]);
		if (t_params->en_diff_sample_width) {
			if(arg < argc) {
				t_params->tpcm_sample_width = atoi(argv[arg++]);
				t_params->rpcm_sample_width = atoi(argv[arg++]);
			} else {
				help();
				exit(0);
			}
		}
		if (ioctl(fd_master, TDM_CONFIG_PARAMS, t_params) < 0) {
			printf("Failed to configure TDM parameters on target\n");
			free(t_params);
			exit(0);
		}
		free(t_params);
		exit(0);
	}

	/* Operation mode : Set I2S interface as master/slave */
	if (mode == SET_MUXMODE) {
		mux = atoi(argv[arg++]);
		printf("Setting muxmode...\n");
		if (ioctl(fd_master, LPAIF_MUXMODE, mux) < 0) {
			printf("Failed to set master/slave configuration on target\n");
			exit(0);
		}
		exit(0);
	}

	/* Operation mode : External loopback between master and slave interfaces */
	if (mode == EXTERNAL_LB_MASTER_SLAVE) {
		if (argc < 6) {
			help();
			exit(0);
		}
		fd_slave = open(argv[arg++], O_RDWR);
		if(fd_slave < 0) {
			printf("Cannot open slave device file\n");
			help();
			exit(0);
		}
		slave = argv[arg-1][7] - '0';
		printf("Slave node is hs%d\n",slave);
	}

	if (mode != NORMAL_TX) {
		/* Open the output file to store received data */
		printf("Opening o/p file...\n");
		fd_write_op = fopen(argv[arg++], "w");
		if (fd_write_op == NULL) {
			printf("Cannot open output file\n");
			help();
			exit(0);
		}
	}

	/* Initializing to default macros and use the macros if no size specified by user*/
	read_length_bytes = READ_LENGTH_MB * 1024 * 1024;
	read_length_words = READ_LENGTH_WORDS;
	mmap_len = read_length_bytes;

	if (mode != NORMAL_RX) {
		switch (mode) {
		/* Operation mode : Normal Tx */
		case NORMAL_TX:
			printf("Setting Tx on master\n");
			if (ioctl(fd_master, LPAIF_RESET) < 0) {
				printf("Failed to reset the hsi2s device\n");
				exit(0);
			}
			if (ioctl(fd_master, LPAIF_MUXMODE, 0) < 0) {
				printf("Failed to set master mode\n");
				exit(0);
			}
			if (ioctl(fd_master, LPAIF_SPEAKER) < 0) {
				printf("Failed to configure speaker\n");
				exit(0);
			}
			break;
		/* Operation mode : Internal loopback */
		case INTERNAL_LB:
			if (argc < 5) {
				help();
				exit(0);
			}
			printf("Setting internal loopback operation \n");
			if (ioctl(fd_master, LPAIF_RESET) < 0) {
				printf("Failed to reset the hsi2s device\n");
				exit(0);
			}
			if (ioctl(fd_master, LPAIF_INTERNAL_LOOPBACK) < 0) {
				printf("Failed to trigger internal loopback\n");
				exit(0);
			}
			break;
		/* Operation mode : External loopback on master interface */
		case EXTERNAL_LB_MASTER:
			if (argc < 5) {
				help();
				exit(0);
			}
			printf("Setting external loopback on master \n");
			if (ioctl(fd_master, LPAIF_RESET) < 0) {
				printf("Failed to reset the hsi2s device\n");
				exit(0);
			}
			if (ioctl(fd_master, LPAIF_EXTERNAL_LOOPBACK) < 0) {
				printf("Failed to trigger external loopback\n");
				exit(0);
			}
			break;
		/* Operation mode : External loopback between master and slave interfaces */
		case EXTERNAL_LB_MASTER_SLAVE:
			printf("Setting external loopback on master/slave \n");
			if (ioctl(fd_master, LPAIF_RESET) < 0) {
				printf("Failed to reset the hsi2s master\n");
				exit(0);
			}
			if (ioctl(fd_slave, LPAIF_RESET) < 0) {
				printf("Failed to reset the hsi2s slave\n");
				exit(0);
			}
			if (ioctl(fd_master, LPAIF_MUXMODE, 0) < 0) {
				printf("Failed to set master mode\n");
				exit(0);
			}
			if (ioctl(fd_slave, LPAIF_MUXMODE, 1) < 0) {
				printf("Failed to set slave mode\n");
				exit(0);
			}
			if (ioctl(fd_master, LPAIF_SET_SLAVE, slave) < 0) {
				printf("Failed to set slave for the master\n");
				exit(0);
			}
			if (ioctl(fd_slave, LPAIF_MIC) < 0) {
				printf("Failed to configure mic\n");
				exit(0);
			}
			if (ioctl(fd_master, LPAIF_SPEAKER) < 0) {
				printf("Failed to configure speaker\n");
				exit(0);
			}
			break;
		}

		printf("Opening i/p file...\n");
		fd_read_ip = fopen(argv[arg++], "r");
		if (fd_read_ip == NULL) {
			printf("Cannot open input file\n");
			help();
			exit(0);
		}

		printf("Getting i/p file size...\n");
		wav_samples = get_size(fd_read_ip);
		no_words = wav_samples/BYTES_PER_WORD;
		printf("File size BYTES: %ld WORDS %ld \n",wav_samples,no_words);
		read_limit = wav_samples;

		if (arg < argc) {
			/* Use the size provided by the user */
			read_length_bytes = (atoi(argv[arg++]) * 1024 * 1024) / 2;
			read_length_words = read_length_bytes/BYTES_PER_WORD;
			mmap_len = read_length_bytes;
		}
	} else {
		/* Operation mode : Normal mode data reception */
		printf("Setting normal mode \n");
		if (ioctl(fd_master, LPAIF_RESET) < 0) {
			printf("Failed to reset the hsi2s device\n");
			exit(0);
		}
		if (ioctl(fd_master, LPAIF_NORMAL_MODE) < 0) {
			printf("Failed to configure normal operation on the hsi2s device\n");
			exit(0);
		}

		read_limit = READ_LIMIT;

		if (rx == MMAP) {
			if (argc < 6) {
				help();
				exit(0);
			}

			i_params = (struct i2s_params *) malloc(sizeof(struct i2s_params));
			if (!i_params) {
				printf("Failed to allocate I2S param structure\n");
				return -ENOMEM;
			}
			i_params->bit_clk = atoi(argv[arg++]);
			i_params->buffer_ms = atoi(argv[arg++]);

			mmap_len = get_periodic_length(i_params->bit_clk, i_params->buffer_ms);
			printf("Periodic length set to %ld bytes\n", mmap_len);

			free(i_params);
		}

		if (arg < argc) {
			/* Use the size provided by the user */
			read_length_bytes = (atoi(argv[arg++]) * 1024 * 1024) / 2;
			read_length_words = read_length_bytes/BYTES_PER_WORD;
		}
	}

	if (mode != NORMAL_TX) {
		if (rx == MMAP) {
			if (mode == EXTERNAL_LB_MASTER_SLAVE) {
				/* Map the slave device write DMA buffer */
				pfd.fd = fd_slave;
				pfd.events = POLLIN | POLLRDNORM;
				printf("Mapping userspace memory with kernel memory\n");
				mmap_ptr = mmap(NULL, read_length_bytes * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd_slave, 0);
				if (mmap_ptr == MAP_FAILED) {
					printf("mmap failed\n");
					goto exit_app;
				} else {
					mmap_read = mmap_ptr;
					mmap_end = mmap_ptr + (read_length_bytes * 2);
				}
			} else {
				/* Map the device write DMA buffer */
				pfd.fd = fd_master;
				pfd.events = POLLIN | POLLRDNORM;
				printf("Mapping userspace memory with kernel memory\n");
				mmap_ptr = mmap(NULL, read_length_bytes * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd_master, 0);
				if (mmap_ptr == MAP_FAILED) {
					printf("mmap failed\n");
					goto exit_app;
				} else {
					mmap_read = mmap_ptr;
					mmap_end = mmap_ptr + (read_length_bytes * 2);
				}
			}
		}

		/* Create thread to read the received data */
		printf("Creating thread to read the received data\n");
		switch (rx) {
		case READ:
			printf("Using read mode...\n");
			if (pthread_create(&tid, NULL, user_read, NULL) != 0) {
				printf("Error creating reader thread\n");
			}
			break;
		case MMAP:
			printf("Using mmap mode...\n");
			if (pthread_create(&tid, NULL, poll_read, NULL) != 0) {
				printf("Error creating poll thread\n");
			}
			break;
		default:
			printf("Using read mode...\n");
			if (pthread_create(&tid, NULL, user_read, NULL) != 0) {
				printf("Error creating reader thread\n");
			}
			break;
		}
	}

	/* Reading from input file only for loopback modes */
	if (mode != NORMAL_RX) {
		printf("Reading i/p file...\n");
		wav_data = (int32_t *) malloc(no_words * sizeof(int32_t));
		if (!wav_data) {
			printf("Failed to allocate transmit data buffer\n");
			return -ENOMEM;
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
			exit(0);
		}
	}

	if (mode != NORMAL_TX) {
		printf("Joining threads\n");
		pthread_join(tid,NULL);
		printf("Threads joined \n");
	}

	/* Disable transmission */
	if (mode != NORMAL_RX) {
		printf("Disabling Tx on read DMA channel\n");
		if (ioctl(fd_master, LPAIF_DEINIT_TX) < 0) {
			printf("Failed to stop Tx on hsi2s device\n");
			exit(0);
		}
	}

exit_app:
	printf("Closing Files \n");
	if (mode != NORMAL_RX) {
		free(wav_data);
		fclose(fd_read_ip);
	}
	if (mode != NORMAL_TX)
		fclose(fd_write_op);
	if (mode == EXTERNAL_LB_MASTER_SLAVE)
		close(fd_slave);
	close(fd_master);

	return 0;
}
