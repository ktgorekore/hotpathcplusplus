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

"""Hot Path C++ — Episode 01: Deep Learning Foundations Benchmark.

Benchmarks Shallow Wide Networks (Single Hidden Layer) vs. Deep Hierarchical
Networks (Dimensionality Reduction: 1024 -> 512 -> 256 -> 128 -> 10).
Quantifies parameter counts, FLOPs, forward-pass latency, and memory footprint.
"""

import time
import torch
import torch.nn as nn


class ShallowApproximator(nn.Module):
    """Shallow Universal Approximator with a massive single hidden layer.
    
    To approximate complex high-dimensional non-linear functions with a single
    hidden layer, universal approximation theorems dictate an exponentially wide
    hidden representation (e.g., 10,240 neurons).
    """

    def __init__(self, in_features: int = 1024, hidden_dim: int = 10240, out_classes: int = 10):
        super().__init__()
        self.fc1 = nn.Linear(in_features, hidden_dim)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(hidden_dim, out_classes)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.fc2(self.relu(self.fc1(x)))


class DeepHierarchicalNetwork(nn.Module):
    """Deep Hierarchical Network with progressive dimensionality reduction.
    
    Emulates mathematical 'middle management': each layer extracts progressively
    higher-level abstract representations while condensing tensor dimensionality:
    1024 (Raw Input) -> 512 -> 256 -> 128 -> 10 (Decision Output).
    """

    def __init__(self, in_features: int = 1024, out_classes: int = 10):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(in_features, 512),
            nn.ReLU(),
            nn.Linear(512, 256),
            nn.ReLU(),
            nn.Linear(256, 128),
            nn.ReLU(),
            nn.Linear(128, out_classes),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def count_parameters(model: nn.Module) -> int:
    return sum(p.numel() for p in model.parameters() if p.requires_grad)


def benchmark_model(model: nn.Module, x: torch.Tensor, warmup_iters: int = 100, bench_iters: int = 1000):
    model.eval()
    with torch.no_grad():
        for _ in range(warmup_iters):
            _ = model(x)
        if torch.cuda.is_available():
            torch.cuda.synchronize()

        start = time.perf_counter()
        for _ in range(bench_iters):
            _ = model(x)
        if torch.cuda.is_available():
            torch.cuda.synchronize()
        end = time.perf_counter()

    avg_ms = ((end - start) / bench_iters) * 1000.0
    throughput = (bench_iters * x.size(0)) / (end - start)
    return avg_ms, throughput


def main():
    print("=" * 72)
    print("  HOT PATH C++ // EPISODE 01: DEPTH VS SHALLOW BENCHMARK")
    print("  Testing Dimensionality Reduction: [1024] -> [512] -> [256] -> [128] -> [10]")
    print("=" * 72)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Executing on device: {device} ({torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'CPU'})\n")

    batch_size = 128
    in_features = 1024
    out_classes = 10

    x = torch.randn(batch_size, in_features, device=device)

    shallow = ShallowApproximator(in_features=in_features, hidden_dim=10240, out_classes=out_classes).to(device)
    deep = DeepHierarchicalNetwork(in_features=in_features, out_classes=out_classes).to(device)

    shallow_params = count_parameters(shallow)
    deep_params = count_parameters(deep)

    # Calculate theoretical FLOPs per item:
    # Shallow: 2 * 1024 * 10240 + 2 * 10240 * 10 = ~21.18 MFLOPs
    shallow_flops = 2 * (in_features * 10240 + 10240 * out_classes)
    # Deep: 2 * (1024*512 + 512*256 + 256*128 + 128*10) = 2 * (524288 + 131072 + 32768 + 1280) = ~1.38 MFLOPs
    deep_flops = 2 * (1024 * 512 + 512 * 256 + 256 * 128 + 128 * out_classes)

    shallow_ms, shallow_thru = benchmark_model(shallow, x)
    deep_ms, deep_thru = benchmark_model(deep, x)

    print(f"| Model Architecture           | Parameters    | FLOPs/Sample  | Latency (Batch={batch_size}) | Throughput (Samples/s) |")
    print(f"| :--------------------------- | :------------ | :------------ | :------------------- | :--------------------- |")
    print(f"| Shallow (1 Hidden @ 10,240)  | {shallow_params:>12,d} | {shallow_flops/1e6:>11.2f}M | {shallow_ms:>18.3f} ms | {shallow_thru:>20.1f} |")
    print(f"| Deep (4 Layers Hierarchical) | {deep_params:>12,d} | {deep_flops/1e6:>11.2f}M | {deep_ms:>18.3f} ms | {deep_thru:>20.1f} |")

    param_reduction = shallow_params / deep_params
    speedup = shallow_ms / deep_ms
    flop_reduction = shallow_flops / deep_flops

    print("\nBenchmark Key Takeaways:")
    print(f"  • Deep network reduces parameter count by {param_reduction:.1f}x ({shallow_params:,d} vs {deep_params:,d}).")
    print(f"  • Deep network requires {flop_reduction:.1f}x fewer FLOPs per inference.")
    print(f"  • Deep network executes {speedup:.2f}x faster on silicon hardware.")
    print("=" * 72)


if __name__ == "__main__":
    main()
