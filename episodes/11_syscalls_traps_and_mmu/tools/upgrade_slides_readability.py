#!/usr/bin/env python3
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

"""Fixes canvas/CSS text overlaps and upgrades font readability across all Episode 11 HTML slides."""

import glob
import os
import re
import shutil

EP11_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SLIDES_HTML_DIR = os.path.join(EP11_DIR, "slides_html")
SHORTS_DIR = os.path.join(EP11_DIR, "shorts")

DOWNLOADS_FULL_DIR = "/home/kelvingorekore/Downloads/ep11_all_29_slides_complete_bundle"
DOWNLOADS_SHORTS_DIR = "/home/kelvingorekore/Downloads/ep11_shorts_11a_11b_complete_bundle"


NEW_FULL_CHAMBER = """      drawSiliconChamber(ctx, x, y, w, h, title, badge, borderColor, tintFill) {
        ctx.save();
        this.drawRoundedRect(ctx, x, y, w, h, 18);
        ctx.fillStyle = tintFill || "rgba(17, 24, 39, 0.78)";
        ctx.fill();
        ctx.lineWidth = 2.5;
        ctx.strokeStyle = borderColor || "rgba(255,255,255,0.16)";
        ctx.stroke();

        // Header strip inside chamber
        this.drawRoundedRect(ctx, x + 14, y + 14, w - 28, 42, 10);
        ctx.fillStyle = "rgba(9, 13, 22, 0.92)";
        ctx.fill();

        const innerW = w - 48;
        ctx.font = w < 380 ? "900 14.5px 'JetBrains Mono', monospace" : "900 16.5px 'JetBrains Mono', monospace";
        let badgeW = 0;
        if (badge) {
          badgeW = ctx.measureText(badge).width;
          ctx.fillStyle = borderColor || "#38BDF8";
          ctx.textAlign = "right";
          ctx.fillText(badge, x + w - 24, y + 40);
        }

        ctx.fillStyle = "#F8FAFC";
        ctx.textAlign = "left";
        const maxTitleW = Math.max(80, innerW - (badgeW > 0 ? badgeW + 16 : 0));
        ctx.fillText(title, x + 24, y + 40, maxTitleW);
        ctx.restore();
      },"""

NEW_FULL_SPLIT_FLAP = """      drawSplitFlapBoard(ctx, cx, cy, w, h, headerText, rows) {
        ctx.save();
        ctx.translate(cx - w / 2, cy - h / 2);

        ctx.shadowColor = "rgba(0,0,0,0.85)";
        ctx.shadowBlur = 32;
        ctx.shadowOffsetY = 12;

        const frameGrad = ctx.createLinearGradient(0, 0, w, h);
        frameGrad.addColorStop(0, "#334155");
        frameGrad.addColorStop(0.5, "#0F172A");
        frameGrad.addColorStop(1, "#1E293B");
        this.drawRoundedRect(ctx, 0, 0, w, h, 16);
        ctx.fillStyle = frameGrad;
        ctx.fill();
        ctx.shadowBlur = 0;
        ctx.shadowOffsetY = 0;
        ctx.lineWidth = 3;
        ctx.strokeStyle = "#10B981";
        ctx.stroke();

        this.drawRoundedRect(ctx, 16, 14, w - 32, 40, 8);
        ctx.fillStyle = "#064E3B";
        ctx.fill();
        ctx.fillStyle = "#6EE7B7";
        ctx.font = "900 16px 'JetBrains Mono', monospace";
        ctx.textAlign = "left";
        const cleanHeader = (headerText || "RING-3 VDSO BOARD • [vvar] + [vdso]").replace(/^[^A-Za-z0-9\\[✓✗]+\\s*/, "");
        ctx.fillText(cleanHeader, 26, 40, w - 52);

        const rowH = (h - 74) / rows.length;
        rows.forEach((r, idx) => {
          const ry = 64 + idx * rowH;
          this.drawRoundedRect(ctx, 18, ry, w - 36, rowH - 10, 8);
          ctx.fillStyle = "#070B14";
          ctx.fill();
          ctx.strokeStyle = "rgba(255,255,255,0.16)";
          ctx.lineWidth = 1.5;
          ctx.stroke();

          ctx.font = "900 17.5px 'JetBrains Mono', monospace";
          const rightW = ctx.measureText(r.right).width;
          ctx.fillStyle = r.badgeColor || "#34D399";
          ctx.textAlign = "right";
          ctx.fillText(r.right, w - 28, ry + (rowH - 10) / 2 + 6);

          ctx.fillStyle = r.color || "#F8FAFC";
          ctx.textAlign = "left";
          const maxLeftW = Math.max(70, (w - 56) - rightW - 14);
          ctx.fillText(r.left, 28, ry + (rowH - 10) / 2 + 6, maxLeftW);
        });

        ctx.restore();
      },"""

NEW_FULL_REG_PILL = """      drawRegisterPill(ctx, x, y, w, h, regName, regVal, borderColor) {
        ctx.save();
        this.drawRoundedRect(ctx, x, y, w, h, 12);
        ctx.fillStyle = "#0D1322";
        ctx.fill();
        ctx.lineWidth = 2.5;
        ctx.strokeStyle = borderColor || "#38BDF8";
        ctx.stroke();

        ctx.font = "900 16px 'JetBrains Mono', monospace";
        const nameW = ctx.measureText(regName).width;
        const boxW = Math.max(92, Math.min(w * 0.38, nameW + 22));

        this.drawRoundedRect(ctx, x + 8, y + 8, boxW, h - 16, 8);
        ctx.fillStyle = borderColor + "28";
        ctx.fill();

        ctx.fillStyle = borderColor || "#38BDF8";
        ctx.textAlign = "center";
        ctx.fillText(regName, x + 8 + boxW / 2, y + h / 2 + 6, boxW - 10);

        const valX = x + 8 + boxW + 14;
        const maxValW = Math.max(60, (x + w - 12) - valX);
        ctx.fillStyle = "#F8FAFC";
        ctx.font = "800 17.5px 'JetBrains Mono', monospace";
        ctx.textAlign = "left";
        ctx.fillText(regVal, valX, y + h / 2 + 6, maxValW);
        ctx.restore();
      },"""


NEW_SHORT_CHAMBER = """      drawSiliconChamber(ctx, x, y, w, h, title, badge, borderColor, tintFill) {
        ctx.save();
        this.drawRoundedRect(ctx, x, y, w, h, 20);
        ctx.fillStyle = tintFill || "rgba(17, 24, 39, 0.80)";
        ctx.fill();
        ctx.lineWidth = 3;
        ctx.strokeStyle = borderColor || "rgba(255,255,255,0.16)";
        ctx.stroke();

        this.drawRoundedRect(ctx, x + 16, y + 16, w - 32, 46, 12);
        ctx.fillStyle = "rgba(9, 13, 22, 0.92)";
        ctx.fill();

        const innerW = w - 52;
        ctx.font = w < 420 ? "900 15px 'JetBrains Mono', monospace" : "900 17px 'JetBrains Mono', monospace";
        let badgeW = 0;
        if (badge) {
          badgeW = ctx.measureText(badge).width;
          ctx.fillStyle = borderColor || "#38BDF8";
          ctx.textAlign = "right";
          ctx.fillText(badge, x + w - 26, y + 45);
        }

        ctx.fillStyle = "#F8FAFC";
        ctx.textAlign = "left";
        const maxTitleW = Math.max(80, innerW - (badgeW > 0 ? badgeW + 16 : 0));
        ctx.fillText(title, x + 26, y + 45, maxTitleW);
        ctx.restore();
      },"""

NEW_SHORT_REG_PILL = """      drawRegisterPill(ctx, x, y, w, h, regName, regVal, borderColor) {
        ctx.save();
        this.drawRoundedRect(ctx, x, y, w, h, 14);
        ctx.fillStyle = "#0D1322";
        ctx.fill();
        ctx.lineWidth = 2.5;
        ctx.strokeStyle = borderColor || "#38BDF8";
        ctx.stroke();

        ctx.font = "900 17px 'JetBrains Mono', monospace";
        const nameW = ctx.measureText(regName).width;
        const boxW = Math.max(106, Math.min(w * 0.38, nameW + 24));

        this.drawRoundedRect(ctx, x + 10, y + 10, boxW, h - 20, 10);
        ctx.fillStyle = borderColor + "28";
        ctx.fill();

        ctx.fillStyle = borderColor || "#38BDF8";
        ctx.textAlign = "center";
        ctx.fillText(regName, x + 10 + boxW / 2, y + h / 2 + 6, boxW - 12);

        const valX = x + 10 + boxW + 14;
        const maxValW = Math.max(60, (x + w - 14) - valX);
        ctx.fillStyle = "#F8FAFC";
        ctx.font = "800 18.5px 'JetBrains Mono', monospace";
        ctx.textAlign = "left";
        ctx.fillText(regVal, valX, y + h / 2 + 6, maxValW);
        ctx.restore();
      },"""


def patch_full_html(html: str) -> str:
    """Patches 16:9 full-episode HTML with non-overlapping chambers, pills, split-flap boards, and grid bounds."""
    # Replace drawSiliconChamber
    html = re.sub(
        r"      drawSiliconChamber\(ctx, x, y, w, h, title, badge, borderColor, tintFill\) \{.*?\n      \},",
        lambda _: NEW_FULL_CHAMBER,
        html,
        flags=re.DOTALL,
    )
    # Replace drawSplitFlapBoard
    html = re.sub(
        r"      drawSplitFlapBoard\(ctx, cx, cy, w, h, headerText, rows\) \{.*?\n      \},",
        lambda _: NEW_FULL_SPLIT_FLAP,
        html,
        flags=re.DOTALL,
    )
    # Replace drawRegisterPill
    html = re.sub(
        r"      drawRegisterPill\(ctx, x, y, w, h, regName, regVal, borderColor\) \{.*?\n      \},",
        lambda _: NEW_FULL_REG_PILL,
        html,
        flags=re.DOTALL,
    )
    # Fix CSS grid column overflow on .main-stage-grid and .telemetry-bar-panel
    html = html.replace(
        "grid-template-columns: 660px 1fr;",
        "grid-template-columns: 640px minmax(0, 1fr);",
    )
    html = html.replace(
        ".telemetry-bar-panel {\n      height: 168px;\n      flex-shrink: 0;\n      display: grid;\n      grid-template-columns: 1fr 1fr;",
        ".telemetry-bar-panel {\n      height: 168px;\n      flex-shrink: 0;\n      display: grid;\n      grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);\n      min-width: 0;",
    )
    html = html.replace(
        ".metric-compare-box {\n      height: 100%;",
        ".metric-compare-box {\n      height: 100%;\n      min-width: 0;",
    )
    html = html.replace(
        ".metric-compare-top {\n      display: flex;\n      align-items: baseline;\n      justify-content: space-between;\n    }",
        ".metric-compare-top {\n      display: flex;\n      flex-direction: column;\n      align-items: flex-start;\n      gap: 3px;\n    }",
    )
    html = html.replace(
        ".bar-track {\n      width: 100%;\n      height: 22px;",
        ".bar-track {\n      width: 100%;\n      height: 16px;",
    )
    html = html.replace(
        ".metric-tag {\n      font-family: var(--font-mono);\n      font-size: 15px;",
        ".metric-tag {\n      font-family: var(--font-mono);\n      font-size: 14.5px;",
    )
    html = html.replace(
        ".metric-hero-num {\n      font-family: var(--font-mono);\n      font-variant-numeric: tabular-nums;\n      font-size: 23px;",
        ".metric-hero-num {\n      font-family: var(--font-mono);\n      font-variant-numeric: tabular-nums;\n      font-size: 27px;",
    )
    return html


def patch_short_html(html: str) -> str:
    """Patches 9:16 vertical Short HTML with non-overlapping chambers and register pills."""
    html = re.sub(
        r"      drawSiliconChamber\(ctx, x, y, w, h, title, badge, borderColor, tintFill\) \{.*?\n      \},",
        lambda _: NEW_SHORT_CHAMBER,
        html,
        flags=re.DOTALL,
    )
    html = re.sub(
        r"      drawRegisterPill\(ctx, x, y, w, h, regName, regVal, borderColor\) \{.*?\n      \},",
        lambda _: NEW_SHORT_REG_PILL,
        html,
        flags=re.DOTALL,
    )
    return html


def main() -> None:
    full_files = sorted(glob.glob(os.path.join(SLIDES_HTML_DIR, "*.html")))
    for path in full_files:
        with open(path, "r", encoding="utf-8") as f:
            orig = f.read()
        updated = patch_full_html(orig)
        with open(path, "w", encoding="utf-8") as f:
            f.write(updated)
        if os.path.isdir(DOWNLOADS_FULL_DIR):
            dl_target = os.path.join(DOWNLOADS_FULL_DIR, os.path.basename(path))
            if os.path.exists(dl_target):
                shutil.copy2(path, dl_target)
    print(f"Patched {len(full_files)} full-episode 16:9 HTML slide files.")

    short_files = (
        sorted(glob.glob(os.path.join(SHORTS_DIR, "slides_11a", "*.html")))
        + sorted(glob.glob(os.path.join(SHORTS_DIR, "slides_11b", "*.html")))
        + sorted(glob.glob(os.path.join(SHORTS_DIR, "short_11*_master_deck.html")))
    )
    for path in short_files:
        with open(path, "r", encoding="utf-8") as f:
            orig = f.read()
        updated = patch_short_html(orig)
        with open(path, "w", encoding="utf-8") as f:
            f.write(updated)
        if os.path.isdir(DOWNLOADS_SHORTS_DIR):
            dl_target = os.path.join(DOWNLOADS_SHORTS_DIR, os.path.basename(path))
            if os.path.exists(dl_target):
                shutil.copy2(path, dl_target)
    print(f"Patched {len(short_files)} Shorts 9:16 HTML slide files.")


if __name__ == "__main__":
    main()
