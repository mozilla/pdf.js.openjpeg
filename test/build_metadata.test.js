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

const assert = require("node:assert/strict");
const { execFileSync } = require("node:child_process");
const { mkdtempSync, rmSync, writeFileSync } = require("node:fs");
const { tmpdir } = require("node:os");
const { join } = require("node:path");
const { test } = require("node:test");

test("build metadata records the revision and local changes", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "openjpeg-metadata-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const env = { ...process.env, REPO_REVISION: "" };
  delete env.GIT_INDEX_FILE;
  delete env.OPENJPEG_GIT_HASH;
  const options = { cwd: dir, encoding: "utf8", env };
  const git = (...args) =>
    execFileSync(
      "git",
      ["-c", "user.name=Test", "-c", "user.email=test@example.com", ...args],
      options,
    ).trim();

  const source = join(dir, "decoder.c");
  writeFileSync(source, "original\n");
  git("init", "--quiet");
  git("add", ".");
  git("commit", "--quiet", "--no-gpg-sign", "-m", "Initial commit");
  git("tag", "--annotate", "--no-sign", "-m", "Build tag", "build-tag");

  const revision = git("rev-parse", "HEAD");
  const emscripten = "1.2.3";
  const flags = '-s EXPORTED_FUNCTIONS=["_malloc", "_free"]\t-O3';
  const args = [
    require.resolve("../build_metadata.js"),
    dir,
    dir,
    emscripten,
    flags,
  ];
  const readMetadata = (extraEnv = {}) =>
    JSON.parse(
      execFileSync(process.execPath, args, {
        ...options,
        env: { ...env, ...extraEnv },
      }),
    );

  assert.deepEqual(readMetadata(), {
    openjpeg: revision,
    repo: revision,
    emscripten,
    flags,
  });

  // The upstream revision is preferred to the patched checkout one.
  const upstream = "0123456789abcdef0123456789abcdef01234567";
  assert.equal(
    readMetadata({ OPENJPEG_GIT_HASH: upstream }).openjpeg,
    upstream,
  );

  git("config", "status.showUntrackedFiles", "no");
  const untracked = join(dir, "untracked.h");
  writeFileSync(untracked, "untracked\n");
  assert.equal(readMetadata().repo, `${revision}-dirty`);
  rmSync(untracked);
  assert.equal(readMetadata().repo, revision);

  writeFileSync(source, "modified\n");
  assert.equal(readMetadata().repo, `${revision}-dirty`);

  writeFileSync(join(dir, ".git", "index"), "invalid index\n");
  assert.equal(readMetadata().repo, "unknown");
});
