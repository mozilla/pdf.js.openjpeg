/* Copyright 2024 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

// #define PDFJS_DEBUG

extern void jsPrintWarning(const char *);
extern void setImageData(OPJ_UINT8 *, OPJ_SIZE_T);
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
  OPJ_UINT8 *out = (OPJ_UINT8 *)malloc(image_size);

  if (convert_to_rgba) {
    if (image->color_space == OPJ_CLRSPC_GRAY) {
      if (image->numcomps == 1) {
        for (OPJ_SIZE_T i = 0; i < nb_pixels; i++) {
          out[4 * i] = out[4 * i + 1] = out[4 * i + 2] = image->comps[0].data[i];
          out[4 * i + 3] = 255;
        }
      } else if (pdf_smaks_in_data) {
        OPJ_INT32 *gray = image->comps[0].data;
        OPJ_INT32 *alpha = image->comps[1].data;
        for (OPJ_SIZE_T i = 0; i < nb_pixels; i++) {
          out[4 * i] = out[4 * i + 1] = out[4 * i + 2] = gray[i];
          out[4 * i + 3] = alpha[i];
        }
      }
    } else {
      OPJ_INT32 *red = image->comps[0].data;
      OPJ_INT32 *green = image->comps[1].data;
      OPJ_INT32 *blue = image->comps[2].data;
      for (OPJ_SIZE_T i = 0; i < nb_pixels; i++) {
        out[4 * i] = red[i];
        out[4 * i + 1] = green[i];
        out[4 * i + 2] = blue[i];
        out[4 * i + 3] = 255;
      }
    }
  } else {
    switch (numcomps) {
    case 1:
      for (OPJ_SIZE_T i = 0; i < nb_pixels; i++) {
        out[i] = image->comps[0].data[i];
      }
      break;
    case 3: {
      OPJ_INT32 *red = image->comps[0].data;
      OPJ_INT32 *green = image->comps[1].data;
      OPJ_INT32 *blue = image->comps[2].data;
      for (OPJ_SIZE_T i = 0; i < nb_pixels; i++) {
        out[3 * i] = red[i];
        out[3 * i + 1] = green[i];
        out[3 * i + 2] = blue[i];
      }
      break;
    }
    case 4: {
      OPJ_INT32 *red = image->comps[0].data;
      OPJ_INT32 *green = image->comps[1].data;
      OPJ_INT32 *blue = image->comps[2].data;
      OPJ_INT32 *alpha = image->comps[3].data;
      for (OPJ_SIZE_T i = 0; i < nb_pixels; i++) {
        out[4 * i] = red[i];
        out[4 * i + 1] = green[i];
        out[4 * i + 2] = blue[i];
        out[4 * i + 3] = alpha[i];
      }
    }
    }
  }

  if (image->icc_profile_buf) {
    // Avoid a memory leak (see opj_decompress.c).
    free(image->icc_profile_buf);
    image->icc_profile_buf = NULL;
    image->icc_profile_len = 0;
  }

  opj_image_destroy(image);
  setImageData(out, image_size);
  free(out);

  return 0;
}
