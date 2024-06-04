/*
 * Copyright (c) 2024, Mozilla Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
