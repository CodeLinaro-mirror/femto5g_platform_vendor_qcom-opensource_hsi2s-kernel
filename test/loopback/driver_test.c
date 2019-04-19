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

#define BYTES_PER_WORD 4
#define RECEIVED_DATA_SIZE_BYTES 4096
#define RECEIVED_DATA_SIZE RECEIVED_DATA_SIZE_BYTES/BYTES_PER_WORD
#define READ_LIMIT 1024*1024*1024
#define WRITE_LIMIT 4096
#define WRITE_LIMIT_WORDS WRITE_LIMIT/BYTES_PER_WORD

int fd;
FILE *fd_read_ip;
FILE *fd_write_op;
long received_data_size_bytes;
long received_data_size;
long read_limit;
long write_limit;
long write_limit_words;


void help()
{
	printf("Usage:\n");
	printf("hsi2s_test <device file> <output file> <mode> [<input file>] [<size>]\n");
	printf("<device file> : /dev/hs0_i2s | /dev/hs1_i2s\n");
	printf("<output file> : To store the data read from the device file\n");
	printf("<mode> : 0 - Internal loopback 1 - Normal mode\n");
	printf("Optional arguments:\n");
	printf("<input file> : To be provided only for internal loopback\n");
	printf("<size> : Number of bytes read at a time (4096 by default)\n");
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
	int32_t received_data[received_data_size];
	int cnt = 0;
	long r_limit = 0;

	printf("Performing sample read...\n");

	while (r_limit < read_limit) {
		printf("Reading %lu bytes...\n", received_data_size_bytes);
		transfer_length = read(fd, &received_data, received_data_size_bytes);
		printf("Bytes read: %zd\n", transfer_length);
		r_limit = r_limit + transfer_length;
		printf("Writing to o/p file\n");
		fwrite(&received_data,BYTES_PER_WORD,received_data_size,fd_write_op);
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
	int mode;
	pthread_t tid;

	if (argc < 4) {
		help();
		exit(0);
	}

	printf("Opening i2s device file...\n");
	fd = open(argv[1], O_RDWR);
	if(fd < 0) {
		printf("Cannot open device file\n");
		help();
		exit(0);
	}

	printf("Opening o/p file...\n");
	fd_write_op = fopen(argv[2], "w");
	if (fd_write_op == NULL) {
		printf("Cannot open output file\n");
		help();
		exit(0);
	}

	printf("Reading operation mode...\n");
	mode = atoi(argv[3]);

	/* Initializing to default macros and use the macros if no size specified by user*/
	received_data_size_bytes = RECEIVED_DATA_SIZE_BYTES;
	received_data_size = RECEIVED_DATA_SIZE;

	if (!mode) {
		printf("Setting internal loopback operation \n");
		ioctl(fd, I2S_INTERNAL_LOOPBACK);

		printf("Opening i/p file...\n");
		fd_read_ip = fopen(argv[4], "r");
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

		write_limit = WRITE_LIMIT;
		write_limit_words = WRITE_LIMIT_WORDS;

		if (argc > 5) {
			/* Use the size provided by the user */
			received_data_size_bytes = atoi(argv[5]);
			write_limit = received_data_size_bytes;
			write_limit_words = write_limit/BYTES_PER_WORD;
			received_data_size = received_data_size_bytes/BYTES_PER_WORD;

			printf("User Command Line\n");
			printf("received_data_size_bytes: %ld\n", received_data_size_bytes);
			printf("write_limit: %ld\n", write_limit);
			printf("write_limit_words: %ld\n", write_limit_words);
			printf("received_data_size: %ld\n", received_data_size);
		}
	}
	else {
		printf("Setting normal mode \n");
		ioctl(fd, I2S_NORMAL_MODE);

		read_limit = READ_LIMIT;

		if (argc > 4) {
			/* Use the size provided by the user */
			received_data_size_bytes = atoi(argv[4]);
			received_data_size = received_data_size_bytes/BYTES_PER_WORD;

			printf("User Command Line\n");
			printf("received_data_size_bytes: %ld\n", received_data_size_bytes);
			printf("received_data_size: %ld\n", received_data_size);
		}
	}

	printf("Creating read thread\n");
	if (pthread_create(&tid, NULL, user_read, NULL)!=0) {
		printf("Error creating reader thread\n");
	}

	if (!mode) {
		printf("Reading i/p file...\n");
		wav_data = (int32_t *) malloc(no_words * sizeof(int32_t));

		/* Copy data from wav file into memory */
		for (i = 0; i < no_words; i++) {
			fread(&temp_data, BYTES_PER_WORD, 1,fd_read_ip);
			*(wav_data + i) = (int32_t)temp_data;
		}

		w_len = wav_samples;
		i = 0;

		while (w_len > write_limit) {
			printf("w_len: %ld\n",w_len);
			printf("i: %d\n",i);
			printf("Writing to hsi2s device\n");
			write(fd, wav_data+(i*write_limit_words), write_limit);
			w_len = w_len - write_limit;
			i++;
		}
		write(fd, wav_data+(i*write_limit_words), w_len);
	}

	printf("Joining threads\n");
	pthread_join(tid,NULL);
	printf("Threads joined \n");

	printf("Closing Files \n");
	if (!mode) {
		free(wav_data);
		fclose(fd_read_ip);
	}
	fclose(fd_write_op);
	close(fd);

	return 0;
}
