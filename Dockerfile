FROM emscripten/emsdk:4.0.3
WORKDIR /tmp

ENV OPENJPEG_GIT_HASH 6e92b398dc8e0bf1c2d39dc01f4e9f94b15e75f7
ADD *.patch .

RUN git config --global user.email "you@example.com" && \
    git config --global user.name "Your Name" && \
    git clone https://github.com/uclouvain/openjpeg && \
    cd openjpeg && \
    git checkout -b extra ${OPENJPEG_GIT_HASH} && \
    git am ../0001-Add-support-for-buffer-based-stream-see-https-github.patch && \
    cd ..

ENV OUTPUT /js
ENV OPENJPEG /tmp/openjpeg
ENV INPUT /code/src

ADD compile_lib.sh .

ENV BUILD_TYPE wasm
ENV BUILD_DIR build_${BUILD_TYPE}
RUN ./compile_lib.sh

ENV BUILD_TYPE js
ENV BUILD_DIR build_${BUILD_TYPE}
RUN ./compile_lib.sh

ENV BUILD_DIR ""

CMD /code/compile.sh
