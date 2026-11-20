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

package main

import (
	"flag"
	"fmt"
	"os"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"time"
)

const (
	seedKeys    uint64 = 0xC0FFEE15
	seedQueries uint64 = 0xFACE0015
	seedMisses  uint64 = 0xBAD00015
	seedChurn   uint64 = 0x99990015

	numL1Entries    = 1024
	numAgedEntries  = 2_000_000
	numQueries      = 2_000_000
	numHashDosKeys  = 40_000
	defaultDosPrime = uint64(43_067)
)

// SessionState matches the 24-byte C++/Rust struct layout by value.
type SessionState struct {
	QuotaRemaining uint64
	LastSeenNs     uint64
	TierFlags      uint32
	RateWindowHits uint32
}

type gatewayDataset struct {
	keys           []uint64
	packedVals     []uint64
	sessionVals    []SessionState
	queries8020    []uint64
	queries100Miss []uint64
}

// GlobalSink prevents compiler dead-code elimination of timed lookup results.
var GlobalSink uint64

func splitMix64(state *uint64) uint64 {
	*state += 0x9e3779b97f4a7c15
	z := *state
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb
	return z ^ (z >> 31)
}

func makePresentKey(state *uint64) uint64 {
	return (splitMix64(state) & 0x3FFFFFFFFFFFFFFF) | 1
}

func makeAbsentKey(state *uint64) uint64 {
	return (splitMix64(state) & 0x3FFFFFFFFFFFFFFF) | (uint64(1) << 62) | 1
}

func packTokenBucket(raw uint64) uint64 {
	lastSeenMs := (raw >> 24) & 0x7FFFFFFFFF
	quota := (raw & 0xFFFF) + 1
	return (lastSeenMs << 24) | quota
}

func makeSessionState(key, packed uint64) SessionState {
	quota := packed & 0xFFFFFF
	lastSeenMs := (packed >> 24) & 0x7FFFFFFFFF
	return SessionState{
		QuotaRemaining: quota,
		LastSeenNs:     lastSeenMs * 1_000_000,
		TierFlags:      uint32(key & 0xFFFF),
		RateWindowHits: uint32((key >> 16) & 0xFF),
	}
}

func digestSessionState(s SessionState) uint64 {
	return s.QuotaRemaining ^ s.LastSeenNs ^ uint64(s.TierFlags) ^ uint64(s.RateWindowHits)
}

func finalizeChecksum(sum, hits uint64) uint64 {
	return sum ^ (hits * 0x9e3779b97f4a7c15)
}

func buildDataset(numEntries, nQueries int) gatewayDataset {
	ds := gatewayDataset{
		keys:           make([]uint64, numEntries),
		packedVals:     make([]uint64, numEntries),
		sessionVals:    make([]SessionState, numEntries),
		queries8020:    make([]uint64, nQueries),
		queries100Miss: make([]uint64, nQueries),
	}

	keyRng := seedKeys
	for i := 0; i < numEntries; i++ {
		k := makePresentKey(&keyRng)
		raw := splitMix64(&keyRng)
		packed := packTokenBucket(raw)
		ds.keys[i] = k
		ds.packedVals[i] = packed
		ds.sessionVals[i] = makeSessionState(k, packed)
	}

	qRng := seedQueries
	for i := 0; i < nQueries; i++ {
		r := splitMix64(&qRng)
		if r%5 != 0 {
			idx := int((r >> 8) % uint64(numEntries))
			ds.queries8020[i] = ds.keys[idx]
		} else {
			ds.queries8020[i] = (r & 0x3FFFFFFFFFFFFFFF) | (uint64(1) << 62) | 1
		}
	}

	mRng := seedMisses
	for i := 0; i < nQueries; i++ {
		ds.queries100Miss[i] = makeAbsentKey(&mRng)
	}
	return ds
}

func applyHeapChurn(chunkCount int) [][]byte {
	rng := seedChurn
	chunks := make([][]byte, chunkCount)
	for i := 0; i < chunkCount; i++ {
		sz := 32 + int((splitMix64(&rng)&7)*32)
		buf := make([]byte, sz)
		buf[0] = byte(i)
		chunks[i] = buf
	}
	for i := 0; i < len(chunks); i += 2 {
		chunks[i] = nil
	}
	return chunks
}

func readVmRssBytes() uint64 {
	data, err := os.ReadFile("/proc/self/status")
	if err != nil {
		return 0
	}
	for _, line := range strings.Split(string(data), "\n") {
		if strings.HasPrefix(line, "VmRSS:") {
			fields := strings.Fields(line)
			if len(fields) >= 2 {
				kb, err := strconv.ParseUint(fields[1], 10, 64)
				if err == nil {
					return kb * 1024
				}
			}
		}
	}
	return 0
}

//go:noinline
func findPacked(m map[uint64]uint64, queries []uint64) uint64 {
	var sum uint64
	var hits uint64
	for _, q := range queries {
		if v, ok := m[q]; ok {
			sum += v
			hits++
		}
	}
	return finalizeChecksum(sum, hits)
}

//go:noinline
func findSession(m map[uint64]SessionState, queries []uint64) uint64 {
	var sum uint64
	var hits uint64
	for _, q := range queries {
		if v, ok := m[q]; ok {
			sum += digestSessionState(v)
			hits++
		}
	}
	return finalizeChecksum(sum, hits)
}

//go:noinline
func iterPacked(m map[uint64]uint64) uint64 {
	var sum uint64
	var count uint64
	for k, v := range m {
		sum += k ^ v
		count++
	}
	return finalizeChecksum(sum, count)
}

func printSweepRow(label string, totalMs float64, sweeps, live int, checksum uint64) {
	msPerSweep := totalMs / float64(sweeps)
	totalLive := sweeps * live
	nsPerLive := (totalMs * 1_000_000.0) / float64(totalLive)
	mops := float64(totalLive) / (totalMs * 1_000.0)
	fmt.Printf("%-42s | %6.2f ms/swp | %6.2f ns/live | %7.2f M/s | checksum=0x%x\n",
		label, msPerSweep, nsPerLive, mops, checksum)
}

func printLookupRow(label string, totalMs float64, totalOps int, checksum uint64) {
	nsPerOp := (totalMs * 1_000_000.0) / float64(totalOps)
	mops := float64(totalOps) / (totalMs * 1_000.0)
	fmt.Printf("%-42s | %8.2f ns/op | %7.2f M ops/s | checksum=0x%x\n",
		label, nsPerOp, mops, checksum)
}

func runL1Packed(iters int, ver string) {
	ds := buildDataset(numL1Entries, numQueries)
	churn := applyHeapChurn(8192)
	m := make(map[uint64]uint64, len(ds.keys))
	for i, k := range ds.keys {
		m[k] = ds.packedVals[i]
	}
	runtime.KeepAlive(churn)
	churn = nil

	sink := findPacked(m, ds.queries8020)
	t0 := time.Now()
	for it := 0; it < iters; it++ {
		sink += findPacked(m, ds.queries8020)
	}
	ms := float64(time.Since(t0).Nanoseconds()) / 1e6
	GlobalSink += sink
	printLookupRow(fmt.Sprintf("Go %s map[uint64]uint64 (L1 1K 80/20)", ver), ms, iters*len(ds.queries8020), sink)
}

func runAgedPacked(iters int, missOnly bool, ver string) {
	ds := buildDataset(numAgedEntries, numQueries)
	churn := applyHeapChurn(300_000)
	m := make(map[uint64]uint64, len(ds.keys))
	for i, k := range ds.keys {
		m[k] = ds.packedVals[i]
	}
	runtime.KeepAlive(churn)
	churn = nil
	runtime.GC()

	queries := ds.queries8020
	label := fmt.Sprintf("Go %s map[uint64]uint64 (Aged 2M 80/20)", ver)
	if missOnly {
		queries = ds.queries100Miss
		label = fmt.Sprintf("Go %s map[uint64]uint64 (Aged 2M Miss)", ver)
	}

	sink := findPacked(m, queries)
	t0 := time.Now()
	for it := 0; it < iters; it++ {
		sink += findPacked(m, queries)
	}
	ms := float64(time.Since(t0).Nanoseconds()) / 1e6
	GlobalSink += sink
	printLookupRow(label, ms, iters*len(queries), sink)
}

func runAgedSession(iters int, ver string) {
	ds := buildDataset(numAgedEntries, numQueries)
	churn := applyHeapChurn(300_000)
	m := make(map[uint64]SessionState, len(ds.keys))
	for i, k := range ds.keys {
		m[k] = ds.sessionVals[i]
	}
	runtime.KeepAlive(churn)
	churn = nil
	runtime.GC()

	sink := findSession(m, ds.queries8020)
	t0 := time.Now()
	for it := 0; it < iters; it++ {
		sink += findSession(m, ds.queries8020)
	}
	ms := float64(time.Since(t0).Nanoseconds()) / 1e6
	GlobalSink += sink
	printLookupRow(fmt.Sprintf("Go %s map[uint64]Session (Aged 24B)", ver), ms, iters*len(ds.queries8020), sink)
}

func measureLiveHeap(label string, buildFn func() interface{}, entries int) {
	runtime.GC()
	runtime.GC()
	var mBefore, mAfter runtime.MemStats
	runtime.ReadMemStats(&mBefore)
	rssBefore := readVmRssBytes()

	holder := buildFn()
	runtime.GC()
	runtime.GC()
	runtime.ReadMemStats(&mAfter)
	rssAfter := readVmRssBytes()
	runtime.KeepAlive(holder)

	var heapDelta uint64
	if mAfter.HeapAlloc > mBefore.HeapAlloc {
		heapDelta = mAfter.HeapAlloc - mBefore.HeapAlloc
	}
	var rssDelta uint64
	if rssAfter > rssBefore {
		rssDelta = rssAfter - rssBefore
	}
	fmt.Printf("%-42s | HeapAlloc=%6.1f MiB (%5.1f B/entry) | RSS=%6.1f MiB (%5.1f B/entry)\n",
		label,
		float64(heapDelta)/(1024*1024), float64(heapDelta)/float64(entries),
		float64(rssDelta)/(1024*1024), float64(rssDelta)/float64(entries))
}

func runBattle2Rss(ver string) {
	ds := buildDataset(numAgedEntries, 1024)
	fmt.Printf("\n=== Go %s Battle 2A: Resident & Live Heap Footprint (2,000,000 Entries) ===\n", ver)
	measureLiveHeap(fmt.Sprintf("Go %s map[uint64]uint64 (16B Packed)", ver), func() interface{} {
		m := make(map[uint64]uint64, len(ds.keys))
		for i, k := range ds.keys {
			m[k] = ds.packedVals[i]
		}
		return m
	}, len(ds.keys))

	measureLiveHeap(fmt.Sprintf("Go %s map[uint64]Session (32B)", ver), func() interface{} {
		m := make(map[uint64]SessionState, len(ds.keys))
		for i, k := range ds.keys {
			m[k] = ds.sessionVals[i]
		}
		return m
	}, len(ds.keys))
}

func runBattle2RssPacked(ver string) {
	ds := buildDataset(numAgedEntries, 1024)
	measureLiveHeap(fmt.Sprintf("Go %s map[uint64]uint64 (16B Packed)", ver), func() interface{} {
		m := make(map[uint64]uint64, len(ds.keys))
		for i, k := range ds.keys {
			m[k] = ds.packedVals[i]
		}
		return m
	}, len(ds.keys))
}

func runBattle2RssSession(ver string) {
	ds := buildDataset(numAgedEntries, 1024)
	measureLiveHeap(fmt.Sprintf("Go %s map[uint64]Session (32B)", ver), func() interface{} {
		m := make(map[uint64]SessionState, len(ds.keys))
		for i, k := range ds.keys {
			m[k] = ds.sessionVals[i]
		}
		return m
	}, len(ds.keys))
}

func runTailInsert(label string, ds gatewayDataset, reserve bool) {
	samples := make([]int64, len(ds.keys))
	var m map[uint64]uint64
	if reserve {
		m = make(map[uint64]uint64, len(ds.keys))
	} else {
		m = make(map[uint64]uint64)
	}
	startAll := time.Now()
	for i, k := range ds.keys {
		t0 := time.Now()
		m[k] = ds.packedVals[i]
		samples[i] = time.Since(t0).Nanoseconds()
	}
	totalMs := float64(time.Since(startAll).Nanoseconds()) / 1e6
	GlobalSink ^= uint64(len(m))
	sort.Slice(samples, func(i, j int) bool { return samples[i] < samples[j] })
	pct := func(p float64) int64 {
		idx := int(p * float64(len(samples)-1))
		if idx >= len(samples) {
			idx = len(samples) - 1
		}
		return samples[idx]
	}
	fmt.Printf("%-42s | total=%7.1f ms | p50=%4d ns | p99=%5d ns | p99.9=%6d ns | p99.99=%7d ns | max=%10d ns\n",
		label, totalMs, pct(0.50), pct(0.99), pct(0.999), pct(0.9999), samples[len(samples)-1])
}

func runBattle2Tail(ver string) {
	ds := buildDataset(numAgedEntries, 1024)
	fmt.Printf("\n=== Go %s Battle 2B: Streaming Insert Tail Latency (0 -> 2,000,000) ===\n", ver)
	runTailInsert(fmt.Sprintf("Go %s map[uint64]uint64 (Unreserved)", ver), ds, false)
	runTailInsert(fmt.Sprintf("Go %s map[uint64]uint64 (Reserved)", ver), ds, true)
}

func runBattle3HashDos(prime uint64, ver string) {
	normalDs := buildDataset(numHashDosKeys, 1024)
	dosKeys := make([]uint64, numHashDosKeys)
	for i := 0; i < numHashDosKeys; i++ {
		dosKeys[i] = uint64(i+1) * prime
	}
	fmt.Printf("\n=== Go %s Battle 3: Adversarial HashDoS (%d Keys, prime=%d) ===\n", ver, numHashDosKeys, prime)

	mNorm := make(map[uint64]uint64, numHashDosKeys)
	t0 := time.Now()
	for i, k := range normalDs.keys {
		mNorm[k] = uint64(i + 1)
	}
	normMs := float64(time.Since(t0).Nanoseconds()) / 1e6
	GlobalSink ^= uint64(len(mNorm))
	fmt.Printf("%-42s | %8.2f ms\n", fmt.Sprintf("Go %s map (Normal Keys)", ver), normMs)

	mDos := make(map[uint64]uint64, numHashDosKeys)
	t0 = time.Now()
	for i, k := range dosKeys {
		mDos[k] = uint64(i + 1)
	}
	dosMs := float64(time.Since(t0).Nanoseconds()) / 1e6
	GlobalSink ^= uint64(len(mDos))
	fmt.Printf("%-42s | %8.2f ms\n", fmt.Sprintf("Go %s map (Crafted Prime Keys)", ver), dosMs)
}

func runIterPacked(iters int, sparse bool, ver string) {
	ds := buildDataset(numAgedEntries, 1024)
	churn := applyHeapChurn(300_000)
	m := make(map[uint64]uint64, len(ds.keys))
	for i, k := range ds.keys {
		m[k] = ds.packedVals[i]
	}
	runtime.KeepAlive(churn)
	churn = nil
	if sparse {
		for i, k := range ds.keys {
			if i%10 != 0 {
				delete(m, k)
			}
		}
	}
	runtime.GC()

	live := len(m)
	label := fmt.Sprintf("Go %s map[uint64]uint64 (Iter 2M Full)", ver)
	if sparse {
		label = fmt.Sprintf("Go %s map[uint64]uint64 (Sparse 200K)", ver)
	}

	sink := iterPacked(m)
	t0 := time.Now()
	for it := 0; it < iters; it++ {
		sink += iterPacked(m)
	}
	ms := float64(time.Since(t0).Nanoseconds()) / 1e6
	GlobalSink += sink
	printSweepRow(label, ms, iters, live, sink)
}

func runSelfTests() {
	ds := buildDataset(16_384, 32_768)
	m := make(map[uint64]uint64, len(ds.keys))
	for i, k := range ds.keys {
		m[k] = ds.packedVals[i]
	}
	c := findPacked(m, ds.queries8020)
	if c == 0 {
		panic("expected non-zero 80/20 checksum")
	}
	cMiss := findPacked(m, ds.queries100Miss)
	if cMiss != 0 {
		panic("expected zero miss checksum")
	}
	cFull := iterPacked(m)
	if cFull == 0 {
		panic("expected non-zero full sweep checksum")
	}
	for i, k := range ds.keys {
		if i%10 != 0 {
			delete(m, k)
		}
	}
	if len(m) != 1639 {
		panic(fmt.Sprintf("expected 1639 sparse entries, got %d", len(m)))
	}
	cSparse := iterPacked(m)
	if cSparse == 0 {
		panic("expected non-zero sparse sweep checksum")
	}
	fmt.Printf("Go %s self-test PASSED: 16K lookup=0x%x, iter_full=0x%x, iter_sparse=0x%x\n",
		runtime.Version(), c, cFull, cSparse)
}

func main() {
	mode := flag.String("mode", "all", "Benchmark mode")
	itersFlag := flag.Int("iters", 0, "Lookup pass count")
	primeFlag := flag.Uint64("prime", defaultDosPrime, "libstdc++ prime bucket count")
	flag.Parse()

	ver := strings.TrimPrefix(runtime.Version(), "go")
	iters := *itersFlag

	switch *mode {
	case "test":
		runSelfTests()
	case "all":
		fmt.Printf("=== Go %s Battle 1: L1 (1,024) vs. Aged Post-Churn (2,000,000) ===\n", ver)
		if iters == 0 {
			runL1Packed(100, ver)
			runAgedPacked(10, false, ver)
			runAgedPacked(10, true, ver)
			runAgedSession(10, ver)
			runIterPacked(20, false, ver)
			runIterPacked(60, true, ver)
		} else {
			runL1Packed(iters, ver)
			runAgedPacked(iters, false, ver)
			runAgedPacked(iters, true, ver)
			runAgedSession(iters, ver)
			runIterPacked(iters, false, ver)
			runIterPacked(iters, true, ver)
		}
		runBattle2Rss(ver)
		runBattle2Tail(ver)
		runBattle3HashDos(*primeFlag, ver)
	case "b1_l1_packed":
		if iters == 0 {
			iters = 500
		}
		runL1Packed(iters, ver)
	case "b1_aged_packed":
		if iters == 0 {
			iters = 40
		}
		runAgedPacked(iters, false, ver)
	case "b1_miss_packed":
		if iters == 0 {
			iters = 40
		}
		runAgedPacked(iters, true, ver)
	case "b1_aged_session":
		if iters == 0 {
			iters = 40
		}
		runAgedSession(iters, ver)
	case "b1_iter_full_packed":
		if iters == 0 {
			iters = 40
		}
		runIterPacked(iters, false, ver)
	case "b1_iter_sparse_packed":
		if iters == 0 {
			iters = 200
		}
		runIterPacked(iters, true, ver)
	case "b2_rss":
		runBattle2Rss(ver)
	case "b2_rss_packed":
		runBattle2RssPacked(ver)
	case "b2_rss_session":
		runBattle2RssSession(ver)
	case "b2_tail":
		runBattle2Tail(ver)
	case "b3_hashdos":
		runBattle3HashDos(*primeFlag, ver)
	default:
		fmt.Fprintf(os.Stderr, "Unknown --mode=%s\n", *mode)
		os.Exit(1)
	}
}
