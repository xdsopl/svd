/*
Decoder for lossy image compression based on the singular value decomposition

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "vli.h"
#include "bits.h"

void quantization(float *output, int *input, int num, int quant)
{
	float factor = 1 << quant;
	for (int i = 0; i < num; ++i)
		output[i] = input[i] / factor;
}

void copy(float *output, float *input, int pixels, int stride)
{
	for (int i = 0; i < pixels; ++i)
		output[i*stride] = input[i];
}

void multiply(float *A, float *U, float *S, float *VT, int M, int K, int N)
{
	if (M == K) {
		for (int m = 0; m < M; ++m)
			for (int k = 0; k < K; ++k)
				U[K*m+k] *= S[k];
	} else {
		for (int k = 0; k < K; ++k)
			for (int n = 0; n < N; ++n)
				VT[N*k+n] *= S[k];
	}
	for (int m = 0; m < M; ++m) {
		for (int n = 0; n < N; ++n) {
			A[N*m+n] = 0;
			for (int k = 0; k < K; ++k)
				A[N*m+n] += U[K*m+k] * VT[N*k+n];
		}
	}
}

int decode(struct bits_reader *bits, int *val, int num)
{
	for (int i = 0; i < num; ++i) {
		int ret = get_vli(bits);
		if (ret < 0)
			return ret;
		val[i] = ret;
		if (ret && (ret = get_bit(bits)) < 0)
			return ret;
		if (ret)
			val[i] = -val[i];
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s input.svd output.ppm\n", argv[0]);
		return 1;
	}
	struct bits_reader *bits = bits_reader(argv[1]);
	if (!bits)
		return 1;
	int width = get_vli(bits);
	int height = get_vli(bits);
	if ((width|height) < 0)
		return 1;
	int quant[3];
	for (int chan = 0; chan < 3; ++chan)
		if ((quant[chan] = get_vli(bits)) < 0)
			return 1;
	struct image *image = new_image(argv[2], width, height);
	int M = height, N = width;
	int K = M < N ? M : N;
	int L = M > N ? M : N;
	float *A = malloc(sizeof(float) * M * N);
	float *U = malloc(sizeof(float) * M * K);
	float *S = malloc(sizeof(float) * K);
	float *VT = malloc(sizeof(float) * K * N);
	int *Q = malloc(sizeof(int) * L * L);
	for (int chan = 0; chan < 3; ++chan) {
		decode(bits, Q, M * K);
		quantization(U, Q, M * K, quant[chan]);
		decode(bits, Q, K);
		quantization(S, Q, K, quant[chan]);
		decode(bits, Q, K * N);
		quantization(VT, Q, K * N, quant[chan]);
		multiply(A, U, S, VT, M, K, N);
		copy(image->buffer+chan, A, M * N, 3);
	}
	close_reader(bits);
	free(A);
	free(U);
	free(S);
	free(VT);
	free(Q);
	for (int i = 0; i < width * height; ++i)
		image->buffer[3*i] += 0.5f;
	rgb_image(image);
	if (!write_ppm(image))
		return 1;
	delete_image(image);
	return 0;
}

