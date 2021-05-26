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

int encode(struct vli_writer *vli, int *val, int num, int stride)
{
	for (int i = 0; i < num; ++i) {
		int ret = put_vli(vli, abs(val[i*stride]));
		if (ret)
			return ret;
		if (val[i*stride] && (ret = vli_put_bit(vli, val[i*stride] < 0)))
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
	int quant[3] = { 12, 10, 10 };
	if (argc >= 6)
		for (int chan = 0; chan < 3; ++chan)
			quant[chan] = atoi(argv[3+chan]);
	int capacity = 1 << 23;
	if (argc >= 7)
		capacity = atoi(argv[6]);
	struct bits_writer *bits = bits_writer(argv[2], capacity);
	if (!bits)
		return 1;
	struct vli_writer *vli = vli_writer(bits);
	put_vli(vli, width);
	put_vli(vli, height);
	for (int chan = 0; chan < 3; ++chan)
		put_vli(vli, quant[chan]);
	int meta_data = bits_count(bits);
	fprintf(stderr, "%d bits for meta data\n", meta_data);
	ycbcr_image(image);
	for (int i = 0; i < width * height; ++i)
		image->buffer[3*i] -= 0.5f;
	int M = height, N = width;
	int K = M < N ? M : N;
	float *A = malloc(sizeof(float) * M * N);
	float *U = malloc(sizeof(float) * M * K);
	float *S = malloc(sizeof(float) * K);
	float *VT = malloc(sizeof(float) * K * N);
	int size = M*K+K+K*N;
	int *Q = malloc(sizeof(int) * 3 * size);
	for (int chan = 0; chan < 3; ++chan) {
		copy(A, image->buffer+chan, M * N, 3);
		int ret = LAPACKE_sgesdd(LAPACK_ROW_MAJOR, 'S', M, N, A, N, S, U, K, VT, N);
		if (ret > 0)
			fprintf(stderr, "oh noes!\n");
		quantization(Q+chan*size, U, M*K, quant[chan]);
		quantization(Q+chan*size+M*K, S, K, quant[chan]);
		quantization(Q+chan*size+M*K+K, VT, K*N, quant[chan]);
	}
	delete_image(image);
	free(A);
	free(U);
	free(S);
	free(VT);
	for (int k = 0; k < K; ++k) {
		for (int chan = 0; chan < 3; ++chan) {
			if (put_vli(vli, Q[chan*size+M*K+k]))
				goto end;
			if (encode(vli, Q+chan*size+k, M, K))
				goto end;
			if (encode(vli, Q+chan*size+M*K+K+N*k, N, 1))
				goto end;
		}
	}
end:
	free(Q);
	delete_vli_writer(vli);
	int cnt = bits_count(bits);
	int bytes = (cnt + 7) / 8;
	int kib = (bytes + 512) / 1024;
	fprintf(stderr, "%d bits (%d KiB) encoded\n", cnt, kib);
	close_writer(bits);
	return 0;
}

