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

/* IOCTL commands copied from the i2s_driver header */
#define I2S_NORMAL_MODE _IOWR('i', 0, int)
#define I2S_INTERNAL_LOOPBACK _IOWR('i', 1, int)
#define I2S_EXTERNAL_LOOPBACK _IOWR('i', 2, int)
#define I2S_MUXMODE _IOWR('i', 3, int)
#define I2S_SPEAKER _IOWR('i', 4, int)
#define I2S_MIC _IOWR('i', 5, int)
#define I2S_SET_SLAVE _IOWR('i', 6, int)
#define I2S_INIT_TX _IOWR('i', 7, int)
#define I2S_DEINIT_TX _IOWR('i', 8, int)
#define I2S_CONFIG_PARAMS _IOWR('i', 9, int)
#define I2S_RESET _IOWR('i', 10, int)

/* Macros */
#define BYTES_PER_WORD 4
#define READ_LENGTH_MB 2
#define READ_LENGTH_WORDS (READ_LENGTH_MB * 1024 * 1024) / BYTES_PER_WORD
#define READ_LIMIT 1024*1024*1024

/* Operation mode of the test utility */
enum operation_mode {
	NORMAL,
	INTERNAL_LB,
	EXTERNAL_LB_MASTER,
	EXTERNAL_LB_MASTER_SLAVE,
	SET_MUXMODE,
	CONFIG_PARAMS
};

/* I2S parameters */
struct i2s_params {
	unsigned int bit_clk;
	unsigned int buffer_ms;
	unsigned int bit_depth;
	unsigned int spkr_channel_count;
	unsigned int mic_channel_count;
};

int fd_master;
int fd_slave;
FILE *fd_read_ip;
FILE *fd_write_op;
long read_length_bytes;
long read_length_words;
long read_limit;
enum operation_mode mode;
struct i2s_params *params;

/* Prints the usage information */
void help()
{

	printf("Operational modes:\n 0 - Normal mode\n 1 - Internal loopback\n 2 - External loopback on master*\n"
	       " 3 - External loopback on master-slave*\n 4 - Set master/slave mode*\n 5 - Configure I2S parameters\n");
	printf("* Supported only on SA8155\n\n");
	printf("Usage for each operation mode:\n\n");
	printf("NORMAL MODE:\n");
	printf("hsi2s_test 0 <device file> <output file> [<size>]\n\n");
	printf("INTERNAL LOOPBACK:\n");
	printf("hsi2s_test 1 <device file> <output file> <input file> [<size>]\n\n");
	printf("EXTERNAL LOOPBACK ON MASTER:\n");
	printf("hsi2s_test 2 <device file> <output file> <input file> [<size>]\n\n");
	printf("EXTERNAL LOOPBACK BETWEEN MASTER AND SLAVE INTERFACES:\n");
	printf("hsi2s_test 3 <master device file> <slave device file> <output file> <input file> [<size>]\n\n");
	printf("SET MASTER/SLAVE MODE:\n");
	printf("hsi2s_test 4 <device file> <muxmode>\n\n");
	printf("CONFIGURE I2S PARAMETERS:\n");
	printf("hsi2s_test 5 <device file> <bit clock in hertz> <data buffer in ms> <bit depth> <speaker channel count> <mic channel count>\n\n");
	printf("Argument details:\n");
	printf("<device file> : /dev/hs0_i2s | /dev/hs1_i2s | /dev/hs2_i2s\n");
	printf("<muxmode> : 0 - Master 1 - Slave\n");
	printf("<output file> : To store the data read from the device file\n");
	printf("<input file> : To be provided only for loopback modes\n");
	printf("<size> : DMA buffer length in MB (4MB by default)\n");
	printf("<bit clock in hertz> : Bit clock freqeuncy in Hertz\n");
	printf("<data buffer in ms> : Periodic length of data buffer in milli seconds\n");
	printf("<bit depth> : 16/24/25/32\n");
	printf("<speaker channel count> : 1/2/4\n");
	printf("<mic channel count> : 1/2/4\n");

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

/* Read thread */
void *user_read(void *arg)
{
	int i;
	size_t transfer_length;
	int32_t *received_data;
	int cnt = 0;
	long r_limit = 0;
	int boundary_read;

	printf("Performing data read...\n");

	received_data = (int32_t *) malloc(read_length_words * sizeof(int32_t));

	while (r_limit < read_limit) {
		if (mode != EXTERNAL_LB_MASTER_SLAVE)
			transfer_length = read(fd_master, received_data, read_length_bytes);
		else
			transfer_length = read(fd_slave, received_data, read_length_bytes);
		printf("Bytes read: %zd\n", transfer_length);
		if ((r_limit + transfer_length) > read_limit) {
			boundary_read = (read_limit - r_limit);
			fwrite(received_data,boundary_read,1,fd_write_op);
		}
		else
			fwrite(received_data,BYTES_PER_WORD,read_length_words,fd_write_op);

		r_limit = r_limit + transfer_length;
	}

	free(received_data);

	return NULL;
}

int main(int argc, char **argv)
{
	long wav_samples;
	long no_words;
	long w_len;
	int32_t *wav_data;
	int32_t temp_data;
	int i;
	int mux;
	int arg = 1;
	int slave;
	pthread_t tid;
	int ret = 0;

	if (argc < 4) {
		help();
		exit(0);
	}

	printf("Reading operation mode...\n");
	mode = atoi(argv[arg++]);
	if (mode > CONFIG_PARAMS) {
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

	/* Operation mode : Configure I2S parameters */
	if (mode == CONFIG_PARAMS) {
		printf("Configuring I2S parameters\n");
		if (argc < 8) {
			help();
			exit(0);
		}
		params = (struct i2s_params *) malloc(sizeof(struct i2s_params));
		params->bit_clk = atoi(argv[arg++]);
		params->buffer_ms = atoi(argv[arg++]);
		params->bit_depth = atoi(argv[arg++]);
		params->spkr_channel_count = atoi(argv[arg++]);
		params->mic_channel_count = atoi(argv[arg++]);
		if (ioctl(fd_master, I2S_CONFIG_PARAMS, params) < 0) {
			printf("Failed to configure I2S parameters on target\n");
			exit(0);
		}
		exit(0);
	}

	/* Operation mode : Set I2S interface as master/slave */
	if (mode == SET_MUXMODE) {
		mux = atoi(argv[arg++]);
		printf("Setting muxmode\n");
		if (ioctl(fd_master, I2S_MUXMODE, mux) < 0) {
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

	printf("Opening o/p file...\n");
	fd_write_op = fopen(argv[arg++], "w");
	if (fd_write_op == NULL) {
		printf("Cannot open output file\n");
		help();
		exit(0);
	}

	/* Initializing to default macros and use the macros if no size specified by user*/
	read_length_bytes = READ_LENGTH_MB * 1024 * 1024;
	read_length_words = READ_LENGTH_WORDS;

	if (mode != NORMAL) {
		switch (mode) {
		/* Operation mode : Internal loopback */
		case INTERNAL_LB:
			if (argc < 5) {
				help();
				exit(0);
			}
			printf("Setting internal loopback operation \n");
			if (ioctl(fd_master, I2S_RESET) < 0) {
				printf("Failed to reset the hsi2s device\n");
				exit(0);
			}
			if (ioctl(fd_master, I2S_INTERNAL_LOOPBACK) < 0) {
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
			if (ioctl(fd_master, I2S_RESET) < 0) {
				printf("Failed to reset the hsi2s device\n");
				exit(0);
			}
			if (ioctl(fd_master, I2S_EXTERNAL_LOOPBACK) < 0) {
				printf("Failed to trigger external loopback\n");
				exit(0);
			}
			break;
		/* Operation mode : External loopback between master and slave interfaces */
		case EXTERNAL_LB_MASTER_SLAVE:
			printf("Setting external loopback on master/slave \n");
			if (ioctl(fd_master, I2S_RESET) < 0) {
				printf("Failed to reset the hsi2s master\n");
				exit(0);
			}
			if (ioctl(fd_slave, I2S_RESET) < 0) {
				printf("Failed to reset the hsi2s slave\n");
				exit(0);
			}
			if (ioctl(fd_master, I2S_MUXMODE, 0) < 0) {
				printf("Failed to set master mode\n");
				exit(0);
			}
			if (ioctl(fd_slave, I2S_MUXMODE, 1) < 0) {
				printf("Failed to set slave mode\n");
				exit(0);
			}
			if (ioctl(fd_master, I2S_SET_SLAVE, slave) < 0) {
				printf("Failed to set slave for the master\n");
				exit(0);
			}
			if (ioctl(fd_slave, I2S_MIC) < 0) {
				printf("Failed to configure mic\n");
				exit(0);
			}
			if (ioctl(fd_master, I2S_SPEAKER) < 0) {
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
		}
	} else {
		/* Operation mode : Normal mode data reception */
		printf("Setting normal mode \n");
		if (ioctl(fd_master, I2S_RESET) < 0) {
			printf("Failed to reset the hsi2s device\n");
			exit(0);
		}
		if (ioctl(fd_master, I2S_NORMAL_MODE) < 0) {
			printf("Failed to configure normal operation on the hsi2s device\n");
			exit(0);
		}

		read_limit = READ_LIMIT;

		if (arg < argc) {
			/* Use the size provided by the user */
			read_length_bytes = (atoi(argv[arg++]) * 1024 * 1024) / 2;
			read_length_words = read_length_bytes/BYTES_PER_WORD;
		}
	}

	printf("Creating read thread\n");
	if (pthread_create(&tid, NULL, user_read, NULL)!=0) {
		printf("Error creating reader thread\n");
	}

	/* Reading from input file only for loopback modes */
	if (mode != NORMAL) {
		printf("Reading i/p file...\n");
		wav_data = (int32_t *) malloc(no_words * sizeof(int32_t));

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

	printf("Joining threads\n");
	pthread_join(tid,NULL);
	printf("Threads joined \n");

	/* Disable transmission only for loopback modes */
	if (mode != NORMAL) {
		printf("Disabling Tx on read DMA channel\n");
		if (ioctl(fd_master, I2S_DEINIT_TX) < 0) {
			printf("Failed to stop Tx on hsi2s device\n");
			exit(0);
		}
	}

	printf("Closing Files \n");
	if (mode != NORMAL) {
		free(wav_data);
		fclose(fd_read_ip);
	}
	fclose(fd_write_op);
	if (mode == EXTERNAL_LB_MASTER_SLAVE)
		close(fd_slave);
	close(fd_master);

	return 0;
}
