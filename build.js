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
const { spawn } = require("child_process");
const { resolve } = require("path");

function execAndPrint(fun, args) {
  const child = spawn(fun, args, { stdio: "inherit" });
  return new Promise((resolve, reject) => {
    child.on("error", reject);
    child.on("close", (code) => {
      if (code === 0) {
        resolve();
        return;
      }
      reject(new Error(`${fun} exited with code ${code}`));
    });
  });
}

function create() {
  return execAndPrint("docker", ["build", "-t", "openjpeg-decoder", "."]);
}

function build(type, path) {
  const workingDir = resolve(".");
  return execAndPrint("docker", [
    "run",
    "-t",
    "-v",
    `${path}:/js`,
    "-v",
    `${workingDir}:/code`,
    "--env",
    `BUILD_TYPE=${type}`,
    "--rm",
    "openjpeg-decoder",
  ]);
}

async function hasImage() {
  try {
    await execAndPrint("docker", ["image", "inspect", "openjpeg-decoder"]);
    return true;
  } catch {
    return false;
  }
}

async function compile(type, path) {
  path = resolve(path);
  await fs.promises.access(path, fs.constants.F_OK);
  if (!(await hasImage())) {
    await create();
  }
  await build(type, path);
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
parser.add_argument("-t", "--type", {
  help: "Type (wasm or js)",
  default: "wasm",
});

const args = parser.parse_args();

async function main() {
  if (args.create) {
    await create();
  }
  if (args.compile) {
    await compile(args.type, args.output);
  }
}

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
