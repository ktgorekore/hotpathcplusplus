// Copyright 2026 Cognitas AI Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace hotpath::episode02 {

/**
 * @brief Lightweight two-layer MLP in contiguous row-major memory for auditing
 *        O(W) Reverse-Mode Backpropagation vs O(W^2) Finite Differences.
 */
struct TwoLayerMlp {
  int in_dim;
  int hidden_dim;
  int out_dim;
  std::vector<float> w1;
  std::vector<float> w2;
  std::vector<float> grad_w1;
  std::vector<float> grad_w2;

  /**
   * @brief Constructs and initializes the MLP with Kaiming normal weights.
   * @param in_features Number of input features.
   * @param hidden_units Number of hidden ReLU units.
   * @param out_features Number of output units.
   */
  TwoLayerMlp(int in_features, int hidden_units, int out_features)
      : in_dim(in_features),
        hidden_dim(hidden_units),
        out_dim(out_features),
        w1(hidden_units * in_features),
        w2(out_features * hidden_units),
        grad_w1(hidden_units * in_features, 0.0f),
        grad_w2(out_features * hidden_units, 0.0f) {
    std::mt19937 rng(42);
    std::normal_distribution<float> dist1(
        0.0f, std::sqrt(2.0f / static_cast<float>(in_features)));
    std::normal_distribution<float> dist2(
        0.0f, std::sqrt(2.0f / static_cast<float>(hidden_units)));
    for (float& v : w1) v = dist1(rng);
    for (float& v : w2) v = dist2(rng);
  }

  /**
   * @brief Computes a single forward pass and returns the MSE loss.
   * @param x Input vector of size in_dim.
   * @param target Ground-truth vector of size out_dim.
   * @param hidden_out Pre-allocated buffer of size hidden_dim for ReLU outputs.
   * @param pred_out Pre-allocated buffer of size out_dim for predictions.
   * @return Scalar mean squared error loss.
   */
  float Forward(const std::vector<float>& x, const std::vector<float>& target,
                std::vector<float>& hidden_out,
                std::vector<float>& pred_out) const {
    for (int h = 0; h < hidden_dim; ++h) {
      float sum = 0.0f;
      const int row_offset = h * in_dim;
      for (int i = 0; i < in_dim; ++i) {
        sum += w1[row_offset + i] * x[i];
      }
      hidden_out[h] = sum > 0.0f ? sum : 0.0f;
    }

    float loss = 0.0f;
    for (int o = 0; o < out_dim; ++o) {
      float sum = 0.0f;
      const int row_offset = o * hidden_dim;
      for (int h = 0; h < hidden_dim; ++h) {
        sum += w2[row_offset + h] * hidden_out[h];
      }
      pred_out[o] = sum;
      const float diff = sum - target[o];
      loss += 0.5f * diff * diff;
    }
    return loss / static_cast<float>(out_dim);
  }

  /**
   * @brief Executes O(W) Reverse-Mode Autodiff (Backpropagation) in one sweep.
   * @param x Input vector of size in_dim.
   * @param target Ground-truth vector of size out_dim.
   * @param hidden_buf Cached forward hidden activations (receipts).
   * @param pred_buf Forward predictions.
   * @param delta_out_buf Scratch buffer of size out_dim.
   * @param delta_hid_buf Scratch buffer of size hidden_dim.
   */
  void BackwardReverseMode(const std::vector<float>& x,
                           const std::vector<float>& target,
                           std::vector<float>& hidden_buf,
                           std::vector<float>& pred_buf,
                           std::vector<float>& delta_out_buf,
                           std::vector<float>& delta_hid_buf) {
    Forward(x, target, hidden_buf, pred_buf);
    const float inv_out = 1.0f / static_cast<float>(out_dim);

    for (int o = 0; o < out_dim; ++o) {
      delta_out_buf[o] = (pred_buf[o] - target[o]) * inv_out;
      const int row_offset = o * hidden_dim;
      for (int h = 0; h < hidden_dim; ++h) {
        grad_w2[row_offset + h] = delta_out_buf[o] * hidden_buf[h];
      }
    }

    for (int h = 0; h < hidden_dim; ++h) {
      if (hidden_buf[h] <= 0.0f) {
        delta_hid_buf[h] = 0.0f;
        continue;
      }
      float upstream = 0.0f;
      for (int o = 0; o < out_dim; ++o) {
        upstream += w2[o * hidden_dim + h] * delta_out_buf[o];
      }
      delta_hid_buf[h] = upstream;
      const int row_offset = h * in_dim;
      for (int i = 0; i < in_dim; ++i) {
        grad_w1[row_offset + i] = upstream * x[i];
      }
    }
  }
};

/**
 * @brief Benchmarks O(W) Reverse-Mode Backpropagation (1 forward + 1 backward).
 * @param state Google Benchmark state tracking loop execution.
 */
static void BM_Backprop_ReverseMode_OW(benchmark::State& state) {
  TwoLayerMlp net(64, 64, 16);
  std::vector<float> x(64, 0.25f);
  std::vector<float> target(16, 1.0f);
  std::vector<float> hidden_buf(64);
  std::vector<float> pred_buf(16);
  std::vector<float> delta_out(16);
  std::vector<float> delta_hid(64);

  for (auto _ : state) {
    net.BackwardReverseMode(x, target, hidden_buf, pred_buf, delta_out,
                            delta_hid);
    benchmark::DoNotOptimize(net.grad_w1.data());
    benchmark::DoNotOptimize(net.grad_w2.data());
  }
}
BENCHMARK(BM_Backprop_ReverseMode_OW);

/**
 * @brief Benchmarks O(W^2) brute-force finite differences (1 forward per knob).
 * @param state Google Benchmark state tracking loop execution.
 */
static void BM_FiniteDifferences_BruteForce_OW2(benchmark::State& state) {
  TwoLayerMlp net(64, 64, 16);
  std::vector<float> x(64, 0.25f);
  std::vector<float> target(16, 1.0f);
  std::vector<float> hidden_buf(64);
  std::vector<float> pred_buf(16);
  constexpr float kEps = 1e-4f;

  for (auto _ : state) {
    const float base_loss = net.Forward(x, target, hidden_buf, pred_buf);
    for (size_t idx = 0; idx < net.w1.size(); ++idx) {
      const float orig = net.w1[idx];
      net.w1[idx] = orig + kEps;
      const float nudged = net.Forward(x, target, hidden_buf, pred_buf);
      net.grad_w1[idx] = (nudged - base_loss) / kEps;
      net.w1[idx] = orig;
    }
    for (size_t idx = 0; idx < net.w2.size(); ++idx) {
      const float orig = net.w2[idx];
      net.w2[idx] = orig + kEps;
      const float nudged = net.Forward(x, target, hidden_buf, pred_buf);
      net.grad_w2[idx] = (nudged - base_loss) / kEps;
      net.w2[idx] = orig;
    }
    benchmark::DoNotOptimize(net.grad_w1.data());
    benchmark::DoNotOptimize(net.grad_w2.data());
  }
}
BENCHMARK(BM_FiniteDifferences_BruteForce_OW2);

/**
 * @brief Benchmarks Pure SGD (Batch=1 GEMV) processing 64 samples one by one,
 *        re-streaming the weight matrix W from cache/memory 64 separate times.
 * @param state Google Benchmark state tracking loop execution.
 */
static void BM_PureSGD_Batch1_GEMV_64Samples(benchmark::State& state) {
  constexpr int kBatch = 64;
  constexpr int kIn = 256;
  constexpr int kOut = 256;
  std::vector<float> w(kOut * kIn, 0.01f);
  std::vector<float> x(kBatch * kIn, 0.5f);
  std::vector<float> out(kBatch * kOut, 0.0f);

  for (auto _ : state) {
    for (int b = 0; b < kBatch; ++b) {
      for (int o = 0; o < kOut; ++o) {
        float sum = 0.0f;
        const int w_row = o * kIn;
        const int x_row = b * kIn;
        for (int i = 0; i < kIn; ++i) {
          sum += w[w_row + i] * x[x_row + i];
        }
        out[b * kOut + o] = sum;
      }
      benchmark::ClobberMemory();
    }
    benchmark::DoNotOptimize(out.data());
  }
  state.SetItemsProcessed(state.iterations() * kBatch);
}
BENCHMARK(BM_PureSGD_Batch1_GEMV_64Samples);

/**
 * @brief Benchmarks Mini-Batch SGD (Batch=64 GEMM) reusing loaded weight tiles
 *        across all 64 samples in CPU registers/L1 cache.
 * @param state Google Benchmark state tracking loop execution.
 */
static void BM_MiniBatch64_Tiled_GEMM_64Samples(benchmark::State& state) {
  constexpr int kBatch = 64;
  constexpr int kIn = 256;
  constexpr int kOut = 256;
  constexpr int kTile = 16;
  std::vector<float> w(kOut * kIn, 0.01f);
  std::vector<float> x(kBatch * kIn, 0.5f);
  std::vector<float> out(kBatch * kOut, 0.0f);

  for (auto _ : state) {
    std::fill(out.begin(), out.end(), 0.0f);
    for (int o0 = 0; o0 < kOut; o0 += kTile) {
      for (int i0 = 0; i0 < kIn; i0 += kTile) {
        for (int b = 0; b < kBatch; ++b) {
          for (int o = o0; o < o0 + kTile; ++o) {
            float sum = out[b * kOut + o];
            for (int i = i0; i < i0 + kTile; ++i) {
              sum += w[o * kIn + i] * x[b * kIn + i];
            }
            out[b * kOut + o] = sum;
          }
        }
      }
    }
    benchmark::DoNotOptimize(out.data());
  }
  state.SetItemsProcessed(state.iterations() * kBatch);
}
BENCHMARK(BM_MiniBatch64_Tiled_GEMM_64Samples);

/**
 * @brief Benchmarks Unfused 3-pass inference (MatMul -> Bias write -> ReLU
 *        write) vs a single Fused C++ kernel keeping values in registers.
 * @param state Google Benchmark state tracking loop execution.
 */
static void BM_Inference_Unfused_3Trips(benchmark::State& state) {
  constexpr int kN = 1024;
  std::vector<float> z(kN, 0.5f);
  std::vector<float> bias(kN, -0.1f);
  std::vector<float> temp(kN, 0.0f);
  std::vector<float> out(kN, 0.0f);

  for (auto _ : state) {
    for (int i = 0; i < kN; ++i) {
      temp[i] = z[i] + bias[i];
    }
    benchmark::ClobberMemory();
    for (int i = 0; i < kN; ++i) {
      out[i] = temp[i] > 0.0f ? temp[i] : 0.0f;
    }
    benchmark::DoNotOptimize(out.data());
  }
}
BENCHMARK(BM_Inference_Unfused_3Trips);

/**
 * @brief Benchmarks Fused single-pass Bias + ReLU activation in registers.
 * @param state Google Benchmark state tracking loop execution.
 */
static void BM_Inference_Fused_SingleTrip(benchmark::State& state) {
  constexpr int kN = 1024;
  std::vector<float> z(kN, 0.5f);
  std::vector<float> bias(kN, -0.1f);
  std::vector<float> out(kN, 0.0f);

  for (auto _ : state) {
    for (int i = 0; i < kN; ++i) {
      const float val = z[i] + bias[i];
      out[i] = val > 0.0f ? val : 0.0f;
    }
    benchmark::DoNotOptimize(out.data());
  }
}
BENCHMARK(BM_Inference_Fused_SingleTrip);

}  // namespace hotpath::episode02
