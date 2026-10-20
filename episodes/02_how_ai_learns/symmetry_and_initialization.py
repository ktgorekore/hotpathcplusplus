#!/usr/bin/env python3
# Copyright 2026 Cognitas AI Authors
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

"""Episode 02 Verification: The Clone Symmetry Trap & He/Kaiming Initialization.

Proves why neural networks cannot be initialized to all zeros (W = 0) or
identical constants (W = c):
1. Zero Initialization (W = 0.00): Forward signals collapse to 0; hidden layer
   weight gradients dW = delta * a^T are identically 0.
2. Constant Non-Zero Initialization (W = 0.50): All hidden neurons compute the
   exact same forward activation and receive rank-1 identical gradient rows
   (dW_0 == dW_1 == ... == dW_k), turning K parallel neurons into 1 clone.
3. He / Kaiming Initialization (W ~ N(0, 2 / n_in)): Breaks symmetry so every
   neuron specializes from Day One.
"""

from typing import Dict
import numpy as np
import torch
import torch.nn as nn


def run_symmetry_proof(seed: int = 7) -> Dict[str, int]:
    """Demonstrates zero-init death, constant-init clone paralysis, and He initialization.

    Args:
        seed: Random number generator seed for deterministic verification.

    Returns:
        Dictionary mapping initialization mode name to its gradient matrix rank.
    """
    print("=" * 78)
    print("  EPISODE 02 VERIFICATION: THE CLONE SYMMETRY TRAP & KAIMING INITIALIZATION")
    print("=" * 78)

    x = torch.tensor([[1.2, -0.5, 0.8, 2.0]])
    y_true = torch.tensor([[1.0]])
    ranks: Dict[str, int] = {}

    for mode in ["ZERO_INIT (W = 0.00)", "CONSTANT_INIT (W = 0.50)", "HE_KAIMING_INIT (N(0, 2/n_in))"]:
        torch.manual_seed(seed)
        layer1 = nn.Linear(4, 4, bias=False)
        layer2 = nn.Linear(4, 1, bias=False)

        with torch.no_grad():
            if "ZERO" in mode:
                layer1.weight.fill_(0.0)
                layer2.weight.fill_(0.0)
            elif "CONSTANT" in mode:
                layer1.weight.fill_(0.50)
                layer2.weight.fill_(0.50)
            else:
                nn.init.kaiming_normal_(layer1.weight, nonlinearity="relu")
                nn.init.kaiming_normal_(layer2.weight, nonlinearity="relu")

        h = torch.relu(layer1(x))
        y_pred = layer2(h)
        loss = 0.5 * (y_pred - y_true) ** 2
        loss.backward()

        assert layer1.weight.grad is not None
        grad_rows = layer1.weight.grad.detach().numpy()
        unique_rank = int(np.linalg.matrix_rank(grad_rows))
        ranks[mode] = unique_rank

        print(f"\n  [{mode}]")
        print(f"    Hidden Activations a^(1) : {np.round(h.detach().numpy()[0], 4)}")
        print(f"    Grad Row 0 (Neuron 0)    : {np.round(grad_rows[0], 4)}")
        print(f"    Grad Row 1 (Neuron 1)    : {np.round(grad_rows[1], 4)}")
        print(f"    Gradient Matrix Rank     : {unique_rank} (Identical Clones: {np.allclose(grad_rows[0], grad_rows[1])})")

    print("=" * 78)
    return ranks


if __name__ == "__main__":
    run_symmetry_proof()
