FROM emscripten/emsdk:latest

WORKDIR /tmp

ENV OPENJPEG_GIT_HASH e8b9d9274a0aee998402d967f65dadd919c31eca
ENV OUTPUT /tmp
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
    emcmake cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF && \
    emmake make -j4 && \
    emcc --clear-cache

CMD /code/compile.sh
