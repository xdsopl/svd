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
	for (int m = 0; m < M; ++m) {
		for (int n = 0; n < N; ++n) {
			A[N*m+n] = 0;
			for (int k = 0; k < K; ++k)
				A[N*m+n] += U[K*m+k] * S[k] * VT[N*k+n];
		}
	}
}

int decode(struct vli_reader *vli, int *val, int num, int stride)
{
	for (int i = 0; i < num; ++i) {
		int ret = get_vli(vli);
		if (ret < 0)
			return ret;
		val[i*stride] = ret;
		if (ret && (ret = vli_get_bit(vli)) < 0)
			return ret;
		if (ret)
			val[i*stride] = -val[i*stride];
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
	struct vli_reader *vli = vli_reader(bits);
	int width = get_vli(vli);
	int height = get_vli(vli);
	if ((width|height) < 0)
		return 1;
	int quant[3];
	for (int chan = 0; chan < 3; ++chan)
		if ((quant[chan] = get_vli(vli)) < 0)
			return 1;
	int M = height, N = width;
	int K = M < N ? M : N;
	int size = M*K+K+K*N;
	int *Q = malloc(sizeof(int) * 3 * size);
	for (int i = 0; i < 3 * size; ++i)
		Q[i] = 0;
	for (int k = 0; k < K; ++k) {
		for (int chan = 0; chan < 3; ++chan) {
			int ret = get_vli(vli);
			if (ret < 0)
				goto end;
			Q[chan*size+M*K+k] = ret;
			if (decode(vli, Q+chan*size+k, M, K))
				goto end;
			if (decode(vli, Q+chan*size+M*K+K+N*k, N, 1))
				goto end;
		}
	}
end:
	delete_vli_reader(vli);
	close_reader(bits);
	struct image *image = new_image(argv[2], width, height);
	float *A = malloc(sizeof(float) * M * N);
	float *U = malloc(sizeof(float) * M * K);
	float *S = malloc(sizeof(float) * K);
	float *VT = malloc(sizeof(float) * K * N);
	for (int chan = 0; chan < 3; ++chan) {
		quantization(U, Q+chan*size, M*K, quant[chan]);
		quantization(S, Q+chan*size+M*K, K, quant[chan]);
		quantization(VT, Q+chan*size+M*K+K, K*N, quant[chan]);
		multiply(A, U, S, VT, M, K, N);
		copy(image->buffer+chan, A, M * N, 3);
	}
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

