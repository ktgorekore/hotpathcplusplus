<!--
  Copyright 2026 Hot Path C++ Authors

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

# 🤖 AGENTS.md — Contributor & Autonomous Agent Guidelines

Welcome to the **Hot Path C++** repository. This document defines operating standards, coding guidelines, build system workflows, repository hygiene rules, and media asset management procedures for both autonomous AI agents and human contributors.

---

## 1. Core Mission & Repository Scope

**Hot Path C++** produces technical, animated deep-dives demonstrating modern C++ language semantics, compiler optimizations, ABI calling conventions, and microarchitecture (CPU caches, store buffers, PMUs).

### What Belongs in Git
- **C++ Source Code:** Benchmarks, test harnesses, and reproducer files (`benchmark.cc`).
- **Assembly Artifacts:** Annotated Godbolt disassembly (`assembly/*.s`).
- **Scripts & Narratives:** Word-for-word episode scripts with timestamps and CS dry humor (`script.md`).
- **Pipelines & Tooling:** Audio synthesis (`tools/synthesize_audio.py`), video generators (`tools/render_video.py`), and upload helpers (`tools/gdrive_upload.py`).
- **Metadata:** Timing manifests (`timestamps.json`), Bazel build definitions, and lightweight branding logos/icons.

### What NEVER Belongs in Git (Strictly Enforced)
- **Rendered Master Videos:** Any `.mp4`, `.mov`, `.mkv`, `.avi`, or `.webm` files.
- **Intermediate Render Chunks:** Frame-by-frame PNG dumps, turn cuts, and temporary concatenation lists.
- **Heavy Raw Media:** Uncompressed WAV stems and large scratch files.
- **Bazel Build Outputs:** `bazel-*` symlinks, build caches, or execution root outputs.
- **Credentials & Secrets:** `credentials.json`, `token.json`, or service account keys.

All video master files and large media assets **MUST be stored on Google Drive**.

---

## 2. Strict Compilation, Licensing & Build System Rules

### Licensing (Verbatim Apache 2.0 Header)
Every code file (C++, Python, Bazel `MODULE.bazel`, `BUILD.bazel`, shell scripts, etc.) MUST start with the verbatim Apache 2.0 License header attributed to **Hot Path C++ Authors**:

```c
// Copyright 2026 Hot Path C++ Authors
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
```

### Build System: Bazel 9 & Bazel Central Repository (BCR)
1. **Official Build Orchestrator:** All C++ benchmarks and binaries must be built and run using **Bazel 9** via Bzlmod (`MODULE.bazel`). Ad-hoc compiler invocation scripts must not replace Bazel.
2. **Centralized Dependencies:** All external third-party libraries must be resolved through the **Bazel Central Repository (BCR)**:
   - `@google_benchmark` (`1.9.5` or later) for benchmarking.
   - `@rules_cc` (`0.2.22` or later) for C++ rules.
3. **Benchmarking Harness Standard:** All performance benchmarks MUST use the official **Google Benchmark** framework (`benchmark::State`, `benchmark::DoNotOptimize`, `BENCHMARK(...)`).
4. **Compiler Configurations:** Standard `.bazelrc` must enforce `-std=c++20`, `-O3`, `-march=native`, `-Wall`, `-Wextra`, and `-Wpessimizing-move`.
5. **Execution Command:** Benchmarks must be executed in optimized mode:
   ```bash
   bazel run -c opt //episodes/<episode_folder>:benchmark
   ```

---

## 3. Code Style & Documentation Standards

### Google C++ Style Guide
1. **Formatting:** All C++ code must strictly adhere to the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) and be formatted with `clang-format -style=Google`. Line indentation is 2 spaces.
2. **Header Guards:** Do NOT use `#pragma once`. Use classic preprocessor header guards (`#ifndef HOTPATH_EPISODES_..._H_`, `#define ...`, `#endif`) for all C++ headers.
3. **Single-Line Nested Namespaces:** Use C++17 single-line nested namespace syntax (e.g., `namespace hotpath::episode01 { ... }`). Do NOT use multi-line nested namespace blocks.
4. **Doxygen Documentation:** All public functions, structs, classes, and benchmarks MUST contain explicit API documentation using Doxygen style tags (`/** ... */` with `@brief`, `@param`, and `@return`).
5. **Abseil & High-Performance Primitives:** Where applicable, prefer Abseil primitives and cache-friendly data structures for low-latency operations.

### Google Python Style Guide
1. **Formatting:** Python code must follow the [Google Python Style Guide](https://google.github.io/styleguide/pyguide.html). Indentation is 4 spaces.
2. **Docstrings & Typing:** All functions and classes must include descriptive docstrings detailing `Args:` and `Returns:`. Type hints should be used for public APIs.

---

## 4. Git Version Control Constraints

1. **Atomic Commits:** Changes must be systematically split into small, atomic, and self-contained commits.
2. **Semantic Messages:** Use strict semantic commit messages (e.g., `feat:`, `bench:`, `fix:`, `docs:`, `style:`, `refactor:`).
3. **No Overrides:** NEVER use `git commit --amend` to rewrite or append onto existing pushed or local commits.
4. **Mandatory Commits:** All code, scripts, and documentation changes MUST be committed to Git.
5. **Clean Working Tree:** Ensure untracked video master files and cache outputs are properly ignored before committing.

---

## 5. Google Drive Storage Architecture

All media deliverables are organized under the shared **"Projects/Hot Path C++"** Google Drive directory:

```
Google Drive/
└── Projects/
    └── Hot Path C++/
        ├── branding/
        │   ├── channel_icon.png
        │   ├── channel_banner_2560x1440.jpg
        │   ├── host_mascot_marcus.jpg
        │   └── video_watermark_150x150.png
        └── episodes/
            ├── 01_pessimization_of_move/
            │   ├── episode_01_master.mp4       <-- 1080p Master Video
            │   ├── narration_master.mp3        <-- Master Audio Track
            │   └── thumbnail.png               <-- YouTube Video Thumbnail (1280x720)
            ├── 02_vector_vs_list_cache_lines/
            │   └── ...
            └── ...
```

### Video Upload Procedures

#### Method 1: Using the Automated Python CLI (`tools/gdrive_upload.py`)
```bash
# Upload an episode master video
python3 tools/gdrive_upload.py \
  --file episodes/01_pessimization_of_move/episode_01_master.mp4 \
  --folder "Projects/Hot Path C++/episodes/01_pessimization_of_move"
```

#### Method 2: Using `rclone` (Recommended for Cloud Workstations)
```bash
# Upload episode folder
rclone copy \
  episodes/01_pessimization_of_move/episode_01_master.mp4 \
  "gdrive:Hot Path C++/episodes/01_pessimization_of_move/" \
  --progress
```

#### Method 3: Web UI (Manual Upload)
1. Open [Google Drive](https://drive.google.com).
2. Navigate to `Projects/Hot Path C++/episodes/<episode_folder>/`.
3. Drag and drop the local render: `episodes/<episode_folder>/episode_XX_master.mp4`.
4. Right-click the uploaded file, select **Share -> Copy link**, and record in the episode's `README.md`.

---

## 6. Autonomous Agent Step-by-Step Production Checklist

When an agent is tasked with creating or updating an episode:

0. **Step 0: Editorial & Reality Gate (Mandatory Pre-Flight)**
   - Verify the episode satisfies all four directives in **Section 9** ("Why Care?" Contract, Explicit Stance & Standards/Industry Alignment, Reality Mandate with zero toy/contrived benchmarks, and verified Primary Sources for Slide Citation Bars) before writing benchmarks or scripts.
1. **Step 1: Benchmark & Verification with Bazel & Google Benchmark**
   - Implement `benchmark.cc` using the **Google Benchmark** library.
   - Define a `cc_binary` in `BUILD.bazel` depending on `@google_benchmark//:benchmark_main`.
   - Run under GCC and Clang via `bazel run -c opt //...`.
   - Capture clean Intel-syntax assembly to `assembly/godbolt_comparison.s`.
   - Run Linux `perf stat` hardware PMU profiling against the optimized Bazel binary.
2. **Step 2: Scriptwriting & Host Dynamic**
   - Write `script.md` using the Marcus (Senior Architect) & Byte (Hardware Auditor) host dynamic.
   - Ensure the tone balances approachable analogies with rigorous hardware PMU details and CS dry humor.
3. **Step 3: Speech Synthesis (Adult Voice Standards)**
   - Run `tools/synthesize_audio.py` (via `edge-tts`).
   - Marcus Voice: Adult male (`en-US-ChristopherNeural`, deep mentor delivery).
   - Byte Voice: Adult female (`en-US-AriaNeural`, sharp, confident hardware auditor). Minor/child voices are strictly prohibited.
   - Verify that durations and `timestamps.json` are properly recorded.
4. **Step 4: Authentic Slide Asset Capture**
   - Run `tools/real_screenshot.py` or episode-specific `generate_slide_assets.py`.
   - MUST capture 100% genuine screenshots (real live Godbolt website via Selenium/Chrome, real decorated X11 terminal windows via Xvfb/xfwm4/xterm).
   - Simulated/PIL-drawn mockups are strictly prohibited.
5. **Step 5: Full Episode Video Rendering**
   - Run `tools/render_video.py` to composite 1080p slide frames, host viewport, closed captions, and watermark badge into dynamic turn cuts.
   - Verify the output video properties with `ffprobe`.
6. **Step 6: YouTube Shorts Production (Vertical 9:16)**
   - Create a fast-paced vertical video (`1080x1920`, < 60 seconds) in `episodes/<episode_folder>/shorts/`.
   - Use `synthesize_shorts_audio.py` and `render_shorts.py`.
   - Feature high-retention dialogue, real screenshot cards, high-contrast captions, and a sticky call-to-action linking to the full episode.
7. **Step 7: Cloud Storage & Git Hygiene**
   - Verify that `git status` does NOT track any `.mp4` or intermediate video files.
   - Upload the master `.mp4` and Shorts `.mp4` to Google Drive under `Projects/Hot Path C++/episodes/<episode_folder>/`.
   - Update `episodes/<episode_folder>/README.md` with benchmark tables, YouTube description, pinned comments, and Drive links.
   - Format all code with `clang-format -style=Google`.
   - Commit code, scripts, and documentation to Git using semantic commit messages.

---

## 7. Authentic Visual Asset & Screenshot Standards

### The Authenticity Mandate (Strictly Enforced)
Viewers of high-performance systems and low-latency engineering content demand empirical proof.
- **NEVER use PIL (`ImageDraw`), canvas, or SVG scripts to fabricate or draw simulated terminal windows, fake IDE frames, or mock code cards.**
- All visual evidence presented in episodes MUST be captured from live, running software environments.

### Standard 1: Compiler Explorer (Godbolt) Web Captures
When referencing Compiler Explorer (Godbolt):
1. **Never label a local terminal or objdump command as "Godbolt".**
2. **Capture the live web application:** Use Selenium and headless Google Chrome against `https://godbolt.org` via `tools/real_screenshot.py godbolt`.
3. **Automated URL generation:** Use Compiler Explorer's official API (`https://godbolt.org/api/shortener`) with a clientstate JSON to generate a permanent shortlink with the exact source code, compiler, and `-O3 -std=c++20` optimization flags.
4. **Clean Presentation:** Execute JavaScript to dismiss cookie banners, privacy modals (`#alert`), and community announcements (`.notification-banner`, `.motd`) before capturing the 1080p screenshot.

### Standard 2: Decorated Linux Terminal Captures
When displaying shell commands, compiler diagnostics, benchmark runs, or hardware counters:
1. **Virtual Display Environment:** Run commands inside a virtual X11 server (`Xvfb :99 -screen 0 1920x1080x24`) with an active window manager (`xfwm4`) to provide authentic window title bars, minimize/maximize/close buttons, and drop shadows.
2. **Terminal Emulator:** Launch `xterm` with genuine system FreeType antialiased fonts (`DejaVu Sans Mono` or `Ubuntu Mono`), authentic bash prompts, and ANSI color syntax.
3. **Automated Capture & Auto-Crop:** Use ImageMagick `import -window root` on the black root display and auto-crop the window bounding box using Pillow, preserving clean window decorations without manual pixel guesswork.
4. **CLI Utility:** Use `python3 tools/real_screenshot.py terminal --cmd "<bash_commands>" --out <path.png>`.

---

## 8. YouTube Shorts & Social Clip Specifications

To maximize outreach and discoverability, every episode MUST include a companion YouTube Short:
- **Aspect Ratio:** `9:16` vertical video (`1080x1920` @ 25fps).
- **Duration:** Strictly under 60 seconds (ideal target: 35 to 50 seconds).
- **Audio:** Fast-paced banter between Marcus and adult Byte using Edge-TTS with natural cadence.
- **Visual Composition:**
  - Top: Channel branding badge, hook title, and dual host avatar heads with active speaker indicators.
  - Middle: Real high-resolution screenshot card (Godbolt or Terminal PMU counters).
  - Bottom: High-contrast, bold subtitle pills and a persistent glowing Call-to-Action banner ("▶ WATCH FULL 10-MIN DEEP DIVE IN PINNED COMMENT").

---

## 9. Editorial Mandate, Reality Benchmarks & Slide Citation Bar Directives

All episodes (starting with **Phase III: Episodes 15–19** and all future productions) MUST pass the following four editorial directives before benchmarks, scripts, or slides are finalized:

### Directive 1: The "Why Should a Viewer Care?" Contract
Every episode must explicitly establish—within the first **60 seconds** and reinforced in the closing engineering playbook:
1. **The Real Production Pain:** Name the concrete failure mode (p99/p99.99 latency spike, OOM/RSS blowup, 16-core scaling collapse, silent Use-After-Free, HashDoS outage, or binary/cache bloat) that hits working software engineers.
2. **Return on Viewer Time:** Explain clearly what a viewer gains by spending 15–20 minutes on the episode—connecting hardware behavior directly to application-level correctness and performance.
3. **Monday-Morning Takeaway:** Deliver an actionable **Escalation Ladder** or **Code-Review Checklist** (including compiler/CI guards like `static_assert` or `null_memory_resource`) that teams can apply immediately without rewriting their entire system.

### Directive 2: Explicit Best-Practice Stance & Industry / Standards Alignment
We reject shallow tribalism and absolute dogma (never *"ban `std::unordered_map` everywhere"* or *"language X is slow"*). Every episode MUST state:
1. **Our Grounded Engineering Stance:** A numbered Escalation Ladder (e.g., Step 0 default $\rightarrow$ Step 1 zero-dependency fix $\rightarrow$ Step 2 specialized library $\rightarrow$ Step 3 architectural split) with explicit boundaries for when **not** to optimize.
2. **Alignment With Standards Committees & Industry:** Explain *why* the **ISO C++ Standards Committee (WG21)**, **C++ Core Guidelines**, **Google (Abseil)**, **Meta (Folly)**, **Boost**, **LLVM**, **Chromium**, **Rust**, **Go**, **OpenJDK**, and **CPython** made their actual design trade-offs (ABI stability, DoS defense, pointer/iterator validity, binary `.text` footprint, or GC ergonomics).

### Directive 3: The Reality Mandate (Zero Contrived Benchmarks or Cute Snippets)
- **Strictly Prohibited:**
  - Toy loops such as `std::unordered_map<int, int>` inserting sequential `0..N` keys on a pristine startup heap.
  - Meaningless `Foo` / `Bar` structs, placeholder `doWork()` functions, or cherry-picked setups rigged to flatter one language or library.
- **Strictly Required:**
  - **Authentic Domain Workloads:** Every benchmark MUST model a realistic production hot path (e.g., edge rate limiters, RPC header routing, limit order books, session caches) using realistic struct sizes, **aged/fragmented heaps**, realistic hit/miss ratios, **Zipfian skewed key distributions** where real traffic is skewed, and streaming insert/erase churn.
  - **Real Codebases & War Stories:** Snippets and narratives must be anchored in verifiable open-source repositories, standards papers, or documented production telemetry—never invented anecdotes.

### Directive 4: Mandatory Slide Citation Bar Standard
Whenever a slide or Short displays a real-world war story, fleet metric, open-source implementation detail, or standards committee paper, it MUST include a standardized **Citation Bar** inside the bottom visual safe-zone:
- **Standard Format:**
  `SOURCE • <Author / Organization> (<Year>) — <Paper / Engineering Post / Exact Repo Path>`
- **Representative Examples:**
  - `SOURCE • Matt Kulukundis (CppCon 2017) — Google Fleet Telemetry (~1% CPU, >4% RAM in C++ Hash Tables)`
  - `SOURCE • WG21 P1863R1 (Titus Winters, 2020) — "ABI — Now or Never"`
  - `SOURCE • Chromium Docs — base/containers/containers.md (MSVC 16B _DEQUESIZ std::deque Ban)`
- **Verification Rule:** Every paper ID, author attribution, repository path, and fleet metric shown in a Citation Bar MUST be verified against primary sources before scriptwriting begins.

