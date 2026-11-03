#!/usr/bin/env bash
# Copyright 2026 Hot Path C++ Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

FORMATTER_BIN="${0}.runfiles/_main/tools/google_java_format"
if [[ ! -x "${FORMATTER_BIN}" ]]; then
  FORMATTER_BIN="$(dirname "$0")/google_java_format"
fi

TARGET_DIR="${1:-.}"
echo "Formatting Java source files in: ${TARGET_DIR}..."

find "${TARGET_DIR}" -type f -name "*.java" -not -path "*/.*" -not -path "*/bazel-*/*" | while read -r file; do
  "${FORMATTER_BIN}" --replace "${file}"
done

echo "Formatting completed successfully."
