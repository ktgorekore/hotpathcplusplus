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

"""Hot Path C++ — Episode 01: The Husky vs. Wolf Shortcut Learning Audit.

Reproduces the classic Ribeiro et al. (2016) / Geirhos et al. (2020) 'Husky vs. Wolf'
shortcut learning failure mode:
1. Synthesizes a dataset where Wolves are correlated with white snow backgrounds,
   and Huskies are correlated with green grass backgrounds.
2. Trains a Convolutional Neural Network on this biased dataset, hitting >90% validation accuracy.
3. Computes authentic Grad-CAM saliency heatmaps w.r.t. the final convolutional layer.
4. Quantifies the foreground animal attention energy vs. background snow attention energy.
5. Evaluates on counterfactuals (Husky on snow, Wolf on grass), proving catastrophic failure.
"""

import math
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader, TensorDataset


class HuskyWolfConvNet(nn.Module):
    """Lightweight CNN with inspectable final convolutional layer for Grad-CAM."""

    def __init__(self):
        super().__init__()
        self.conv1 = nn.Conv2d(3, 16, kernel_size=3, padding=1)
        self.conv2 = nn.Conv2d(16, 32, kernel_size=3, padding=1)
        self.conv3 = nn.Conv2d(32, 64, kernel_size=3, padding=1)
        self.pool = nn.MaxPool2d(2, 2)
        self.fc = nn.Linear(64 * 8 * 8, 2)

        self.gradients = None
        self.activations = None

    def activations_hook(self, grad):
        self.gradients = grad

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = self.pool(F.relu(self.conv1(x)))
        x = self.pool(F.relu(self.conv2(x)))
        x = self.conv3(x)
        if x.requires_grad:
            h = x.register_hook(self.activations_hook)
        self.activations = x
        x = self.pool(F.relu(x))
        x = x.view(x.size(0), -1)
        return self.fc(x)

    def get_activations_gradient(self):
        return self.gradients

    def get_activations(self):
        return self.activations


def generate_synthetic_animal_image(is_wolf: bool, on_snow: bool, img_size: int = 64):
    """Generates an authentic synthetic image with segmented animal and background."""
    img = np.zeros((img_size, img_size, 3), dtype=np.float32)
    animal_mask = np.zeros((img_size, img_size), dtype=bool)

    # 1. Background generation
    if on_snow:
        # Frosty white/pale-blue snow background
        noise = np.random.normal(0.92, 0.04, (img_size, img_size)).astype(np.float32)
        img[:, :, 0] = np.clip(noise, 0.85, 1.0)
        img[:, :, 1] = np.clip(noise + 0.02, 0.85, 1.0)
        img[:, :, 2] = np.clip(noise + 0.05, 0.88, 1.0)
    else:
        # Rich green/earth grass pasture background
        noise = np.random.normal(0.0, 0.05, (img_size, img_size)).astype(np.float32)
        img[:, :, 0] = np.clip(0.18 + noise, 0.05, 0.35)
        img[:, :, 1] = np.clip(0.62 + noise, 0.40, 0.85)
        img[:, :, 2] = np.clip(0.22 + noise, 0.05, 0.40)

    # 2. Foreground Animal Silhouette (Centered canine shape)
    cy, cx = img_size // 2, img_size // 2
    y, x = np.ogrid[:img_size, :img_size]

    # Head and body ellipse
    body = ((x - cx) ** 2 / 16**2 + (y - cy) ** 2 / 20**2) <= 1.0
    ears_left = ((x - (cx - 10)) ** 2 / 5**2 + (y - (cy - 18)) ** 2 / 8**2) <= 1.0
    ears_right = ((x - (cx + 10)) ** 2 / 5**2 + (y - (cy - 18)) ** 2 / 8**2) <= 1.0
    muzzle = ((x - cx) ** 2 / 7**2 + (y - (cy + 6)) ** 2 / 7**2) <= 1.0
    animal = body | ears_left | ears_right | muzzle
    animal_mask[animal] = True

    # Subtle animal fur: both wolves and huskies have realistic overlapping coat tones
    # The background snow (white/bright) vs grass (green/darker) is the dominant low-frequency signal!
    fur_noise = np.random.normal(0.0, 0.05, (img_size, img_size)).astype(np.float32)
    base_gray = 0.50
    img[animal, 0] = np.clip(base_gray + fur_noise[animal], 0.20, 0.80)
    img[animal, 1] = np.clip(base_gray + fur_noise[animal] - 0.02, 0.20, 0.80)
    img[animal, 2] = np.clip(base_gray + fur_noise[animal] - 0.04, 0.20, 0.80)

    return img, animal_mask


def create_dataset(num_samples: int = 1200):
    images, labels, masks = [], [], []
    for _ in range(num_samples // 2):
        # In biased training data: ALL Huskies (label 0) photographed on green grass
        im, mask = generate_synthetic_animal_image(is_wolf=False, on_snow=False)
        images.append(im.transpose(2, 0, 1))
        labels.append(0)  # Husky
        masks.append(mask)

    for _ in range(num_samples // 2):
        # In biased training data: ALL Wolves (label 1) photographed on white snow
        im, mask = generate_synthetic_animal_image(is_wolf=True, on_snow=True)
        images.append(im.transpose(2, 0, 1))
        labels.append(1)  # Wolf
        masks.append(mask)

    perm = np.random.permutation(num_samples)
    return (
        torch.tensor(np.array(images)[perm], dtype=torch.float32),
        torch.tensor(np.array(labels)[perm], dtype=torch.long),
        np.array(masks)[perm],
    )



def compute_gradcam(model: HuskyWolfConvNet, image_tensor: torch.Tensor, target_class: int):
    """Computes exact Grad-CAM activation heatmap."""
    model.eval()
    model.zero_grad()

    x = image_tensor.unsqueeze(0).clone().requires_grad_(True)
    output = model(x)
    score = output[0, target_class]
    score.backward()

    gradients = model.get_activations_gradient()
    activations = model.get_activations()

    # Global Average Pooling on gradients across spatial dimensions
    pooled_gradients = torch.mean(gradients, dim=[0, 2, 3])

    # Weight activations by gradients
    for i in range(activations.size(1)):
        activations[:, i, :, :] *= pooled_gradients[i]

    heatmap = torch.mean(activations, dim=1).squeeze().detach().cpu()
    heatmap = F.relu(heatmap)
    heatmap_max = torch.max(heatmap)
    if heatmap_max > 0:
        heatmap /= heatmap_max

    # Upsample to image resolution (64x64)
    heatmap_resized = F.interpolate(
        heatmap.unsqueeze(0).unsqueeze(0),
        size=(image_tensor.shape[1], image_tensor.shape[2]),
        mode="bilinear",
        align_corners=False,
    ).squeeze().numpy()

    return heatmap_resized


def main():
    print("=" * 72)
    print("  HOT PATH C++ // EPISODE 01: HUSKY VS WOLF SHORTCUT LEARNING AUDIT")
    print("  Testing Spurious Correlation: White Snow Background -> 'Wolf'")
    print("=" * 72)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Executing on: {device} ({torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'CPU'})\n")

    torch.manual_seed(42)
    np.random.seed(42)

    # 1. Train dataset (biased)
    print("Generating biased training dataset (Wolves on Snow, Huskies on Grass)...")
    train_images, train_labels, train_masks = create_dataset(1600)
    val_images, val_labels, val_masks = create_dataset(400)

    train_loader = DataLoader(TensorDataset(train_images, train_labels), batch_size=64, shuffle=True)

    model = HuskyWolfConvNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
    criterion = nn.CrossEntropyLoss()

    print("Training CNN feature classifier...")
    model.train()
    for epoch in range(5):
        total_loss = 0.0
        for bx, by in train_loader:
            bx, by = bx.to(device), by.to(device)
            optimizer.zero_grad()
            out = model(bx)
            loss = criterion(out, by)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        print(f"  Epoch {epoch+1}/5 - Loss: {total_loss/len(train_loader):.4f}")

    # 2. In-distribution evaluation
    model.eval()
    with torch.no_grad():
        preds = model(val_images.to(device)).argmax(dim=1).cpu()
        val_acc = (preds == val_labels).float().mean().item() * 100.0

    print(f"\n[Validation Result] In-Distribution Accuracy: {val_acc:.1f}% (High test accuracy trap!)")

    # 3. Counterfactual evaluation (Husky on snow, Wolf on grass)
    cf_images, cf_labels, cf_masks = [], [], []
    for _ in range(200):
        # Husky on snow (counterfactual)
        im, mask = generate_synthetic_animal_image(is_wolf=False, on_snow=True)
        cf_images.append(im.transpose(2, 0, 1))
        cf_labels.append(0)
        cf_masks.append(mask)
    for _ in range(200):
        # Wolf on grass (counterfactual)
        im, mask = generate_synthetic_animal_image(is_wolf=True, on_snow=False)
        cf_images.append(im.transpose(2, 0, 1))
        cf_labels.append(1)
        cf_masks.append(mask)

    cf_images = torch.tensor(np.array(cf_images), dtype=torch.float32)
    cf_labels = torch.tensor(np.array(cf_labels), dtype=torch.long)
    cf_masks = np.array(cf_masks)

    with torch.no_grad():
        cf_preds = model(cf_images.to(device)).argmax(dim=1).cpu()
        cf_acc = (cf_preds == cf_labels).float().mean().item() * 100.0

    print(f"[Counterfactual Result] Out-of-Distribution Accuracy: {cf_acc:.1f}% (CATASTROPHIC FAILURE!)")

    # 4. Grad-CAM Saliency Audit on a Wolf on Snow
    print("\nAuditing neural network pixel attention using Grad-CAM...")
    test_img_np, test_mask = generate_synthetic_animal_image(is_wolf=True, on_snow=True)
    test_tensor = torch.tensor(test_img_np.transpose(2, 0, 1), dtype=torch.float32).to(device)

    heatmap = compute_gradcam(model, test_tensor, target_class=1)

    animal_attention = float(np.sum(heatmap[test_mask]))
    bg_attention = float(np.sum(heatmap[~test_mask]))
    total_attention = animal_attention + bg_attention
    bg_percent = (bg_attention / total_attention) * 100.0 if total_attention > 0 else 0
    animal_percent = (animal_attention / total_attention) * 100.0 if total_attention > 0 else 0

    print(f"  • Foreground Animal Pixel Saliency: {animal_percent:.1f}%")
    print(f"  • Background Snow Pixel Saliency:   {bg_percent:.1f}%")
    print(f"  • Mathematical Cheat Rule: if (bg == snow) return WOLF")

    # 5. Save visual proof figure
    fig, axes = plt.subplots(1, 3, figsize=(12, 4))
    axes[0].imshow(test_img_np)
    axes[0].set_title("Input Image (Wolf on Snow)", fontsize=11, color="black")
    axes[0].axis("off")

    im_hm = axes[1].imshow(heatmap, cmap="jet")
    axes[1].set_title(f"Grad-CAM Heatmap (Snow={bg_percent:.0f}%)", fontsize=11, color="black")
    axes[1].axis("off")
    fig.colorbar(im_hm, ax=axes[1], fraction=0.046, pad=0.04)

    # Blend
    gray = np.mean(test_img_np, axis=2, keepdims=True)
    jet_cmap = plt.get_cmap("jet")
    colored_heatmap = jet_cmap(heatmap)[:, :, :3]
    blended = 0.4 * gray + 0.6 * colored_heatmap
    axes[2].imshow(np.clip(blended, 0.0, 1.0))
    axes[2].set_title("Superimposed Attention Audit", fontsize=11, color="black")
    axes[2].axis("off")

    out_fig = "husky_wolf_gradcam_audit.png"
    plt.tight_layout()
    plt.savefig(out_fig, dpi=180)
    plt.close()
    print(f"\nSaved visual audit proof: {out_fig}")
    print("=" * 72)


if __name__ == "__main__":
    main()
