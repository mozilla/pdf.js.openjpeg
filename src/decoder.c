/*
 * Copyright (c) 2024, Mozilla Foundation
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
 * ARISING IN ANY WAY buffer OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// clang-format off
#include "emscripten.h"
#include "openjpeg.h"
#include "color.h"
#include "convert.h"
// clang-format on

#define JP2_RFC3745_MAGIC_0 0x0C000000
#define JP2_RFC3745_MAGIC_1 0x2020506A
#define JP2_RFC3745_MAGIC_2 0x0A870A0D
#define JP2_MAGIC 0x0A870A0D
#define J2K_CODESTREAM_MAGIC 0x51FF4FFF

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// 1 MiB pixels should be enough in most of the cases.
#define BUFFER_PIXELS_NUMBER 1048576

// #define PDFJS_DEBUG

extern void allocateImageData(OPJ_SIZE_T);
extern void jsPrintWarning(const char *);
extern void setImageData(OPJ_UINT8 *, OPJ_SIZE_T, OPJ_SIZE_T);
extern void storeErrorMessage(const char *);

static void error_callback(const char *msg, void *client_data) {
  (void)client_data;
  storeErrorMessage(msg);
}
static void warning_callback(const char *msg, void *client_data) {
  (void)client_data;
  jsPrintWarning(msg);
}

static void quiet_callback(const char *msg, void *client_data) {
  (void)msg;
  (void)client_data;
}

inline void gray_to_rgba(opj_image_t *image, OPJ_UINT8 *buffer,
                         OPJ_SIZE_T buffer_pixels, OPJ_SIZE_T offset) {
  for (OPJ_SIZE_T i = 0; i < buffer_pixels; i++) {
    buffer[4 * i] = buffer[4 * i + 1] = buffer[4 * i + 2] =
        image->comps[0].data[offset + i];
    buffer[4 * i + 3] = 255;
  }
  setImageData(buffer, buffer_pixels * 4, offset * 4);
}

inline void graya_to_rgba(opj_image_t *image, OPJ_UINT8 *buffer,
                          OPJ_SIZE_T buffer_pixels, OPJ_SIZE_T offset) {
  for (OPJ_SIZE_T i = 0; i < buffer_pixels; i++) {
    buffer[4 * i] = buffer[4 * i + 1] = buffer[4 * i + 2] =
        image->comps[0].data[offset + i];
    buffer[4 * i + 3] = image->comps[1].data[offset + i];
  }
  setImageData(buffer, buffer_pixels * 4, offset * 4);
}

inline void rgb_to_rgba(opj_image_t *image, OPJ_UINT8 *buffer,
                        OPJ_SIZE_T buffer_pixels, OPJ_SIZE_T offset) {
  for (OPJ_SIZE_T i = 0; i < buffer_pixels; i++) {
    buffer[4 * i] = image->comps[0].data[offset + i];
    buffer[4 * i + 1] = image->comps[1].data[offset + i];
    buffer[4 * i + 2] = image->comps[2].data[offset + i];
    buffer[4 * i + 3] = 255;
  }
  setImageData(buffer, buffer_pixels * 4, offset * 4);
}

inline void copy_pixels(OPJ_SIZE_T num_comps, opj_image_t *image,
                        OPJ_UINT8 *buffer, OPJ_SIZE_T buffer_pixels,
                        OPJ_SIZE_T offset) {
  for (OPJ_SIZE_T i = 0; i < buffer_pixels; i++) {
    for (OPJ_SIZE_T j = 0; j < num_comps; j++) {
      buffer[num_comps * i + j] = image->comps[j].data[offset + i];
    }
  }
  setImageData(buffer, buffer_pixels * num_comps, offset * num_comps);
}

int EMSCRIPTEN_KEEPALIVE jp2_decode(OPJ_UINT8 *data, OPJ_SIZE_T data_size,
                                    OPJ_UINT32 pdf_numcomps,
                                    OPJ_BOOL pdf_is_indexed_colormap,
                                    OPJ_BOOL pdf_smaks_in_data) {
  opj_dparameters_t parameters;
  opj_codec_t *l_codec = NULL;
  opj_image_t *image = NULL;
  opj_stream_t *l_stream = NULL;

  OPJ_UINT32 *int32data = (OPJ_UINT32 *)data;
  if (int32data[0] == JP2_MAGIC || (int32data[0] == JP2_RFC3745_MAGIC_0 &&
                                    int32data[1] == JP2_RFC3745_MAGIC_1 &&
                                    int32data[2] == JP2_RFC3745_MAGIC_2)) {
    l_codec = opj_create_decompress(OPJ_CODEC_JP2);
  } else if (int32data[0] == J2K_CODESTREAM_MAGIC) {
    l_codec = opj_create_decompress(OPJ_CODEC_J2K);
  } else {
    storeErrorMessage("Unknown format");
    return 1;
  }

  opj_set_info_handler(l_codec, quiet_callback, 00);
  opj_set_warning_handler(l_codec, warning_callback, 00);
  opj_set_error_handler(l_codec, error_callback, 00);

  opj_set_default_decoder_parameters(&parameters);
  if (pdf_is_indexed_colormap) {
    parameters.flags |= OPJ_DPARAMETERS_IGNORE_PCLR_CMAP_CDEF_FLAG;
  }

  // set stream
  opj_buffer_info_t buffer_info;
  buffer_info.buf = data;
  buffer_info.cur = data;
  buffer_info.len = data_size;
  l_stream = opj_stream_create_buffer_stream(&buffer_info, OPJ_TRUE);

  /* Setup the decoder decoding parameters using user parameters */
  if (unlikely(!opj_setup_decoder(l_codec, &parameters))) {
    storeErrorMessage("Failed to setup the decoder");
    opj_stream_destroy(l_stream);
    opj_destroy_codec(l_codec);
    return 1;
  }

  /* Read the main header of the codestream and if necessary the JP2 boxes*/
  if (unlikely(!opj_read_header(l_stream, l_codec, &image))) {
    storeErrorMessage("Failed to read the header");
    opj_stream_destroy(l_stream);
    opj_destroy_codec(l_codec);
    opj_image_destroy(image);
    return 1;
  }

#ifdef PDFJS_DEBUG
  printf("Arguments: numcomps: %d, is_indexed: %d, smask_in_data: %d\n",
         pdf_numcomps, pdf_is_indexed_colormap, pdf_smaks_in_data);
  printf("image X %d\n", image->x1);
  printf("image Y %d\n", image->y1);
  printf("image numcomps %d\n", image->numcomps);
#endif

  /* decode the image */
  if (unlikely(!opj_decode(l_codec, l_stream, image) ||
               !opj_end_decompress(l_codec, l_stream))) {
    storeErrorMessage("Failed to decode the image");
    opj_destroy_codec(l_codec);
    opj_stream_destroy(l_stream);
    opj_image_destroy(image);
    return 1;
  }

#ifdef PDFJS_DEBUG
  printf("image numcomps %d\n", image->numcomps);
  switch (image->color_space) {
  case OPJ_CLRSPC_UNKNOWN:
    printf("image colorspace unknown\n");
    break;
  case OPJ_CLRSPC_UNSPECIFIED:
    printf("image colorspace unspecified\n");
    break;
  case OPJ_CLRSPC_SRGB:
    printf("image colorspace sRGB\n");
    break;
  case OPJ_CLRSPC_GRAY:
    printf("image colorspace gray\n");
    break;
  case OPJ_CLRSPC_SYCC:
    printf("image colorspace sycc\n");
    break;
  case OPJ_CLRSPC_EYCC:
    printf("image colorspace eycc\n");
    break;
  case OPJ_CLRSPC_CMYK:
    printf("image colorspace cmyk\n");
    break;
  }
  printf("prec=%d, bpp=%d, sgnd=%d w=%d h=%d\n", image->comps[0].prec,
         image->comps[0].bpp, image->comps[0].sgnd, image->comps[0].w,
         image->comps[0].h);
  printf("prec=%d, bpp=%d, sgnd=%d w=%d h=%d\n", image->comps[1].prec,
         image->comps[1].bpp, image->comps[1].sgnd, image->comps[1].w,
         image->comps[1].h);
  printf("prec=%d, bpp=%d, sgnd=%d w=%d h=%d\n", image->comps[2].prec,
         image->comps[2].bpp, image->comps[2].sgnd, image->comps[2].w,
         image->comps[2].h);
#endif

  opj_stream_destroy(l_stream);
  opj_destroy_codec(l_codec);

  if (image->icc_profile_buf) {
    // Avoid a memory leak (see opj_decompress.c).
    free(image->icc_profile_buf);
    image->icc_profile_buf = NULL;
    image->icc_profile_len = 0;
  }

  OPJ_UINT32 numcomps = image->numcomps;
  OPJ_BOOL convert_to_rgba = OPJ_FALSE;

  if (pdf_numcomps <= 0) {
    // The pdf doesn't specify the color space, so we use the one from the image
    // and convert it to RGBA.
    numcomps = image->numcomps;
    if (!(pdf_smaks_in_data && numcomps == 4)) {
      if (image->color_space != OPJ_CLRSPC_SYCC && numcomps == 3 &&
          image->comps[0].dx == image->comps[0].dy && image->comps[1].dx != 1) {
        image->color_space = OPJ_CLRSPC_SYCC;
      } else if (numcomps <= 2) {
        image->color_space = OPJ_CLRSPC_GRAY;
      }

      if (image->color_space == OPJ_CLRSPC_SYCC) {
        color_sycc_to_rgb(image);
      } else if (image->color_space == OPJ_CLRSPC_CMYK) {
        color_cmyk_to_rgb(image);
      } else if (image->color_space == OPJ_CLRSPC_EYCC) {
        color_esycc_to_rgb(image);
      }
      convert_to_rgba = OPJ_TRUE;
    }
  } else if (numcomps > pdf_numcomps) {
    numcomps = pdf_numcomps;
  }

  if (!pdf_is_indexed_colormap) {
    for (int i = 0; i < numcomps; i++) {
      scale_component(&image->comps[i], 8);
    }
  }

  if (convert_to_rgba) {
    numcomps = 4;
  }

  OPJ_SIZE_T nb_pixels = image->x1 * image->y1;
  OPJ_SIZE_T image_size = nb_pixels * numcomps;

  OPJ_SIZE_T buffer_size;
  OPJ_SIZE_T buffer_pixels_base = BUFFER_PIXELS_NUMBER;
  OPJ_UINT8 *buffer = NULL;

  while (buffer == NULL && buffer_pixels_base > 1024) {
    OPJ_SIZE_T max_size = buffer_pixels_base * numcomps;
    buffer_size = image_size > max_size ? max_size : image_size;
    buffer = (OPJ_UINT8 *)malloc(buffer_size * sizeof(OPJ_UINT8));
    buffer_pixels_base /= 2;
  }

  if (buffer == NULL) {
    storeErrorMessage("Failed to allocate memory for the image");
    opj_image_destroy(image);
    return 1;
  }

  if (buffer_size != image_size) {
    jsPrintWarning("Reducing the size of the buffer");
  }

  OPJ_SIZE_T buffer_pixels = buffer_size / numcomps;
  OPJ_SIZE_T i;
  OPJ_SIZE_T n = nb_pixels / buffer_pixels;
  OPJ_SIZE_T r = nb_pixels % buffer_pixels;

  allocateImageData(image_size);

  if (convert_to_rgba) {
    if (image->color_space == OPJ_CLRSPC_GRAY) {
      if (image->numcomps == 1) {
        for (i = 0; i < n; i++) {
          gray_to_rgba(image, buffer, buffer_pixels, i * buffer_pixels);
        }
        if (r) {
          gray_to_rgba(image, buffer, r, i * buffer_pixels);
        }
      } else if (pdf_smaks_in_data) {
        for (i = 0; i < n; i++) {
          graya_to_rgba(image, buffer, buffer_pixels, i * buffer_pixels);
        }
        if (r) {
          graya_to_rgba(image, buffer, r, i * buffer_pixels);
        }
      }
    } else {
      for (i = 0; i < n; i++) {
        rgb_to_rgba(image, buffer, buffer_pixels, i * buffer_pixels);
      }
      if (r) {
        rgb_to_rgba(image, buffer, r, i * buffer_pixels);
      }
    }
  } else {
    for (i = 0; i < n; i++) {
      copy_pixels(numcomps, image, buffer, buffer_pixels, i * buffer_pixels);
    }
    if (r) {
      copy_pixels(numcomps, image, buffer, r, i * buffer_pixels);
    }
  }

  opj_image_destroy(image);
  free(buffer);

  return 0;
}
