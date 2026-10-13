#!/bin/sh

# Copyright (c) 2024, Mozilla Foundation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -e

OPENJPEG=${OPENJPEG:=..}
BUILD_TYPE=${BUILD_TYPE:=wasm}
BUILD_DIR=${BUILD_DIR:=build_${BUILD_TYPE}}
export EM_CACHE="/tmp/em_cache_${BUILD_TYPE}"

if [ "$BUILD_TYPE" = "js" ]
then
    CFLAGS="-Oz"
else
    # Enable OpenJPEG's SSE2 paths for the 5/3 wavelet and reversible MCT.
    SIMD_CFLAGS="-msimd128 -msse2 -mrelaxed-simd"
    CFLAGS="-Os ${SIMD_CFLAGS}"
    # Keep -O3 for these decoding kernels; use -Os for the rest.
    HOT_FILES="t1 ht_dec mct tcd sparse_array"
    HOT_CFLAGS="-O3 ${SIMD_CFLAGS}"
fi

# Set Release flags directly: the default -O3 overrides CMAKE_C_FLAGS.
cd "${OPENJPEG}" && \
    mkdir -p "${BUILD_DIR}" && cd "${BUILD_DIR}" && \
    emcmake cmake .. \
    -DCMAKE_C_FLAGS_RELEASE="${CFLAGS} -DNDEBUG" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DOPJ_USE_THREAD=OFF \
    -DCMAKE_BUILD_TYPE=Release && \
    emmake make openjp2

if [ -n "${HOT_FILES}" ]
then
    mkdir -p hot
    for FILE in ${HOT_FILES}
    do
        emcc ${HOT_CFLAGS} -DNDEBUG -Isrc/lib/openjp2 \
            -c "../src/lib/openjp2/${FILE}.c" -o "hot/${FILE}.c.o"
    done
    # Replace the corresponding archive members.
    emar rs bin/libopenjp2.a hot/*.c.o
fi
