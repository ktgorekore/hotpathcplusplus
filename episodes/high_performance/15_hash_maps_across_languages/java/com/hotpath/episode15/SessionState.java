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

package com.hotpath.episode15;

/**
 * Heap-allocated 24-byte-equivalent Java session record (12B object header + 24B fields = 40B
 * aligned on 64-bit JVM with compressed oops, plus 4B/8B reference inside map nodes).
 */
public final class SessionState {

  public final long quotaRemaining;
  public final long lastSeenNs;
  public final int tierFlags;
  public final int rateWindowHits;

  /**
   * Constructs an immutable session state snapshot.
   *
   * @param quotaRemaining remaining API tokens in current rate window
   * @param lastSeenNs last request timestamp in nanoseconds
   * @param tierFlags account tier bitflags
   * @param rateWindowHits hit counter in active window
   */
  public SessionState(
      long quotaRemaining, long lastSeenNs, int tierFlags, int rateWindowHits) {
    this.quotaRemaining = quotaRemaining;
    this.lastSeenNs = lastSeenNs;
    this.tierFlags = tierFlags;
    this.rateWindowHits = rateWindowHits;
  }

  /**
   * Computes deterministic 64-bit fold digest matching C++, Rust, Go, and Python.
   *
   * @return 64-bit digest value
   */
  public long digest() {
    return quotaRemaining
        ^ lastSeenNs
        ^ Integer.toUnsignedLong(tierFlags)
        ^ Integer.toUnsignedLong(rateWindowHits);
  }
}
