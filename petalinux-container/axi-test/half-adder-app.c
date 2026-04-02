/*
* Copyright (C) 2013-2022  Xilinx, Inc.  All rights reserved.
* Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge,
* publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in this
* Software without prior written authorization from Xilinx.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define UIO_DEVICE "/dev/uio4"
#define UIO_MAP_SIZE_PATH "/sys/class/uio/uio4/maps/map0/size"

#define OPERAND_A_OFFSET 0x00
#define OPERAND_B_OFFSET 0x04
#define SUM_OFFSET 	 0x08

long get_map_size() {
	FILE *fp;
	long size;

	fp = fopen(UIO_MAP_SIZE_PATH, "r");
	if (fp == NULL) {
		perror("Failed to open UIO map size file");
		return -1;
	}

	if (fscanf(fp, "%lx", &size) != 1) {
		fprintf(stderr, "Failed to read map size from UIO file.\n");
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return size;
}

int main(int argc, char *argv[])
{
	int uio_fd;
	void *uio_ptr;
	volatile uint32_t *operand_a_ptr;
	volatile uint32_t *operand_b_ptr;
	volatile uint32_t *sum_ptr;
	long map_size;

	uint32_t operand_a, operand_b, sum_result;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <operand_a> <operand_b>\n", argv[0]);
		return 1;
	}

	operand_a = (uint32_t)strtoul(argv[1], NULL, 10);
	operand_b = (uint32_t)strtoul(argv[2], NULL, 10);

	printf("--- Initializing UIO driver for Half-Adder ---\n");

	map_size = get_map_size();
	if (map_size < 0) {
		return 1;
	}
	printf("Detected UIO map size: 0x%lX bytes\n", map_size);

	uio_fd = open(UIO_DEVICE, O_RDWR);
	if (uio_fd < 0) {
		perror("Error opening UIO device");
		return 1;
	}

	uio_ptr = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, uio_fd, 0);
	if (uio_ptr == MAP_FAILED) {
		perror("Error mapping UIO memory");
		close(uio_fd);
		return 1;
	}

	operand_a_ptr = (uint32_t *)(uio_ptr + OPERAND_A_OFFSET);
	operand_b_ptr = (uint32_t *)(uio_ptr + OPERAND_B_OFFSET);
	sum_ptr       = (uint32_t *)(uio_ptr + SUM_OFFSET);

	printf("--- Interacting with the Half-Adder IP ---\n");

	printf("Writing Operand A = %u\n", operand_a);
	*operand_a_ptr = operand_a;

	printf("Writing Operand B = %u\n", operand_b);
	*operand_b_ptr = operand_b;

	usleep(10000);

	sum_result = *sum_ptr;
	printf("Read Sum = %u\n", sum_result);
	printf("-------------------------------\n");

	uint32_t expected_sum = operand_a + operand_b;
	printf("Software-calculated sum: %u\n", expected_sum);

	if (sum_result == expected_sum) {
		printf("SUCCESS: Hardware result matches software calculation.\n");
	} else {
		printf("FAILURE: Hardware result does NOT match software calculation.\n");
	}

	if (munmap(uio_ptr, map_size) != 0) {
		perror("Error unmapping memory");
	}
	close(uio_fd);

	return 0;
}
