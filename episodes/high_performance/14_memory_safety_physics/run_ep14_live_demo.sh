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

# ==============================================================================
# Hot Path C++ — Episode 14 Live Terminal Demo Wrapper
# File: episodes/high_performance/14_memory_safety_physics/run_ep14_live_demo.sh
# ==============================================================================
set -euo pipefail
EP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${EP_DIR}/run_ep14_forensics.sh" "$@"
