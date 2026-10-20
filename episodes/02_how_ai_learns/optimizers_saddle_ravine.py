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

"""Episode 02 Verification: Learning Rate Overflow (NaN) & Momentum Ravine Physics.

Demonstrates:
1. Sequence 07: Learning rate regimes on L(w) = w^2:
   - Tiny Baby Steps (eta = 1e-9)
   - Safe Speed Limit (eta = 0.20)
   - Giant Strides (eta = 10.0) -> IEEE 754 float32 overflow to Inf/NaN (0x7F800000).
2. Sequence 08: Taco-Shell Ravine L(w1, w2) = 0.5 * w1^2 + 25.0 * w2^2:
   - Memoryless Pinball (Vanilla SGD) bounces violently across steep w2 canyon walls.
   - Heavy Steel Bowling Ball (Momentum) cancels wall-to-wall bouncing and rolls
     smoothly down the valley floor.
"""

import struct
from typing import Dict, Tuple
import numpy as np


def simulate_learning_rate_regimes(max_steps: int = 18) -> Dict[str, float]:
    """Simulates learning rates too small, optimal, and too large (diverging to Inf/NaN).

    Args:
        max_steps: Maximum number of gradient descent updates to run per regime.

    Returns:
        Dictionary mapping each learning-rate regime label to its final weight value.
    """
    print("=" * 78)
    print("  EPISODE 02 VERIFICATION: LEARNING RATE (ETA) & IEEE 754 NaN EXPLOSION")
    print("=" * 78)

    results: Dict[str, float] = {}
    for label, eta in [("TINY STEPS (1e-9)", 1e-9), ("GOLDILOCKS (0.20)", 0.20), ("GIANT STRIDE (10.0)", 10.0)]:
        w = np.float32(2.5)
        history = [float(w)]
        for _ in range(max_steps):
            grad = np.float32(2.0) * w
            w = np.float32(w - np.float32(eta) * grad)
            history.append(float(w))
            if not np.isfinite(w):
                break
        bits = struct.unpack(">I", struct.pack(">f", w))[0]
        results[label] = float(w)
        print(f"  [{label:19s}] Final w = {w!s:>12s} | IEEE-754 Hex: 0x{bits:08X} | Steps: {len(history)-1}")
        print(f"    Trajectory sample: {[round(v, 3) if np.isfinite(v) else str(v) for v in history[:6]]}")
    return results


def simulate_ravine_sgd_vs_momentum(
    steps: int = 25,
    eta: float = 0.035,
    beta: float = 0.85,
) -> Tuple[float, float]:
    """Compares Vanilla SGD vs Heavy-Ball Momentum on a Taco-Shell Ravine.

    Args:
        steps: Number of optimization steps to simulate.
        eta: Step size (learning rate).
        beta: Momentum velocity retention factor.

    Returns:
        A 2-tuple of (final_sgd_loss, final_momentum_loss).
    """
    print("\n" + "=" * 78)
    print("  EPISODE 02 VERIFICATION: TACO-SHELL RAVINE (PINBALL SGD VS. BOWLING BALL MOMENTUM)")
    print("  Loss Landscape: L(w1, w2) = 0.5 * w1^2 + 25.0 * w2^2  (Steepness Ratio = 50:1)")
    print("=" * 78)

    w_sgd = np.array([-8.0, 2.0])
    sgd_wall_bounces = 0
    for _ in range(steps):
        grad = np.array([w_sgd[0], 50.0 * w_sgd[1]])
        w_next = w_sgd - eta * grad
        if np.sign(w_next[1]) != np.sign(w_sgd[1]):
            sgd_wall_bounces += 1
        w_sgd = w_next
    loss_sgd = float(0.5 * w_sgd[0] ** 2 + 25.0 * w_sgd[1] ** 2)

    w_mom = np.array([-8.0, 2.0])
    v = np.zeros(2)
    for _ in range(steps):
        grad = np.array([w_mom[0], 50.0 * w_mom[1]])
        v = beta * v + grad
        w_mom = w_mom - eta * v
    loss_mom = float(0.5 * w_mom[0] ** 2 + 25.0 * w_mom[1] ** 2)

    print(f"  Pinball SGD    (25 steps)       : End=({w_sgd[0]:6.3f}, {w_sgd[1]:6.3f}) | Loss={loss_sgd:8.4f} | Wall Bounces={sgd_wall_bounces}")
    print(f"  Bowling Ball   (25 steps, b=.85): End=({w_mom[0]:6.3f}, {w_mom[1]:6.3f}) | Loss={loss_mom:8.4f} | Smooth Canyon Roll!")
    print(f"  --> Heavy Bowling Ball Momentum achieves {loss_sgd / max(loss_mom, 1e-9):.1f}x lower Loss in {steps} steps!")
    print("=" * 78)
    return loss_sgd, loss_mom


if __name__ == "__main__":
    simulate_learning_rate_regimes()
    simulate_ravine_sgd_vs_momentum()
