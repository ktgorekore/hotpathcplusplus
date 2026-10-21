<!--
  Copyright 2026 Cognitas AI Authors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-->

# 🧠 Episode 02: How AI Actually Learns (Feed-Forward, Backprop, Mini-Batch SGD & C++ Kernels)

[![YouTube Channel](https://img.shields.io/badge/YouTube-Hot%20Path%20C%2B%2B-red?logo=youtube)](https://youtube.com/@HotPathCpp)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![PyTorch CUDA](https://img.shields.io/badge/PyTorch-CUDA-orange.svg)](https://pytorch.org/)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](../../../LICENSE)

Official open-source C++20 and PyTorch companion code for **Episode 02**:
- **Full Masterclass:** *How AI Actually Learns: From 99% Wrong to 99.8% Right (Inside an AI Brain)*
- **YouTube Short:** *How AI Actually Learns: From 99% Wrong to 99.8% Right!*

---

## ⚡ 1. C++20 Google Benchmark Suite (`benchmark.cc`)

Audited on 32-thread x86-64 Linux (`-O3 -std=c++20 -march=native`):

| Benchmark Target | CPU Time | Speedup / Insight |
| :--- | :--- | :--- |
| **`BM_Backprop_ReverseMode_OW`** | **`1,303 ns`** | **3,988x faster** than brute-force nudging on 5,120 weights |
| **`BM_FiniteDifferences_BruteForce_OW2`** | `5,196,618 ns` | Requires 5,120 separate forward passes per step |
| **`BM_MiniBatch64_Tiled_GEMM_64Samples`** | **`520,239 ns`** | **2.47x higher throughput** (`123.0k items/s`) via cache tiling |
| **`BM_PureSGD_Batch1_GEMV_64Samples`** | `1,284,358 ns` | Re-streams weight matrix 64 separate times (`49.8k items/s`) |
| **`BM_Inference_Fused_SingleTrip`** | **`37.1 ns`** | **2.0x faster** by fusing Bias + ReLU in registers |
| **`BM_Inference_Unfused_3Trips`** | `74.3 ns` | Extra memory round-trip between Bias and ReLU |

### Run with Bazel 9
```bash
bazel run -c opt //episodes/ai/02_how_ai_learns:benchmark
bazel run -c opt //episodes/ai/02_how_ai_learns:challenge
```

---

## 🔬 2. PyTorch & GPU Verification Scripts

1. **`vram_activation_benchmark.py`**: Benchmarks Inference (`torch.no_grad()`, 44.0 MB dynamic VRAM) vs. Training (`loss.backward()`, 1,604.0 MB dynamic VRAM, **36.5x memory amplification** because Backprop caches forward activations `a` to compute `dW = delta * a^T`).
2. **`symmetry_and_initialization.py`**: Proves why Zero Init (`W = 0`) and Constant Init (`W = 0.5`) collapse hidden layers into rank-1 clones, and how Kaiming He Gaussian initialization (`N(0, 2/n_in)`) restores full rank.
3. **`backprop_chain_rule_autograd.py`**: Verifies the Sleeping Neuron Alibi (`a = 0.0 -> grad = 0`) and compares 50-layer Sigmoid vanishing gradients (`0.25^50 = 7.89e-31`) against a ResNet skip highway (`y = F(x) + x`).
4. **`optimizers_saddle_ravine.py`**: Simulates IEEE-754 `NaN` overflow when the learning rate exceeds `2 / lambda_max`, and compares Vanilla SGD wall-bouncing against Heavy-Ball Momentum in a narrow 50:1 ravine.

```bash
python3 episodes/ai/02_how_ai_learns/vram_activation_benchmark.py
python3 episodes/ai/02_how_ai_learns/symmetry_and_initialization.py
python3 episodes/ai/02_how_ai_learns/backprop_chain_rule_autograd.py
python3 episodes/ai/02_how_ai_learns/optimizers_saddle_ravine.py
```
