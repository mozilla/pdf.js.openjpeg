# pdf.js.openjpeg

Provide a decoder for JPEG2000 images based on the [OpenJPEG](https://github.com/uclouvain/openjpeg) library.

## Build

Run:

```sh
node build.js --compile --output my_output_dir
```

This builds the Docker image using Docker's layer cache, then writes `openjpeg.js`, `openjpeg.wasm` and `openjpeg_nowasm_fallback.js` to `my_output_dir`.

### Build metadata

`openjpeg.wasm` contains a `build_info` custom section with a JSON object describing the build: the OpenJPEG revision (`openjpeg`), the revision of this repository (`repo`, suffixed with `-dirty` if there were local changes), the Emscripten version (`emscripten`) and the link flags (`flags`). It can be read with:

```js
const module = await WebAssembly.compile(bytes);
const [section] = WebAssembly.Module.customSections(module, "build_info");
const buildInfo = JSON.parse(new TextDecoder().decode(section));
```

## Update

To update OpenJPEG, change the commit hash in `Dockerfile` and run:

```sh
node build.js --compile --output my_output_dir
```

Use `node build.js --create` to build only the Docker image.

## Release

Pushing a version tag (e.g. `v1.2.3`) runs the `Release` workflow: it builds the decoder and opens a pull request in [pdf.js](https://github.com/mozilla/pdf.js) updating `external/openjpeg/`, titled "Update OpenJPEG decoder to version 1.2.3":
```sh
git tag v1.2.3
git push origin v1.2.3
```

The workflow refuses to replace a decoder in pdf.js that was built from a revision which isn't an ancestor of the tagged one, so that retrying an old tag can't revert a newer release.

The workflow runs in the `release` environment, which must provide the `CLIENT_ID` and `APP_PRIVATE_KEY` secrets of a GitHub App installed on `mozilla/pdf.js` with read and write access to contents and pull requests.

## Licensing

The code is released under [BSD 2-clause](https://spdx.org/licenses/BSD-2-Clause.html).
