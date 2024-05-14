#!/bin/sh

INPUT=${INPUT:=src}
OUTPUT=${OUTPUT:=.}
OPENJPEG=${OPENJPEG:=..}
OPENJPEG_BUILD=${OPENJPEG_BUILD:=${OPENJPEG}/build}

emcc -o ${OUTPUT}/openjpeg.js \
        ${OPENJPEG_BUILD}/bin/libopenjp2.a \
        ${OPENJPEG}/src/bin/jp2/convert.c \
        ${INPUT}/decoder.c \
        -I${OPENJPEG}/src/lib/openjp2 \
        -I${OPENJPEG}/src/bin/jp2/ \
        -I${OPENJPEG_BUILD}/src/lib/openjp2 \
        -I${OPENJPEG_BUILD}/src/bin/common \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s WASM=1 \
        -s MODULARIZE=1 \
        -s EXPORT_NAME="'OpenJPEG'" \
        -s WASM_ASYNC_COMPILATION=0 \
        -s EXPORT_ES6=1 \
        -s USE_ES6_IMPORT_META=0 \
        -s SINGLE_FILE=1 \
        -s ENVIRONMENT='web' \
        -s ERROR_ON_UNDEFINED_SYMBOLS=1 \
        -s NO_FILESYSTEM=1 \
        -s NO_EXIT_RUNTIME=1 \
        -s MALLOC=emmalloc \
        -s EXPORTED_FUNCTIONS='["_jp2_decode", "_malloc", "_free"]' \
        -s AGGRESSIVE_VARIABLE_ELIMINATION=1 \
        -s ASSERTIONS=0 \
        -DNDEBUG \
        -flto \
        -O3 \
        -msimd128 -msse \
        --js-library ${INPUT}/myjs.js \
        --pre-js ${INPUT}/mypre.js


# -s ASSERTIONS=2 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2 -O0 -g4 \
        