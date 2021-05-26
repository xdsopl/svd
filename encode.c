/*
Encoder for lossy image compression based on the singular value decomposition

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

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

void sgesdd_(char *jobz, int *m, int *n, float *a, int *lda,
	float *s, float *u, int *ldu, float *vt, int *ldvt,
	float *work, int *lwork, int *iwork, int *info);

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
	int M = width, N = height, K = M < N ? M : N;
	float *A = malloc(sizeof(float) * M * N);
	float *U = malloc(sizeof(float) * M * K);
	float *S = malloc(sizeof(float) * K);
	float *VT = malloc(sizeof(float) * K * N);
	int size = M*K+K+K*N;
	int *Q = malloc(sizeof(int) * 3 * size);
	int *iwork = malloc(sizeof(int) * 8 * K);
	int info = 0, lwork = -1;
	float query;
	sgesdd_("S", &M, &N, A, &M, S, U, &M, VT, &K, &query, &lwork, iwork, &info);
	if (info != 0) {
		fprintf(stderr, "Querying for work size failed: %d\n", info);
		return 1;
	}
	lwork = query;
	float *work = malloc(sizeof(float) * lwork);
	for (int chan = 0; chan < 3; ++chan) {
		copy(A, image->buffer+chan, M * N, 3);
		sgesdd_("S", &M, &N, A, &M, S, U, &M, VT, &K, work, &lwork, iwork, &info);
		if (info != 0)
			fprintf(stderr, "oh noes! %d\n", info);
		quantization(Q+chan*size, U, M*K, quant[chan]);
		quantization(Q+chan*size+M*K, S, K, quant[chan]);
		quantization(Q+chan*size+M*K+K, VT, K*N, quant[chan]);
	}
	free(A);
	free(U);
	free(S);
	free(VT);
	free(work);
	free(iwork);
	delete_image(image);
	for (int k = 0; k < K; ++k) {
		for (int chan = 0; chan < 3; ++chan) {
			if (put_vli(vli, Q[chan*size+M*K+k]))
				goto end;
			if (encode(vli, Q+chan*size+M*k, M, 1))
				goto end;
			if (encode(vli, Q+chan*size+M*K+K+k, N, K))
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

