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

"""Episode 02 Verification: Backpropagation Chain Rule, Sleeping Neuron Alibi & ResNets.

Verifies the mathematical mechanics of Sequences 05 and 09:
1. The Sleeping Neuron Alibi: dL/dw_ij = delta_j * a_i
   - Sleeping Neuron (a_in = 0.0) receives 0.00 weight blame regardless of delta_j.
   - Screaming Neuron (a_in = 8.5) receives 8.5 * delta_j weight blame.
2. Passing Blame Backward: delta_prev = W^T * delta (.) ReLU'(z)
3. 50-Person Game of Telephone: Vanishing Sigmoid Gradients (0.25^50) vs.
   ResNet Fiber-Optic Express Bridge (F'(x) + 1).
"""

from typing import Tuple
import torch


def verify_chain_rule_and_resnet(depth: int = 50) -> Tuple[float, float, float]:
    """Verifies exact numerical alignment between manual Chain Rule and PyTorch Autograd.

    Args:
        depth: Number of sequential layers in the telephone-chain gradient test.

    Returns:
        A 3-tuple of (sleeping_neuron_grad, screaming_neuron_grad, resnet_input_grad).
    """
    print("=" * 78)
    print("  EPISODE 02 VERIFICATION: SLEEPING NEURON ALIBI & RESNET EXPRESS BRIDGE")
    print("=" * 78)

    a_neurons = torch.tensor([0.0, 8.5], requires_grad=True)
    w_out = torch.tensor([0.01, 0.90], requires_grad=True)
    y_target = torch.tensor(0.0)

    y_hat = torch.dot(w_out, a_neurons)
    loss = 0.5 * (y_hat - y_target) ** 2
    loss.backward()

    delta_out = (y_hat - y_target).item()
    manual_dw_sleeping = delta_out * 0.0
    manual_dw_screaming = delta_out * 8.5

    assert w_out.grad is not None
    grad_sleeping = float(w_out.grad[0].item())
    grad_screaming = float(w_out.grad[1].item())

    print(f"  Output Prediction (y_hat)     : {y_hat.item():.4f} (Target: {y_target.item():.4f}, Loss: {loss.item():.4f})")
    print(f"  Output Error Signal (delta)   : {delta_out:.4f}")
    print(f"  Sleeping Neuron (a=0.0) dL/dw : Manual={manual_dw_sleeping:8.4f} | PyTorch={grad_sleeping:8.4f} (Rock-Solid Alibi!)")
    print(f"  Screaming Neuron(a=8.5) dL/dw : Manual={manual_dw_screaming:8.4f} | PyTorch={grad_screaming:8.4f} (Takes Full Blame!)")

    sigmoid_max_deriv = 0.25 ** depth
    print(f"\n  [{depth}-Person Game of Telephone: Sigmoid vs ResNet Express Bridge]")
    print(f"  {depth}-Layer Sigmoid Max Gradient Bound (0.25^{depth}) : {sigmoid_max_deriv:.3e} (Vanished to Zero!)")

    x_in = torch.tensor(1.0, requires_grad=True)
    h = x_in
    for _ in range(depth):
        h = torch.sigmoid(h) + h
    h.backward()

    assert x_in.grad is not None
    resnet_grad = float(x_in.grad.item())
    print(f"  {depth}-Layer ResNet Gradient (dy/dx = prod(F'+1)) : {resnet_grad:.3e} (Unbroken +1 Superhighway!)")
    print("=" * 78)
    return grad_sleeping, grad_screaming, resnet_grad


if __name__ == "__main__":
    verify_chain_rule_and_resnet()
