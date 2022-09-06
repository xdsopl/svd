CFLAGS = -std=c99 -W -Wall -O3 -D_GNU_SOURCE=1 -g -fsanitize=address
LDLIBS = -lm -llapack
RM = rm -f

all: svdenc svddec

test: svdenc svddec
	./svdenc input.ppm /dev/null
#	./svdenc input.ppm /dev/stdout | ./svddec /dev/stdin output.ppm

svdenc: src/encode.c
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@

svddec: src/decode.c
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@

clean:
	$(RM) svdenc svddec

