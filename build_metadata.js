/*
 * Copyright (c) 2026, Mozilla Foundation
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

const { execFileSync } = require("child_process");
const { readFileSync } = require("fs");

function git(dir, args) {
  return execFileSync("git", args, {
    cwd: dir,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "ignore"],
  }).trim();
}

function getRepoRevision(dir, gitOptions = []) {
  try {
    const revision = git(dir, [...gitOptions, "rev-parse", "HEAD"]);
    const status = git(dir, [
      ...gitOptions,
      "status",
      "--porcelain",
      "--untracked-files=normal",
    ]);
    return status ? `${revision}-dirty` : revision;
  } catch {
    return "unknown";
  }
}

function getOpenjpegRevision(dir) {
  // In Docker, HEAD is the commit created by applying our patch on top of the
  // upstream revision, so prefer the upstream one.
  if (process.env.OPENJPEG_GIT_HASH) {
    return process.env.OPENJPEG_GIT_HASH;
  }
  try {
    return git(dir, ["rev-parse", "HEAD"]);
  } catch {
    return "unknown";
  }
}

function readBuildMetadata(wasmFile) {
  const module = new WebAssembly.Module(readFileSync(wasmFile));
  const [section] = WebAssembly.Module.customSections(module, "build_info");
  return section ? JSON.parse(new TextDecoder().decode(section)) : undefined;
}

if (require.main === module) {
  const [openjpegDir, sourceDir, emscripten, flags] = process.argv.slice(2);
  const openjpeg = getOpenjpegRevision(openjpegDir);
  // The mounted checkout can belong to a different user inside Docker.
  const repo =
    process.env.REPO_REVISION ||
    getRepoRevision(sourceDir, ["-c", "safe.directory=*"]);
  process.stdout.write(JSON.stringify({ openjpeg, repo, emscripten, flags }));
}

module.exports = { getRepoRevision, readBuildMetadata };
