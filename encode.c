/*
Encoder for lossy image compression based on the singular value decomposition

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include <lapacke.h>
#include "ppm.h"
#include "vli.h"
#include "bits.h"

void quantization(int *output, float *input, int num, int quant)
{
	float factor = 1 << quant;
	for (int i = 0; i < num; ++i)
		output[i] = nearbyintf(factor * input[i]);
}

void copy(float *output, float *input, int pixels, int stride)
{
	for (int i = 0; i < pixels; ++i)
		output[i] = input[i*stride];
}

int encode(struct bits_writer *bits, int *val, int num)
{
	for (int i = 0; i < num; ++i) {
		int ret = put_vli(bits, abs(val[i]));
		if (ret)
			return ret;
		if (val[i] && (ret = put_bit(bits, val[i] < 0)))
			return ret;
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3 && argc != 6 && argc != 7) {
		fprintf(stderr, "usage: %s input.ppm output.svd [Q0 Q1 Q2] [CAPACITY]\n", argv[0]);
		return 1;
	}
	struct image *image = read_ppm(argv[1]);
	if (!image)
		return 1;
	int width = image->width;
	int height = image->height;
	int quant[3] = { 7, 5, 5 };
	if (argc >= 6)
		for (int chan = 0; chan < 3; ++chan)
			quant[chan] = atoi(argv[3+chan]);
	int capacity = 1 << 23;
	if (argc >= 7)
		capacity = atoi(argv[6]);
	struct bits_writer *bits = bits_writer(argv[2], capacity);
	if (!bits)
		return 1;
	put_vli(bits, width);
	put_vli(bits, height);
	for (int chan = 0; chan < 3; ++chan)
		put_vli(bits, quant[chan]);
	int meta_data = bits_count(bits);
	fprintf(stderr, "%d bits for meta data\n", meta_data);
	ycbcr_image(image);
	for (int i = 0; i < width * height; ++i)
		image->buffer[3*i] -= 0.5f;
	int M = height, N = width;
	int K = M < N ? M : N;
	int L = M > N ? M : N;
	float *A = malloc(sizeof(float) * M * N);
	float *U = malloc(sizeof(float) * M * M);
	float *S = malloc(sizeof(float) * K);
	float *VT = malloc(sizeof(float) * N * N);
	int *Q = malloc(sizeof(int) * L * L);
	for (int chan = 0; chan < 3; ++chan) {
		copy(A, image->buffer+chan, M * N, 3);
		int ret = LAPACKE_sgesdd(LAPACK_ROW_MAJOR, 'A', M, N, A, N, S, U, M, VT, N);
		if (ret > 0)
			fprintf(stderr, "oh noes!\n");
		quantization(Q, U, M * M, quant[chan]);
		encode(bits, Q, M * M);
		quantization(Q, S, K, quant[chan]);
		encode(bits, Q, K);
		quantization(Q, VT, N * N, quant[chan]);
		encode(bits, Q, N * N);
	}
	delete_image(image);
	free(A);
	free(U);
	free(S);
	free(VT);
	free(Q);
	int cnt = bits_count(bits);
	int bytes = (cnt + 7) / 8;
	int kib = (bytes + 512) / 1024;
	fprintf(stderr, "%d bits (%d KiB) encoded\n", cnt, kib);
	close_writer(bits);
	return 0;
}

