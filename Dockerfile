FROM emscripten/emsdk:latest

WORKDIR /tmp

ENV OPENJPEG_GIT_HASH 39e8c50a2f9bdcf36810ee3d41bcbf1cc78968ae
ENV OUTPUT /js
ENV OPENJPEG /tmp/openjpeg
ENV INPUT /code/src

ADD *.patch .

RUN git config --global user.email "you@example.com" && \
    git config --global user.name "Your Name" && \
    git clone https://github.com/uclouvain/openjpeg && \
    cd openjpeg && \
    git checkout -b extra ${OPENJPEG_GIT_HASH} && \
    git am ../0001-Add-support-for-buffer-based-stream-see-https-github.patch

RUN cd openjpeg && \
    mkdir build && cd build && \
    emcmake cmake .. \
    -DCMAKE_C_FLAGS="-O3 -msimd128 -msse" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DOPJ_USE_THREAD=OFF \
    -DCMAKE_BUILD_TYPE=Release && \
    emmake make openjp2 && \
    emcc --clear-cache

CMD /code/compile.sh
