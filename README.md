# pdf.js.openjpeg

Provide a decoder for JPEG2000 images.

## Build

Run:

```sh
node build.js --compile --output my_output_dir
```

it will create a Docker image with emsdk and then run it. The generated `openjpeg.js` will be in `my_output_dir`.

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

The code is released under [Apache 2](https://www.apache.org/licenses/LICENSE-2.0).
