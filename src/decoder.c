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

#include "emscripten.h"
#include "openjpeg.h"
#include "convert.h"

#define JP2_RFC3745_MAGIC_0 0x0C000000
#define JP2_RFC3745_MAGIC_1 0x2020506A
#define JP2_RFC3745_MAGIC_2 0x0A870A0D
#define JP2_MAGIC 0x0A870A0D
#define J2K_CODESTREAM_MAGIC 0x51FF4FFF

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

extern void jsPrintError(const char *);
extern void jsPrintWarning(const char *);
extern void setImageData(OPJ_UINT8 *, OPJ_SIZE_T);

static void error_callback(const char *msg, void *client_data) {
  (void)client_data;
  jsPrintError(msg);
}
static void warning_callback(const char *msg, void *client_data) {
  (void)client_data;
  jsPrintWarning(msg);
}

static void quiet_callback(const char *msg, void *client_data) {
  (void)msg;
  (void)client_data;
}

int EMSCRIPTEN_KEEPALIVE jp2_decode(OPJ_UINT8 *data, OPJ_SIZE_T data_size) {
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
    jsPrintError("Unknown format");
    return 1;
  }

  opj_set_info_handler(l_codec, quiet_callback, 00);
  opj_set_warning_handler(l_codec, warning_callback, 00);
  opj_set_error_handler(l_codec, error_callback, 00);

  opj_set_default_decoder_parameters(&parameters);

  // set stream
  opj_buffer_info_t buffer_info;
  buffer_info.buf = data;
  buffer_info.cur = data;
  buffer_info.len = data_size;
  l_stream = opj_stream_create_buffer_stream(&buffer_info, OPJ_TRUE);

  /* Setup the decoder decoding parameters using user parameters */
  if (unlikely(!opj_setup_decoder(l_codec, &parameters))) {
    jsPrintError("Failed to setup the decoder");
    opj_stream_destroy(l_stream);
    opj_destroy_codec(l_codec);
    return 1;
  }

  /* Read the main header of the codestream and if necessary the JP2 boxes*/
  if (unlikely(!opj_read_header(l_stream, l_codec, &image))) {
    jsPrintError("Failed to read the header");
    opj_stream_destroy(l_stream);
    opj_destroy_codec(l_codec);
    opj_image_destroy(image);
    return 1;
  }

  /* decode the image */
  if (unlikely(!opj_decode(l_codec, l_stream, image) ||
               !opj_end_decompress(l_codec, l_stream))) {
    jsPrintError("Failed to decode the image");
    opj_destroy_codec(l_codec);
    opj_stream_destroy(l_stream);
    opj_image_destroy(image);
    return 1;
  }

  /*printf("image X %d\n", image->x1);
  printf("image Y %d\n", image->y1);
  printf("image numcomps %d\n", image->numcomps);
  printf("image colorspace %d\n", image->color_space);
  printf("prec=%d, bpp=%d, sgnd=%d\n", image->comps[0].prec,
  image->comps[0].bpp, image->comps[0].sgnd); printf("prec=%d, bpp=%d,
  sgnd=%d\n", image->comps[1].prec, image->comps[1].bpp, image->comps[1].sgnd);
  printf("prec=%d, bpp=%d, sgnd=%d\n", image->comps[2].prec,
  image->comps[2].bpp, image->comps[2].sgnd);*/

  opj_stream_destroy(l_stream);
  opj_destroy_codec(l_codec);

  OPJ_SIZE_T nb_pixels = image->x1 * image->y1;
  OPJ_SIZE_T image_size = nb_pixels * image->numcomps;
  OPJ_UINT8 *out = (OPJ_UINT8 *)malloc(image_size);

  switch (image->numcomps) {
  case 1:
    scale_component(&image->comps[0], 8);

    for (OPJ_SIZE_T i = 0; i < nb_pixels; i++) {
      out[i] = image->comps[0].data[i];
    }
    break;
  case 3: {
    scale_component(&image->comps[0], 8);
    scale_component(&image->comps[1], 8);
    scale_component(&image->comps[2], 8);

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
    scale_component(&image->comps[0], 8);
    scale_component(&image->comps[1], 8);
    scale_component(&image->comps[2], 8);
    scale_component(&image->comps[3], 8);

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

  setImageData(out, image_size);
  free(out);

  opj_image_destroy(image);

  return 0;
}
