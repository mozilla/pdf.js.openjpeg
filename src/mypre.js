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

Module.decode = function (bytes, ignoreColorSpace) {
  const size = bytes.length;
  const ptr = Module._malloc(size);
  Module.HEAPU8.set(bytes, ptr);
  const ret = Module._jp2_decode(ptr, size, ignoreColorSpace ? 1 : 0);
  Module._free(ptr);
  if (ret) {
    const { errorMessages } = Module;
    if (errorMessages) {
      delete Module.errorMessages;
      return errorMessages;
    }
    return "Unknown error";
  }
  const { imageData } = Module;
  Module.imageData = null;

  return imageData;
};
