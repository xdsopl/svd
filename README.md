### Playing with lossy image compression based on the singular value decomposition

Quick start:

Encode [smpte.ppm](smpte.ppm) [PNM](https://en.wikipedia.org/wiki/Netpbm) picture file to ```encoded.svd```:

```
./encode smpte.ppm encoded.svd
```

Decode ```encoded.svd``` file to ```decoded.ppm``` picture file:

```
./decode encoded.svd decoded.ppm
```

Watch ```decoded.ppm``` picture file in [feh](https://feh.finalrewind.org/):

```
feh decoded.ppm
```

### Adjusting quantization

Use quantization values of seven for luminance (Y'), six and seven for chrominance (Cb and Cr) instead of the default ```12 10 10``` values:

```
./encode smpte.ppm encoded.svd 7 6 5
```

### Limited storage capacity

Use up to ```65536``` bits of space and discard quality bits, if necessary, to stay below ```65536``` bits:

```
./encode smpte.ppm encoded.svd 12 10 10 65536
```

### Reading

[Singular value decomposition](https://en.wikipedia.org/wiki/Singular_value_decomposition)

