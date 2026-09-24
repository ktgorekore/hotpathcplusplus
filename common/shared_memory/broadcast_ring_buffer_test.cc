// Copyright 2025 Cognitas Trading Inc.
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

#include "common/shared_memory/broadcast_ring_buffer.h"

#include <unistd.h>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "absl/strings/str_cat.h"
#include "common/shared_memory/memory_map.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cognitas::trading {
namespace {

using ::testing::HasSubstr;

class BroadcastRingBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    shm_name_ = absl::StrCat("/test_shm_", getpid(), "_",
                             testing::UnitTest::GetInstance()->random_seed(),
                             "_", test_counter_++);
    BroadcastRingBuffer::Unlink(shm_name_).IgnoreError();
  }

  void TearDown() override {
    BroadcastRingBuffer::Unlink(shm_name_).IgnoreError();
  }

  std::string shm_name_;
  static int test_counter_;
};

int BroadcastRingBufferTest::test_counter_ = 0;

TEST_F(BroadcastRingBufferTest, CreateAndAttach) {
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  auto producer_or = BroadcastRingBuffer::Create(shm_name_, kPageSize);
  ASSERT_TRUE(producer_or.ok()) << producer_or.status();

  auto consumer_or = BroadcastRingBuffer::Attach(shm_name_);
  ASSERT_TRUE(consumer_or.ok()) << consumer_or.status();

  EXPECT_EQ(producer_or.value()->GetLatestCursor(), 0ULL);
  EXPECT_EQ(consumer_or.value()->GetLatestCursor(), 0ULL);
}

TEST_F(BroadcastRingBufferTest, ProduceAndConsumeSingleMessage) {
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  auto producer = *BroadcastRingBuffer::Create(shm_name_, kPageSize);
  auto consumer = *BroadcastRingBuffer::Attach(shm_name_);

  BroadcastReader reader;
  ASSERT_TRUE(consumer->RegisterReader(reader).ok());

  const std::string kMessage = "Hello, Ring Buffer!";
  const uint32_t kType = 1;

  auto buffer_or = producer->Reserve(kMessage.size(), kType);
  ASSERT_TRUE(buffer_or.ok());
  std::memcpy(*buffer_or, kMessage.data(), kMessage.size());
  producer->Commit();

  FrameHeader header;
  auto msg_or = consumer->PollZeroCopy(reader, header);
  ASSERT_TRUE(msg_or.ok());

  EXPECT_EQ(header.type, kType);
  std::string received(reinterpret_cast<const char*>(msg_or->data()),
                       msg_or->size());
  EXPECT_EQ(received, kMessage);

  consumer->Advance(reader, header.TotalFrameSize());
}

TEST_F(BroadcastRingBufferTest, WrapAroundMagicMirror) {
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  const size_t kCapacity = kPageSize;
  auto producer = *BroadcastRingBuffer::Create(shm_name_, kCapacity);
  auto consumer = *BroadcastRingBuffer::Attach(shm_name_);
  BroadcastReader reader;
  ASSERT_TRUE(consumer->RegisterReader(reader).ok());

  // msg1: fill most of the buffer
  size_t msg1_size = kCapacity - 32;
  {
    auto buf = producer->Reserve(msg1_size, 1);
    ASSERT_TRUE(buf.ok());
    std::memset(*buf, 'A', msg1_size);
    producer->Commit();
  }

  FrameHeader header1;
  ASSERT_TRUE(consumer->PollZeroCopy(reader, header1).ok());
  consumer->Advance(reader, header1.TotalFrameSize());

  // msg2: wraps around
  std::string msg2(100, 'B');
  {
    auto buf = producer->Reserve(msg2.size(), 2);
    ASSERT_TRUE(buf.ok()) << buf.status();
    std::memcpy(*buf, msg2.data(), msg2.size());
    producer->Commit();
  }

  FrameHeader header2;
  auto res = consumer->PollZeroCopy(reader, header2);
  ASSERT_TRUE(res.ok()) << res.status();
  EXPECT_EQ(header2.type, 2u);
  EXPECT_EQ(res->size(), 100u);
  EXPECT_EQ((*res)[0], 'B');

  consumer->Advance(reader, header2.TotalFrameSize());
}

TEST_F(BroadcastRingBufferTest, ProducerBackpressureBlocking) {
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  const size_t kCapacity = kPageSize;
  auto producer = *BroadcastRingBuffer::Create(shm_name_, kCapacity);
  auto consumer = *BroadcastRingBuffer::Attach(shm_name_);

  BroadcastReader reader;
  ASSERT_TRUE(consumer->RegisterReader(reader).ok());

  // Producer fills the buffer
  size_t msg_size = kCapacity / 4;
  for (int i = 0; i < 3; ++i) {
    auto buf = producer->Reserve(msg_size, i);
    ASSERT_TRUE(buf.ok());
    producer->Commit();
  }

  // Next reserve should block because consumer hasn't read anything.
  // We'll run it in a thread and verify it's blocked, then read to unblock it.
  std::atomic<bool> reserved{false};
  std::thread producer_thread([&]() {
    auto buf = producer->Reserve(msg_size, 4);  // This will block
    ASSERT_TRUE(buf.ok());
    producer->Commit();
    reserved.store(true);
  });

  // Give the thread time to block
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(reserved.load());  // Should be blocked

  // Consumer reads one message
  FrameHeader header;
  auto res = consumer->PollZeroCopy(reader, header);
  ASSERT_TRUE(res.ok());
  consumer->Advance(reader, header.TotalFrameSize());

  // Now producer should unblock
  producer_thread.join();
  EXPECT_TRUE(reserved.load());
}

TEST_F(BroadcastRingBufferTest, HeartbeatEviction) {
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  const size_t kCapacity = kPageSize;
  auto producer = *BroadcastRingBuffer::Create(shm_name_, kCapacity);
  auto consumer = *BroadcastRingBuffer::Attach(shm_name_);

  BroadcastReader reader;
  ASSERT_TRUE(consumer->RegisterReader(reader).ok());

  // Fill the buffer
  size_t msg_size = kCapacity / 2;
  for (int i = 0; i < 1; ++i) {
    auto buf = producer->Reserve(msg_size, i);
    ASSERT_TRUE(buf.ok());
    producer->Commit();
  }

  // At this point, the consumer hasn't read, but we will wait >500ms to trigger
  // eviction
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  // The next reserve should evict the dead reader and proceed immediately
  auto buf = producer->Reserve(msg_size, 1);
  ASSERT_TRUE(buf.ok());
  producer->Commit();

  // The consumer tries to read now, it should get a ResourceExhaustedError
  // (evicted)
  FrameHeader header;
  auto res = consumer->PollZeroCopy(reader, header);
  EXPECT_EQ(res.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(BroadcastRingBufferTest, ReadOnlyCannotReserve) {
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  auto producer = *BroadcastRingBuffer::Create(shm_name_, kPageSize);

  auto consumer_or = BroadcastRingBuffer::Attach(shm_name_, /*writable=*/false);
  ASSERT_TRUE(consumer_or.ok());
  auto consumer = std::move(*consumer_or);

  auto buffer_or = consumer->Reserve(10, 1);
  EXPECT_FALSE(buffer_or.ok());
  EXPECT_EQ(buffer_or.status().code(), absl::StatusCode::kPermissionDenied);
}

TEST_F(BroadcastRingBufferTest, DataIntegrityStress) {
  const size_t kPageSize = sysconf(_SC_PAGESIZE);
  const size_t kCapacity = kPageSize * 16;
  // Use a 30s eviction timeout so CPU starvation/scheduling jitter under heavy
  // CI test loads does not falsely evict active readers.
  auto producer = *BroadcastRingBuffer::Create(
      shm_name_, kCapacity, /*evict_timeout_ns=*/30'000'000'000LL);

  const int kNumMessages = 5000;
  std::atomic<bool> start{false};
  std::atomic<int> registered_readers{0};

  auto consumer_fn = [&](int id) {
    auto consumer_or = BroadcastRingBuffer::Attach(shm_name_);
    ASSERT_TRUE(consumer_or.ok());
    auto consumer = std::move(*consumer_or);
    BroadcastReader reader;
    ASSERT_TRUE(consumer->RegisterReader(reader).ok());

    registered_readers.fetch_add(1, std::memory_order_release);

    while (!start.load(std::memory_order_acquire)) std::this_thread::yield();

    int messages_received = 0;
    uint32_t last_val = 0;

    while (messages_received < kNumMessages) {
      FrameHeader header;
      auto res = consumer->PollZeroCopy(reader, header);
      if (res.ok()) {
        uint32_t val;
        std::memcpy(&val, res->data(), sizeof(uint32_t));
        // With SPMC backpressure, we should get exactly monotonic values with
        // NO overruns
        if (val != last_val + 1) {
          ADD_FAILURE() << "Consumer " << id
                        << " read non-monotonic value: " << val << " after "
                        << last_val;
        }
        last_val = val;
        messages_received++;
        consumer->Advance(reader, header.TotalFrameSize());
      } else if (absl::IsResourceExhausted(res.status())) {
        ADD_FAILURE()
            << "Consumer should not be evicted in healthy backpressured test!";
        break;  // Break to avoid infinite hang in high-load bazel test
                // environments
      } else {
        std::this_thread::yield();
      }
    }
  };

  std::vector<std::thread> consumers;
  for (int i = 0; i < 4; ++i) consumers.emplace_back(consumer_fn, i);

  // Ensure all 4 consumers have registered their reader slots at cursor 0
  // before producing messages.
  while (registered_readers.load(std::memory_order_acquire) < 4) {
    std::this_thread::yield();
  }

  start.store(true, std::memory_order_release);
  for (int i = 1; i <= kNumMessages; ++i) {
    uint32_t val = i;
    auto buf = producer->Reserve(sizeof(uint32_t), 1);
    ASSERT_TRUE(buf.ok());
    std::memcpy(*buf, &val, sizeof(uint32_t));
    producer->Commit();
  }

  for (auto& t : consumers) t.join();
}

}  // namespace
}  // namespace cognitas::trading