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

"use strict";

const { ArgumentParser } = require("argparse");
const fs = require("fs");
const { spawn, exec } = require("child_process");
const { resolve } = require("path");

function execAndPrint(fun, args) {
  const child = spawn(fun, args.split(" "), { stdio: "inherit" });
  return new Promise((res) => {
    child.on("close", res);
  });
}

function create() {
  return execAndPrint("docker", "build -t openjpeg-decoder .");
}

function build(path) {
  const workingDir = resolve(".");
  return execAndPrint(
    "docker",
    `run -t -v ${path}:/js -v ${workingDir}:/code --rm openjpeg-decoder`
  );
}

function compile(path) {
  path = resolve(path);
  fs.access(path, fs.constants.F_OK, (err) => {
    if (err) {
      console.log(`Invalid path: ${path}`);
      return;
    }
    exec("docker images openjpeg-decoder", (err, stdout) => {
      const output = stdout
        .split("\n")
        .map((line) => line.trim())
        .filter((line) => Boolean(line));
      if (output.length === 1) {
        create().then(() => {
          build(path);
        });
      } else {
        build(path);
      }
    });
  });
}

const parser = new ArgumentParser({
  description: "Build Openjpeg decoder",
});

parser.add_argument("-C", "--create", {
  help: "Create the docker image",
  action: "store_true",
});
parser.add_argument("-c", "--compile", {
  help: "Compile the decoder and output a js file",
  action: "store_true",
});
parser.add_argument("-o", "--output", {
  help: "Output directory",
  default: ".",
});

const args = parser.parse_args();
if (args.create) {
  create().then(() => {
    if (args.compile) {
      compile(args.output);
    }
  });
} else if (args.compile) {
  compile(args.output);
}
