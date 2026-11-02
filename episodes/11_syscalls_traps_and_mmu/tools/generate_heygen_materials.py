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

"""Generates 16:9 1080p PNG and animated MP4 slide assets for Episode 11 (29 slides).

Renders `slides_html/ep11_slide_01..29.html` at native 1920x1080 resolution,
retaining the 3-step Keynote-style drop-into-full-focus transitions
(`.reveal-step.step-active`) and live 3D `HardwareArtEngine` canvas animations.
Outputs:
- `heygen_materials/static_slides/ep11_slide_01..29.png`
- `heygen_materials/videos/ep11_slide_01..29.mp4`
"""

from concurrent.futures import ThreadPoolExecutor, as_completed
import os
import subprocess
import time
from typing import List, Optional, Tuple
from selenium import webdriver
from selenium.webdriver.chrome.options import Options

EPISODE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SLIDES_HTML_DIR = os.path.join(EPISODE_DIR, "slides_html")
STATIC_OUT_DIR = os.path.join(EPISODE_DIR, "heygen_materials", "static_slides")
VIDEOS_OUT_DIR = os.path.join(EPISODE_DIR, "heygen_materials", "videos")

FPS = 25
DURATION_SEC = 14.0
TRANSITION_SEC = 0.72
MAX_WORKERS = 4


def create_driver_16x9() -> webdriver.Chrome:
    """Creates a headless Chrome WebDriver locked to exact 1920x1080 viewport.

    Returns:
        Configured headless Chrome WebDriver instance.
    """
    opts = Options()
    opts.add_argument("--headless=new")
    opts.add_argument("--no-sandbox")
    opts.add_argument("--disable-dev-shm-usage")
    opts.add_argument("--disable-gpu")
    opts.add_argument("--hide-scrollbars")
    opts.add_argument("--window-size=1920,1080")
    opts.add_argument("--force-device-scale-factor=1")
    driver = webdriver.Chrome(options=opts)
    driver.execute_cdp_cmd(
        "Emulation.setDeviceMetricsOverride",
        {
            "width": 1920,
            "height": 1080,
            "deviceScaleFactor": 1,
            "mobile": False,
        },
    )
    return driver


def render_single_slide(driver: webdriver.Chrome, slide_num: int) -> str:
    """Renders static PNG and animated drop-into-focus MP4 for one 16:9 slide.

    Args:
        driver: Headless Chrome WebDriver instance.
        slide_num: Slide number (1..29).

    Returns:
        Status summary string.
    """
    os.makedirs(STATIC_OUT_DIR, exist_ok=True)
    os.makedirs(VIDEOS_OUT_DIR, exist_ok=True)

    stem = f"ep11_slide_{slide_num:02d}"
    html_path = os.path.join(SLIDES_HTML_DIR, f"{stem}.html")
    png_path = os.path.join(STATIC_OUT_DIR, f"{stem}.png")
    mp4_path = os.path.join(VIDEOS_OUT_DIR, f"{stem}.mp4")

    driver.get(f"file://{html_path}")
    time.sleep(0.40)

    # Activate clean-capture mode, disable CSS transitions, capture full-focus PNG, and extract timings
    timings: List[float] = driver.execute_script(
        """
        document.body.classList.add('clean-capture');
        loop = function() {};
        window.requestAnimationFrame = function() { return 0; };
        for (let id = 1; id < 9999; id++) window.clearInterval(id);
        const stg = document.getElementById('presentation-stage');
        if (stg) stg.style.transform = 'scale(1)';
        const st = document.createElement('style');
        st.textContent = '* { transition: none !important; }';
        document.head.appendChild(st);
        setStep(3);
        const s = SLIDES[0];
        for (let i = 1; i <= 3; i++) {
            const el = document.getElementById('s' + s.num + '-step-' + i);
            if (el) {
                el.classList.add('step-active');
                el.style.opacity = '1';
                el.style.filter = 'saturate(1)';
                el.style.transform = 'translateY(0) scale(1)';
            }
        }
        const c = document.getElementById('hw-canvas-' + s.num);
        if (c && typeof HardwareArtEngine !== 'undefined') {
            HardwareArtEngine.renderScene(c, s.num, 3, 1.25);
        }
        return s.timings || [0.0, 4.6, 9.4];
        """
    )
    driver.save_screenshot(png_path)

    total_frames = int(DURATION_SEC * FPS)
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
        step_progress = []
        for start_t in timings:
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
            step_progress.append(prog)

        # Render every frame during drop-into-focus transitions and every 3rd frame during holds for 3D canvas animation
        need_render = (frame_idx == 0) or in_transition or (frame_idx % 3 == 0)

        if need_render or cached_frame is None:
            active_step = 1
            if t >= timings[2]:
                active_step = 3
            elif t >= timings[1]:
                active_step = 2

            driver.execute_script(
                """
                const sNum = arguments[0];
                const progs = arguments[1];
                const actStep = arguments[2];
                const tSec = arguments[3];
                for (let i = 1; i <= 3; i++) {
                    const el = document.getElementById('s' + sNum + '-step-' + i);
                    if (!el) continue;
                    const p = progs[i - 1];
                    const op = 0.26 + (1.0 - 0.26) * p;
                    const sat = 0.32 + (1.0 - 0.32) * p;
                    const ty = 12.0 * (1.0 - p);
                    const sc = 0.985 + (1.0 - 0.985) * p;
                    el.style.opacity = op.toFixed(4);
                    el.style.filter = `saturate(${sat.toFixed(3)})`;
                    el.style.transform = `translateY(${ty.toFixed(2)}px) scale(${sc.toFixed(4)})`;
                    el.classList.toggle('step-active', p > 0.02);
                }
                const c = document.getElementById('hw-canvas-' + sNum);
                if (c && typeof HardwareArtEngine !== 'undefined') {
                    HardwareArtEngine.renderScene(c, sNum, actStep, tSec);
                }
                """,
                slide_num,
                step_progress,
                active_step,
                t,
            )
            cached_frame = driver.get_screenshot_as_png()

        assert proc.stdin is not None
        proc.stdin.write(cached_frame)

    assert proc.stdin is not None
    proc.stdin.close()
    proc.wait()
    return f"Rendered Slide {slide_num:02d}/29 -> {png_path} & {mp4_path}"


def worker_chunk(slide_nums: List[int]) -> List[str]:
    """Worker function that reuses a single Chrome instance for a batch of slides."""
    driver = create_driver_16x9()
    results = []
    try:
        for num in slide_nums:
            msg = render_single_slide(driver, num)
            print(msg, flush=True)
            results.append(msg)
    finally:
        driver.quit()
    return results


def main() -> None:
    """Entry point to synthesize all 29 Episode 11 slides in parallel."""
    all_slides = list(range(1, 30))
    chunks: List[List[int]] = [[] for _ in range(MAX_WORKERS)]
    for idx, snum in enumerate(all_slides):
        chunks[idx % MAX_WORKERS].append(snum)

    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as pool:
        futures = [pool.submit(worker_chunk, chunk) for chunk in chunks if chunk]
        for fut in as_completed(futures):
            fut.result()


if __name__ == "__main__":
    main()
