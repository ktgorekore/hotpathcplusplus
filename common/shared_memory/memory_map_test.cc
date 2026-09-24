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

#include "common/shared_memory/memory_map.h"

#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "absl/strings/str_cat.h"

namespace cognitas::trading {

class MemoryMapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    shm_name_ = absl::StrCat("/test_shm_map_", getpid(), "_",
                             testing::UnitTest::GetInstance()->random_seed(),
                             "_", test_counter_++);
    shm_unlink(shm_name_.c_str());
  }

  void TearDown() override { shm_unlink(shm_name_.c_str()); }

  std::string shm_name_;
  static int test_counter_;
};

int MemoryMapTest::test_counter_ = 0;

TEST_F(MemoryMapTest, BasicCreateAndOpen) {
  size_t size = 4096;
  auto map_or = MemoryMap::Create(shm_name_, size);
  ASSERT_TRUE(map_or.ok()) << map_or.status();

  auto& map = *map_or;
  EXPECT_EQ(map.size(), size);
  EXPECT_NE(map.addr(), nullptr);

  // Write something
  static_cast<char*>(map.addr())[0] = 'A';

  // Open it
  auto map_open_or = MemoryMap::Open(shm_name_, size);
  ASSERT_TRUE(map_open_or.ok()) << map_open_or.status();
  EXPECT_EQ(static_cast<char*>(map_open_or->addr())[0], 'A');
}

TEST_F(MemoryMapTest, MagicMirror) {
  long page_size = sysconf(_SC_PAGESIZE);
  size_t size = page_size;

  auto map_or = MemoryMap::Create(shm_name_, size, true);
  ASSERT_TRUE(map_or.ok()) << map_or.status();

  auto& map = *map_or;
  EXPECT_TRUE(map.is_mirrored());
  EXPECT_EQ(map.size(), size);

  uint8_t* base = static_cast<uint8_t*>(map.addr());
  uint8_t* mirror = base + size;

  // Write to base, read from mirror
  base[0] = 0xDE;
  base[1] = 0xAD;
  base[size - 1] = 0xBE;

  EXPECT_EQ(mirror[0], 0xDE);
  EXPECT_EQ(mirror[1], 0xAD);
  EXPECT_EQ(mirror[size - 1], 0xBE);

  // Write to mirror, read from base
  mirror[10] = 0xEF;
  EXPECT_EQ(base[10], 0xEF);

  // Verify contiguous boundary write
  // A write of 2 bytes at size-1 should wrap correctly
  uint16_t val = 0x1234;
  std::memcpy(base + size - 1, &val, 2);

  uint16_t read_val;
  std::memcpy(&read_val, base + size - 1, 2);
  EXPECT_EQ(read_val, 0x1234);

  // Check the mirror manually
  EXPECT_EQ(base[size - 1], 0x34);  // Low byte
  EXPECT_EQ(base[0], 0x12);         // High byte (wrapped)
}

TEST_F(MemoryMapTest, InvalidSizeForMagicMirror) {
  // Magic mirror requires page alignment.
  auto map_or = MemoryMap::Create(shm_name_, 1024, true);
  EXPECT_FALSE(map_or.ok());
  EXPECT_EQ(map_or.status().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace cognitas::trading
