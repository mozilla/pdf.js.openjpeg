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

INPUT=${INPUT:=src}
OUTPUT=${OUTPUT:=.}
OPENJPEG=${OPENJPEG:=..}
WASM2JS="${EMSDK}/upstream/bin/wasm2js"
TERSER="${EMSDK}/upstream/emscripten/node_modules/.bin/terser"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "${TMP_DIR}"' EXIT

build() {
    BUILD_TYPE=$1
    OUTPUT_FILE=$2
    CFLAGS=$3
    OPENJPEG_BUILD="${OPENJPEG}/build_${BUILD_TYPE}"

    echo "Building ${BUILD_TYPE} from ${OPENJPEG_BUILD} to ${OUTPUT_FILE}..."

    EM_CACHE="/tmp/em_cache_${BUILD_TYPE}" emcc -o "${OUTPUT_FILE}" \
        "${OPENJPEG_BUILD}/bin/libopenjp2.a" \
        "${OPENJPEG}/src/bin/jp2/convert.c" \
        "${OPENJPEG}/src/bin/common/color.c" \
        "${INPUT}/decoder.c" \
        -I"${OPENJPEG}/src/lib/openjp2" \
        -I"${OPENJPEG}/src/bin/jp2/" \
        -I"${OPENJPEG}/src/bin/common/" \
        -I"${OPENJPEG_BUILD}/src/lib/openjp2" \
        -I"${OPENJPEG_BUILD}/src/bin/common" \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s MAXIMUM_MEMORY=2GB \
        -s MODULARIZE=1 \
        -s EXPORT_NAME="'OpenJPEG'" \
        -s EXPORT_ES6=1 \
        -s SINGLE_FILE=0 \
        -s ENVIRONMENT='web' \
        -s ERROR_ON_UNDEFINED_SYMBOLS=1 \
        -s NO_FILESYSTEM=1 \
        -s NO_EXIT_RUNTIME=1 \
        -s MALLOC=emmalloc \
        -s EXPORTED_FUNCTIONS='["_jp2_decode", "_malloc", "_free"]' \
        -s EXPORTED_RUNTIME_METHODS='["writeArrayToMemory"]' \
        -s ASSERTIONS=0 \
        -DNDEBUG \
        -flto \
        ${CFLAGS} \
        --js-library "${INPUT}/myjs.js"
}

build wasm "${OUTPUT}/openjpeg.js" "-O3 -msimd128 -msse -mrelaxed-simd"
chmod ugo-x "${OUTPUT}/openjpeg.wasm"

build js "${TMP_DIR}/openjpeg.js" "-Oz"

for PATTERN in 'var wasmImports={[^}]*}' 'function assignWasmExports(wasmExports){[^}]*}'
do
    EXPECTED=$(grep -o "${PATTERN}" "${OUTPUT}/openjpeg.js" || true)
    ACTUAL=$(grep -o "${PATTERN}" "${TMP_DIR}/openjpeg.js" || true)
    if [ -z "${EXPECTED}" ] || [ "${EXPECTED}" != "${ACTUAL}" ]
    then
        echo "The wasm and js builds must have the same imports and exports." >&2
        exit 1
    fi
done

echo "Converting the js build to ${OUTPUT}/openjpeg_nowasm_fallback.js..."

"${WASM2JS}" --all-features --emscripten -O "${TMP_DIR}/openjpeg.wasm" \
    -o "${TMP_DIR}/wasm2js.js"
{ printf "export default "; cat "${TMP_DIR}/wasm2js.js"; } > "${TMP_DIR}/module.js"
"${TERSER}" "${TMP_DIR}/module.js" --module --compress --mangle \
    -o "${OUTPUT}/openjpeg_nowasm_fallback.js"

for OUTPUT_FILE in openjpeg.js openjpeg_nowasm_fallback.js
do
    sed -i '1 i\/* THIS FILE IS GENERATED - DO NOT EDIT */' "${OUTPUT}/${OUTPUT_FILE}"
done

# -s ASSERTIONS=2 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2 -O0 -g4 \
