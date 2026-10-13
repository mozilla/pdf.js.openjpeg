# pdf.js.openjpeg

Provide a decoder for JPEG2000 images based on the [OpenJPEG](https://github.com/uclouvain/openjpeg) library.

## Build

Run:

```sh
node build.js --compile --output my_output_dir
```

This builds a Docker image if needed, then writes `openjpeg.js`, `openjpeg.wasm` and `openjpeg_nowasm_fallback.js` to `my_output_dir`.

## Update

In order to update openjpeg to a specific revision, change the commit hash in `Dockerfile` and then run:
```sh
node build.js --create
```
to create a new docker image and then
```sh
node build.js --compile --output my_output_dir
```
to compile. The short version is:
```sh
node build.js -Cco my_output_dir
```

## Licensing

The code is released under [BSD 2-clause](https://spdx.org/licenses/BSD-2-Clause.html).
