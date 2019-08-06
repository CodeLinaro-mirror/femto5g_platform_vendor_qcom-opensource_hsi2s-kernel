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
#define I2S_RESET _IOWR('i', 7, int)

#define BYTES_PER_WORD 4
#define READ_LENGTH_KB 4
#define READ_LENGTH_WORDS (READ_LENGTH_KB * 1024) / BYTES_PER_WORD
#define READ_LIMIT 1024*1024*1024
#define WRITE_LENGTH_KB 4
#define WRITE_LENGTH_WORDS (WRITE_LENGTH_KB * 1024) /BYTES_PER_WORD

enum operation_mode {
	NORMAL,
	INTERNAL_LB,
	EXTERNAL_LB_MASTER,
	EXTERNAL_LB_MASTER_SLAVE,
	SET_MUXMODE
};

int fd_master;
int fd_slave;
FILE *fd_read_ip;
FILE *fd_write_op;
long read_length_bytes;
long read_length_words;
long read_limit;
long write_length;
long write_length_words;
enum operation_mode mode;

void help()
{
	printf("Usage:\n");
	printf("hsi2s_test <operational mode> <master device file> [<muxmode>] [<slave device file>] [<output file>] [<input file>] [<size>]\n");
	printf("Arguments:\n");
	printf("<operational mode> :\n 0 - Normal mode\n 1 - Internal loopback\n 2 - External loopback on master\n 3 - External loopback on master-slave\n 4 - Set muxmode\n");
	printf("<device file> : /dev/hs0_i2s | /dev/hs1_i2s | /dev/hs2_i2s\n");
	printf("[<muxmode>] : 0 - Master 1 - Slave\n");
	printf("[<output file>] : To store the data read from the device file\n");
	printf("[<input file>] : To be provided only for loopback modes\n");
	printf("[<size>] : Read-write length (4KB by default)\n");
}

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

void *user_read(void *arg)
{
	int i;
	size_t transfer_length;
	int32_t received_data[read_length_words];
	int cnt = 0;
	long r_limit = 0;
	int boundary_read;

	printf("Performing data read...\n");

	while (r_limit < read_limit) {
		if (mode != EXTERNAL_LB_MASTER_SLAVE)
			transfer_length = read(fd_master, &received_data, read_length_bytes);
		else
			transfer_length = read(fd_slave, &received_data, read_length_bytes);
		printf("Bytes read: %zd\n", transfer_length);
		if ((r_limit + transfer_length) > read_limit) {
			boundary_read = (read_limit - r_limit);
			fwrite(&received_data,boundary_read,1,fd_write_op);
		}
		else
			fwrite(&received_data,BYTES_PER_WORD,read_length_words,fd_write_op);

		r_limit = r_limit + transfer_length;
	}

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
	if (mode > SET_MUXMODE) {
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

	if (mode == SET_MUXMODE) {
		mux = atoi(argv[arg++]);
		printf("Setting muxmode\n");
		if (ioctl(fd_master, I2S_MUXMODE, mux) < 0) {
			printf("Failed to set master/slave configuration on target\n");
			exit(0);
		}
		exit(0);
	}

	if (mode == EXTERNAL_LB_MASTER_SLAVE) {
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
	read_length_bytes = READ_LENGTH_KB * 1024;
	read_length_words = READ_LENGTH_WORDS;

	if (mode != NORMAL) {
		switch (mode) {
		case INTERNAL_LB:
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
		case EXTERNAL_LB_MASTER:
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
		write_length = WRITE_LENGTH_KB * 1024;
		write_length_words = WRITE_LENGTH_WORDS;

		if (arg < argc) {
			/* Use the size provided by the user */
			read_length_bytes = atoi(argv[arg++]) * 1024;
			write_length = read_length_bytes;
			write_length_words = write_length/BYTES_PER_WORD;
			read_length_words = read_length_bytes/BYTES_PER_WORD;

			printf("User Command Line\n");
			printf("read_length_bytes: %ld\n", read_length_bytes);
			printf("write_length: %ld\n", write_length);
			printf("write_length_words: %ld\n", write_length_words);
			printf("read_length_words: %ld\n", read_length_words);
		}
	} else {
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
			read_length_bytes = atoi(argv[arg++]) * 1024;
			read_length_words = read_length_bytes/BYTES_PER_WORD;

			printf("User Command Line\n");
			printf("read_length_bytes: %ld\n", read_length_bytes);
			printf("read_length_words: %ld\n", read_length_words);
		}
	}

	printf("Creating read thread\n");
	if (pthread_create(&tid, NULL, user_read, NULL)!=0) {
		printf("Error creating reader thread\n");
	}

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
