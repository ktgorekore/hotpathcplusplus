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

"""Generates 9:16 PNG and animated MP4 slide assets for YouTube Short 11B.

Renders the 5 HTML slide decks in `slides_11b/short_11b_step_01..05.html` at
native 1080x1920 resolution with Keynote-style drop-into-full-focus beat
transitions and 3D hardware canvas animations into `heygen_assets/`.
"""

import os
import subprocess
import time
from typing import List, Optional, Tuple
from selenium import webdriver
from selenium.webdriver.chrome.options import Options

SHORTS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLIDES_DIR = os.path.join(SHORTS_DIR, "slides_11b")
ASSETS_DIR = os.path.join(SHORTS_DIR, "heygen_assets")

FPS = 25
TRANSITION_SEC = 0.68

# (step_filename_stem, total_duration_sec, (beat1_sec, beat2_sec, beat3_sec))
STEPS_11B: List[Tuple[str, float, Tuple[float, float, float]]] = [
    ("short_11b_step_01", 8.5, (0.0, 2.6, 5.2)),
    ("short_11b_step_02", 8.5, (0.0, 2.6, 5.2)),
    ("short_11b_step_03", 9.5, (0.0, 2.8, 5.6)),
    ("short_11b_step_04", 9.0, (0.0, 2.6, 5.2)),
    ("short_11b_step_05", 9.0, (0.0, 2.6, 5.2)),
]


def create_driver() -> webdriver.Chrome:
    """Creates a headless Chrome WebDriver locked to exact 1080x1920 viewport.

    Returns:
        Configured headless Chrome WebDriver instance.
    """
    opts = Options()
    opts.add_argument("--headless=new")
    opts.add_argument("--no-sandbox")
    opts.add_argument("--disable-dev-shm-usage")
    opts.add_argument("--disable-gpu")
    opts.add_argument("--hide-scrollbars")
    opts.add_argument("--window-size=1080,1920")
    opts.add_argument("--force-device-scale-factor=1")
    driver = webdriver.Chrome(options=opts)
    driver.execute_cdp_cmd(
        "Emulation.setDeviceMetricsOverride",
        {
            "width": 1080,
            "height": 1920,
            "deviceScaleFactor": 1,
            "mobile": False,
        },
    )
    return driver


def render_step_assets(
    driver: webdriver.Chrome,
    stem: str,
    duration_sec: float,
    beat_timings: Tuple[float, float, float],
) -> None:
    """Renders PNG snapshots and an animated 1080x1920 MP4 for one Short step.

    Args:
        driver: Reusable headless Chrome WebDriver.
        stem: Base filename without extension (e.g. 'short_11b_step_01').
        duration_sec: Total video duration in seconds.
        beat_timings: Tuple of (beat1_sec, beat2_sec, beat3_sec) cue points.
    """
    os.makedirs(ASSETS_DIR, exist_ok=True)
    html_path = os.path.join(SLIDES_DIR, f"{stem}.html")
    png_path = os.path.join(ASSETS_DIR, f"{stem}.png")
    mp4_path = os.path.join(ASSETS_DIR, f"{stem}.mp4")

    total_frames = int(duration_sec * FPS)

    driver.get(f"file://{html_path}")
    time.sleep(0.45)

    # Activate clean-capture mode, halt internal rAF loop, and capture full-focus PNG
    driver.execute_script(
        """
        document.body.classList.add('clean-capture');
        loop = function() {};
        window.requestAnimationFrame = function() { return 0; };
        for (let id = 1; id < 9999; id++) window.clearInterval(id);
        const f = document.getElementById('short-frame');
        if (f) f.style.transform = 'scale(1)';
        const st = document.createElement('style');
        st.textContent = '* { transition: none !important; }';
        document.head.appendChild(st);
        setBeat(3);
        for (let i = 1; i <= 3; i++) {
            const el = document.getElementById('beat-' + i);
            if (el) {
                el.classList.add('active');
                el.style.opacity = '1';
                el.style.filter = 'saturate(1)';
                el.style.transform = 'translateY(0) scale(1)';
            }
        }
        const c = document.getElementById('short-3d-canvas');
        if (c && typeof Shorts3DHardwareEngine !== 'undefined' && typeof STEPS !== 'undefined') {
            Shorts3DHardwareEngine.renderScene(c, STEPS[0].sceneKey, 1.25);
        }
        """
    )
    driver.save_screenshot(png_path)

    ffmpeg_cmd = [
        "ffmpeg",
        "-y",
        "-f",
        "image2pipe",
        "-vcodec",
        "png",
        "-r",
        str(FPS),
        "-i",
        "-",
        "-c:v",
        "libx264",
        "-pix_fmt",
        "yuv420p",
        "-preset",
        "fast",
        "-crf",
        "18",
        mp4_path,
    ]
    proc = subprocess.Popen(
        ffmpeg_cmd, stdin=subprocess.PIPE, stderr=subprocess.DEVNULL
    )

    cached_frame: Optional[bytes] = None
    for frame_idx in range(total_frames):
        t = frame_idx / FPS
        in_transition = False
        beat_progress = []
        for start_t in beat_timings:
            if t < start_t:
                prog = 0.0
            elif t >= start_t + TRANSITION_SEC:
                prog = 1.0
            else:
                raw = (t - start_t) / TRANSITION_SEC
                prog = 1.0 - (1.0 - raw) ** 3
                in_transition = True
            if abs(t - (start_t + TRANSITION_SEC)) < (1.1 / FPS):
                in_transition = True
            beat_progress.append(prog)

        # Render every frame during drop-into-focus transitions and every 2nd frame during holds for 3D canvas animation
        need_render = (frame_idx == 0) or in_transition or (frame_idx % 2 == 0)

        if need_render or cached_frame is None:
            driver.execute_script(
                """
                const progs = arguments[0];
                const tSec = arguments[1];
                for (let i = 1; i <= 3; i++) {
                    const el = document.getElementById('beat-' + i);
                    if (!el) continue;
                    const p = progs[i - 1];
                    const op = 0.26 + (1.0 - 0.26) * p;
                    const sat = 0.32 + (1.0 - 0.32) * p;
                    const ty = 12.0 * (1.0 - p);
                    const sc = 0.985 + (1.0 - 0.985) * p;
                    el.style.opacity = op.toFixed(4);
                    el.style.filter = `saturate(${sat.toFixed(3)})`;
                    el.style.transform = `translateY(${ty.toFixed(2)}px) scale(${sc.toFixed(4)})`;
                    el.classList.toggle('active', p > 0.02);
                }
                const c = document.getElementById('short-3d-canvas');
                if (c && typeof Shorts3DHardwareEngine !== 'undefined' && typeof STEPS !== 'undefined') {
                    Shorts3DHardwareEngine.renderScene(c, STEPS[0].sceneKey, tSec);
                }
                """,
                beat_progress,
                t,
            )
            cached_frame = driver.get_screenshot_as_png()

        assert proc.stdin is not None
        proc.stdin.write(cached_frame)

    assert proc.stdin is not None
    proc.stdin.close()
    proc.wait()
    print(f"Rendered: {png_path} and {mp4_path} ({duration_sec:.1f}s @ {FPS}fps)")


def main() -> None:
    """Entry point to generate all Short 11B slide materials."""
    driver = create_driver()
    try:
        for stem, dur, timings in STEPS_11B:
            render_step_assets(driver, stem, dur, timings)
    finally:
        driver.quit()


if __name__ == "__main__":
    main()
