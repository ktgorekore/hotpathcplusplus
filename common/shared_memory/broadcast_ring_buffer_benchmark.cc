// Copyright 2026 Cognitas Trading Inc.
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

#include <unistd.h>

#include <cstring>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "benchmark/benchmark.h"
#include "common/shared_memory/broadcast_ring_buffer.h"
#include "common/shared_memory/memory_map.h"

namespace cognitas::trading {

class BroadcastRingBufferBenchmark : public benchmark::Fixture {
 public:
  void SetUp(const ::benchmark::State&) override {
    shm_name_ = "/bench_shm_" + std::to_string(getpid());
    BroadcastRingBuffer::Unlink(shm_name_).IgnoreError();
    // 16MB buffer (Page aligned)
    long page_size = sysconf(_SC_PAGESIZE);
    size_t capacity = (1024 * 1024 * 16 / page_size) * page_size;

    auto producer_res = BroadcastRingBuffer::Create(shm_name_, capacity);
    if (!producer_res.ok()) {
      LOG(FATAL) << "Failed to create producer: " << producer_res.status();
    }
    producer_ = std::move(*producer_res);

    auto consumer_res = BroadcastRingBuffer::Attach(shm_name_);
    if (!consumer_res.ok()) {
      LOG(FATAL) << "Failed to create consumer: " << consumer_res.status();
    }
    consumer_ = std::move(*consumer_res);
  }

  void TearDown(const ::benchmark::State&) override {
    producer_.reset();
    consumer_.reset();
    BroadcastRingBuffer::Unlink(shm_name_).IgnoreError();
  }

  std::string shm_name_;
  std::unique_ptr<BroadcastRingBuffer> producer_;
  std::unique_ptr<BroadcastRingBuffer> consumer_;
};

BENCHMARK_DEFINE_F(BroadcastRingBufferBenchmark, Produce)
(benchmark::State& state) {
  std::string payload(state.range(0), 'X');

  // Need a reader registered so producer can do backpressure correctly,
  // otherwise it doesn't wait, but without a reader, it just loops the cursor
  // infinitely. Wait, if no readers are registered, min_cursor will be
  // target_cursor, which is wrong. Let's check my logic in Reserve: if
  // target_cursor - cached_min_cursor > capacity min_c = target_cursor for
  // active readers: min_c = min(min_c, rc) If NO active readers, min_c =
  // target_cursor So it will always break the spin loop immediately. This is
  // fine for isolated produce bench.

  for (auto _ : state) {
    auto buf_or = producer_->Reserve(payload.size(), 1);
    if (!buf_or.ok()) {
      state.SkipWithError(buf_or.status().ToString().c_str());
      break;
    }
    std::memcpy(*buf_or, payload.data(), payload.size());
    producer_->Commit();
  }
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(state.range(0)));
}
BENCHMARK_REGISTER_F(BroadcastRingBufferBenchmark, Produce)->Range(8, 8 << 10);

BENCHMARK_DEFINE_F(BroadcastRingBufferBenchmark, RoundTripZeroCopy)
(benchmark::State& state) {
  std::string payload(state.range(0), 'X');
  BroadcastReader reader;
  if (!consumer_->RegisterReader(reader).ok()) {
    state.SkipWithError("Failed to register reader");
    return;
  }

  for (auto _ : state) {
    // Produce
    auto buf_or = producer_->Reserve(payload.size(), 1);
    if (!buf_or.ok()) {
      state.SkipWithError(buf_or.status().ToString().c_str());
      break;
    }
    std::memcpy(*buf_or, payload.data(), payload.size());
    producer_->Commit();

    // Consume (Zero Copy)
    FrameHeader header;
    auto res_or = consumer_->PollZeroCopy(reader, header);
    if (!res_or.ok()) {
      state.SkipWithError(res_or.status().ToString().c_str());
      break;
    }
    benchmark::DoNotOptimize(*res_or);
    consumer_->Advance(reader, header.TotalFrameSize());
  }

  consumer_->UnregisterReader(reader);
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(state.range(0)));
}
BENCHMARK_REGISTER_F(BroadcastRingBufferBenchmark, RoundTripZeroCopy)
    ->Range(8, 8 << 10);

BENCHMARK_DEFINE_F(BroadcastRingBufferBenchmark, PollZeroCopyBatch)
(benchmark::State& state) {
  std::string payload(state.range(0), 'X');
  BroadcastReader reader;
  if (!consumer_->RegisterReader(reader).ok()) {
    state.SkipWithError("Failed to register reader");
    return;
  }

  const int kBatchSize = 100;

  for (auto _ : state) {
    state.PauseTiming();
    for (int i = 0; i < kBatchSize; ++i) {
      auto buf_or = producer_->Reserve(payload.size(), 1);
      if (buf_or.ok()) {
        std::memcpy(*buf_or, payload.data(), payload.size());
        producer_->Commit();
      }
    }
    state.ResumeTiming();

    for (int i = 0; i < kBatchSize; ++i) {
      FrameHeader header;
      auto status = consumer_->PollZeroCopy(reader, header);
      if (!status.ok()) {
        state.SkipWithError(status.status().ToString().c_str());
        break;
      }
      benchmark::DoNotOptimize(*status);
      consumer_->Advance(reader, header.TotalFrameSize());
    }
  }

  consumer_->UnregisterReader(reader);
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(state.range(0)) * kBatchSize);
}
BENCHMARK_REGISTER_F(BroadcastRingBufferBenchmark, PollZeroCopyBatch)
    ->Range(8, 8 << 10);

BENCHMARK_DEFINE_F(BroadcastRingBufferBenchmark, RoundTripCopy)
(benchmark::State& state) {
  std::string payload(state.range(0), 'X');
  BroadcastReader reader;
  if (!consumer_->RegisterReader(reader).ok()) {
    state.SkipWithError("Failed to register reader");
    return;
  }

  SafeMessage msg_out;

  for (auto _ : state) {
    // Produce
    auto buf_or = producer_->Reserve(payload.size(), 1);
    if (!buf_or.ok()) {
      state.SkipWithError(buf_or.status().ToString().c_str());
      break;
    }
    std::memcpy(*buf_or, payload.data(), payload.size());
    producer_->Commit();

    // Consume (Copy)
    FrameHeader header;
    auto status = consumer_->PollCopy(reader, header, msg_out);
    if (!status.ok()) {
      state.SkipWithError(status.ToString().c_str());
      break;
    }
    benchmark::DoNotOptimize(msg_out.payload);
    consumer_->Advance(reader, header.TotalFrameSize());
  }

  consumer_->UnregisterReader(reader);
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(state.range(0)));
}
BENCHMARK_REGISTER_F(BroadcastRingBufferBenchmark, RoundTripCopy)
    ->Range(8, 8 << 10);

BENCHMARK_DEFINE_F(BroadcastRingBufferBenchmark, PollCopyBatch)
(benchmark::State& state) {
  std::string payload(state.range(0), 'X');
  BroadcastReader reader;
  if (!consumer_->RegisterReader(reader).ok()) {
    state.SkipWithError("Failed to register reader");
    return;
  }

  const int kBatchSize = 100;
  std::vector<SafeMessage> messages;
  messages.reserve(kBatchSize);

  for (auto _ : state) {
    state.PauseTiming();
    for (int i = 0; i < kBatchSize; ++i) {
      auto buf_or = producer_->Reserve(payload.size(), 1);
      if (buf_or.ok()) {
        std::memcpy(*buf_or, payload.data(), payload.size());
        producer_->Commit();
      }
    }
    state.ResumeTiming();

    size_t total_read = 0;
    while (total_read < kBatchSize) {
      auto res =
          consumer_->PollCopyBatch(reader, kBatchSize - total_read, messages);
      if (!res.ok()) {
        state.SkipWithError(res.status().ToString().c_str());
        break;
      }
      total_read += *res;
      benchmark::DoNotOptimize(messages);
    }
  }

  consumer_->UnregisterReader(reader);
  state.SetBytesProcessed(int64_t(state.iterations()) *
                          int64_t(state.range(0)) * kBatchSize);
}
BENCHMARK_REGISTER_F(BroadcastRingBufferBenchmark, PollCopyBatch)
    ->Range(8, 8 << 10);

}  // namespace cognitas::trading
BENCHMARK_MAIN();