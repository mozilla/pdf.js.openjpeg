/*
 * Copyright (c) 2026, Mozilla Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// Pack clamped bytes into c0. Output pixels use at most 4 bytes, so reading
// each block before writing preserves unread samples.
// pack_pixels_4 fills a NULL c3 with 255.

#include "pixels.h"

#ifdef __wasm_simd128__
#include <wasm_simd128.h>

#define BLOCK_SIZE 16

// Clamp 16 samples to [0, 255].
static inline v128_t load_block(const OPJ_INT32 *src) {
  const v128_t lo = wasm_i16x8_narrow_i32x4(wasm_v128_load(src),
                                            wasm_v128_load(src + 4));
  const v128_t hi = wasm_i16x8_narrow_i32x4(wasm_v128_load(src + 8),
                                            wasm_v128_load(src + 12));
  return wasm_u8x16_narrow_i16x8(lo, hi);
}
#endif

static inline OPJ_UINT8 clamp(OPJ_INT32 value) {
  return value < 0 ? 0 : value > 255 ? 255 : (OPJ_UINT8)value;
}

void pack_pixels_1(OPJ_INT32 *c0, OPJ_SIZE_T nb_pixels) {
  OPJ_UINT8 *dst = (OPJ_UINT8 *)c0;
  OPJ_SIZE_T i = 0;
#ifdef __wasm_simd128__
  for (; i + BLOCK_SIZE <= nb_pixels; i += BLOCK_SIZE) {
    wasm_v128_store(dst + i, load_block(c0 + i));
  }
#endif
  for (; i < nb_pixels; i++) {
    dst[i] = clamp(c0[i]);
  }
}

void pack_pixels_3(OPJ_INT32 *c0, const OPJ_INT32 *c1, const OPJ_INT32 *c2,
                   OPJ_SIZE_T nb_pixels) {
  OPJ_UINT8 *dst = (OPJ_UINT8 *)c0;
  OPJ_SIZE_T i = 0;
#ifdef __wasm_simd128__
  for (; i + BLOCK_SIZE <= nb_pixels; i += BLOCK_SIZE) {
    const v128_t r = load_block(c0 + i);
    const v128_t g = load_block(c1 + i);
    const v128_t b = load_block(c2 + i);
    const v128_t rg0 = wasm_i8x16_shuffle(r, g, 0, 16, 0, 1, 17, 0, 2, 18, 0,
                                          3, 19, 0, 4, 20, 0, 5);
    // Bytes 0..15: R0 G0 B0 ... R5.
    const v128_t out0 = wasm_i8x16_shuffle(rg0, b, 0, 1, 16, 3, 4, 17, 6, 7,
                                           18, 9, 10, 19, 12, 13, 20, 15);
    const v128_t rg1 = wasm_i8x16_shuffle(r, g, 21, 0, 6, 22, 0, 7, 23, 0, 8,
                                          24, 0, 9, 25, 0, 10, 26);
    // Bytes 16..31: G5 B5 R6 ... G10.
    const v128_t out1 = wasm_i8x16_shuffle(rg1, b, 0, 21, 2, 3, 22, 5, 6, 23,
                                           8, 9, 24, 11, 12, 25, 14, 15);
    const v128_t rg2 = wasm_i8x16_shuffle(r, g, 0, 11, 27, 0, 12, 28, 0, 13,
                                          29, 0, 14, 30, 0, 15, 31, 0);
    // Bytes 32..47: B10 R11 G11 ... B15.
    const v128_t out2 = wasm_i8x16_shuffle(rg2, b, 26, 1, 2, 27, 4, 5, 28, 7,
                                           8, 29, 10, 11, 30, 13, 14, 31);
    wasm_v128_store(dst + 3 * i, out0);
    wasm_v128_store(dst + 3 * i + 16, out1);
    wasm_v128_store(dst + 3 * i + 32, out2);
  }
#endif
  for (; i < nb_pixels; i++) {
    const OPJ_UINT8 r = clamp(c0[i]), g = clamp(c1[i]), b = clamp(c2[i]);
    dst[3 * i] = r;
    dst[3 * i + 1] = g;
    dst[3 * i + 2] = b;
  }
}

void pack_pixels_4(OPJ_INT32 *c0, const OPJ_INT32 *c1, const OPJ_INT32 *c2,
                   const OPJ_INT32 *c3, OPJ_SIZE_T nb_pixels) {
  OPJ_UINT8 *dst = (OPJ_UINT8 *)c0;
  OPJ_SIZE_T i = 0;
#ifdef __wasm_simd128__
  for (; i + BLOCK_SIZE <= nb_pixels; i += BLOCK_SIZE) {
    const v128_t r = load_block(c0 + i);
    const v128_t g = load_block(c1 + i);
    const v128_t b = load_block(c2 + i);
    const v128_t a = c3 ? load_block(c3 + i) : wasm_i8x16_splat(-1);
    const v128_t rg_lo = wasm_i8x16_shuffle(r, g, 0, 16, 1, 17, 2, 18, 3, 19,
                                            4, 20, 5, 21, 6, 22, 7, 23);
    const v128_t rg_hi = wasm_i8x16_shuffle(r, g, 8, 24, 9, 25, 10, 26, 11,
                                            27, 12, 28, 13, 29, 14, 30, 15, 31);
    const v128_t ba_lo = wasm_i8x16_shuffle(b, a, 0, 16, 1, 17, 2, 18, 3, 19,
                                            4, 20, 5, 21, 6, 22, 7, 23);
    const v128_t ba_hi = wasm_i8x16_shuffle(b, a, 8, 24, 9, 25, 10, 26, 11,
                                            27, 12, 28, 13, 29, 14, 30, 15, 31);
    wasm_v128_store(dst + 4 * i,
                    wasm_i16x8_shuffle(rg_lo, ba_lo, 0, 8, 1, 9, 2, 10, 3, 11));
    wasm_v128_store(dst + 4 * i + 16, wasm_i16x8_shuffle(rg_lo, ba_lo, 4, 12,
                                                         5, 13, 6, 14, 7, 15));
    wasm_v128_store(dst + 4 * i + 32,
                    wasm_i16x8_shuffle(rg_hi, ba_hi, 0, 8, 1, 9, 2, 10, 3, 11));
    wasm_v128_store(dst + 4 * i + 48, wasm_i16x8_shuffle(rg_hi, ba_hi, 4, 12,
                                                         5, 13, 6, 14, 7, 15));
  }
#endif
  for (; i < nb_pixels; i++) {
    const OPJ_UINT8 r = clamp(c0[i]), g = clamp(c1[i]), b = clamp(c2[i]);
    const OPJ_UINT8 a = c3 ? clamp(c3[i]) : 255;
    dst[4 * i] = r;
    dst[4 * i + 1] = g;
    dst[4 * i + 2] = b;
    dst[4 * i + 3] = a;
  }
}
