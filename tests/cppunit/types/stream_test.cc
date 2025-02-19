/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include <gtest/gtest.h>

#include "test_base.h"
#include "time_util.h"
#include "types/redis_stream.h"

class RedisStreamTest : public TestBase {  // NOLINT
 public:
  static void CheckStreamEntryValues(const std::vector<std::string> &got, const std::vector<std::string> &expected) {
    EXPECT_EQ(got.size(), expected.size());
    for (size_t i = 0; i < got.size(); ++i) {
      EXPECT_EQ(got[i], expected[i]);
    }
  }

 protected:
  RedisStreamTest() : name_("test_stream") { stream_ = new redis::Stream(storage_, "stream_ns"); }

  ~RedisStreamTest() override { delete stream_; }

  void SetUp() override { stream_->Del(name_); }

  void TearDown() override { stream_->Del(name_); }

  std::string name_;
  redis::Stream *stream_;
};

TEST_F(RedisStreamTest, EncodeDecodeEntryValue) {
  std::vector<std::string> values = {"day", "first", "month", "eleventh", "epoch", "fairly-very-old-one"};
  auto encoded = redis::EncodeStreamEntryValue(values);
  std::vector<std::string> decoded;
  auto s = redis::DecodeRawStreamEntryValue(encoded, &decoded);
  EXPECT_TRUE(s.IsOK());
  CheckStreamEntryValues(decoded, values);
}

TEST_F(RedisStreamTest, AddEntryToNonExistingStreamWithNomkstreamOption) {
  redis::StreamAddOptions options;
  options.nomkstream = true;
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(s.IsNotFound());
}

TEST_F(RedisStreamTest, AddEntryPredefinedIDAsZeroZero) {
  redis::StreamAddOptions options;
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{0, 0};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(!s.ok());
}

TEST_F(RedisStreamTest, AddEntryWithPredefinedIDAsZeroMsAndAnySeq) {
  redis::StreamAddOptions options;
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{0};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(id.ToString(), "0-1");
}

TEST_F(RedisStreamTest, AddFirstEntryWithoutPredefinedID) {
  redis::StreamAddOptions options;
  options.with_entry_id = false;
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(id.seq, 0);
  EXPECT_TRUE(id.ms <= util::GetTimeStampMS());
}

TEST_F(RedisStreamTest, AddEntryFirstEntryWithPredefinedID) {
  redis::StreamEntryID expected_id{12345, 6789};
  redis::StreamAddOptions options;
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{expected_id.ms, expected_id.seq};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(id.ms, expected_id.ms);
  EXPECT_EQ(id.seq, expected_id.seq);
}

TEST_F(RedisStreamTest, AddFirstEntryWithPredefinedNonZeroMsAndAnySeqNo) {
  uint64_t ms = util::GetTimeStampMS();
  redis::StreamAddOptions options;
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{ms};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(id.ms, ms);
  EXPECT_EQ(id.seq, 0);
}

TEST_F(RedisStreamTest, AddEntryToNonEmptyStreamWithPredefinedMsAndAnySeqNo) {
  redis::StreamAddOptions options;
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{12345, 678};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, options, values1, &id1);
  EXPECT_TRUE(s.ok());
  options.entry_id = redis::NewStreamEntryID{12346};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, options, values2, &id2);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(id2.ToString(), "12346-0");
}

TEST_F(RedisStreamTest, AddEntryWithPredefinedButExistingMsAndAnySeqNo) {
  uint64_t ms = 12345;
  uint64_t seq = 6789;
  redis::StreamAddOptions options;
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{ms, seq};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(s.ok());
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{ms};
  s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(id.ms, ms);
  EXPECT_EQ(id.seq, seq + 1);
}

TEST_F(RedisStreamTest, AddEntryWithExistingMsAnySeqNoAndExistingSeqNoIsAlreadyMax) {
  uint64_t ms = 12345;
  uint64_t seq = UINT64_MAX;
  redis::StreamAddOptions options;
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{ms, seq};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(s.ok());
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{ms};
  s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(!s.ok());
}

TEST_F(RedisStreamTest, AddEntryAndExistingMsAndSeqNoAreAlreadyMax) {
  uint64_t ms = UINT64_MAX;
  uint64_t seq = UINT64_MAX;
  redis::StreamAddOptions options;
  options.with_entry_id = true;
  options.entry_id = redis::NewStreamEntryID{ms, seq};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(s.ok());
  options.with_entry_id = false;
  s = stream_->Add(name_, options, values, &id);
  EXPECT_TRUE(!s.ok());
}

TEST_F(RedisStreamTest, AddEntryWithTrimMaxLenStrategy) {
  redis::StreamAddOptions add_options;
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions trim_options;
  trim_options.strategy = redis::StreamTrimStrategy::MaxLen;
  trim_options.max_len = 2;
  add_options.trim_options = trim_options;
  redis::StreamEntryID id3;
  std::vector<std::string> values3 = {"key3", "val3"};
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
  EXPECT_EQ(entries[1].key, id3.ToString());
  CheckStreamEntryValues(entries[1].values, values3);
}

TEST_F(RedisStreamTest, AddEntryWithTrimMaxLenStrategyThatDeletesAddedEntry) {
  redis::StreamAddOptions add_options;
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions trim_options;
  trim_options.strategy = redis::StreamTrimStrategy::MaxLen;
  trim_options.max_len = 0;
  add_options.trim_options = trim_options;
  redis::StreamEntryID id3;
  std::vector<std::string> values3 = {"key3", "val3"};
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, AddEntryWithTrimMinIdStrategy) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{12346, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions trim_options;
  trim_options.strategy = redis::StreamTrimStrategy::MinID;
  trim_options.min_id = redis::StreamEntryID{12346, 0};
  add_options.trim_options = trim_options;
  add_options.entry_id = redis::NewStreamEntryID{12347, 0};
  redis::StreamEntryID id3;
  std::vector<std::string> values3 = {"key3", "val3"};
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
  EXPECT_EQ(entries[1].key, id3.ToString());
  CheckStreamEntryValues(entries[1].values, values3);
}

TEST_F(RedisStreamTest, AddEntryWithTrimMinIdStrategyThatDeletesAddedEntry) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{12346, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions trim_options;
  trim_options.strategy = redis::StreamTrimStrategy::MinID;
  trim_options.min_id = redis::StreamEntryID{1234567, 0};
  add_options.trim_options = trim_options;
  add_options.entry_id = redis::NewStreamEntryID{12347, 0};
  redis::StreamEntryID id3;
  std::vector<std::string> values3 = {"key3", "val3"};
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RangeOnNonExistingStream) {
  redis::StreamRangeOptions options;
  options.start = redis::StreamEntryID{0, 0};
  options.end = redis::StreamEntryID{1234567, 0};
  std::vector<redis::StreamEntry> entries;
  auto s = stream_->Range(name_, options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RangeOnEmptyStream) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = false;
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, add_options, values, &id);
  EXPECT_TRUE(s.ok());
  uint64_t removed = 0;
  s = stream_->DeleteEntries(name_, {id}, &removed);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RangeWithStartAndEndSameMs) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345678, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{12345678, 1};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{12345679, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID{12345678, 0};
  range_options.end = redis::StreamEntryID{12345678, UINT64_MAX};
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id1.ToString());
  CheckStreamEntryValues(entries[0].values, values1);
  EXPECT_EQ(entries[1].key, id2.ToString());
  CheckStreamEntryValues(entries[1].values, values2);
}

TEST_F(RedisStreamTest, RangeInterval) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID{123456, 0};
  range_options.end = redis::StreamEntryID{123459, 0};
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 3);
  EXPECT_EQ(entries[0].key, id1.ToString());
  CheckStreamEntryValues(entries[0].values, values1);
  EXPECT_EQ(entries[1].key, id2.ToString());
  CheckStreamEntryValues(entries[1].values, values2);
  EXPECT_EQ(entries[2].key, id3.ToString());
  CheckStreamEntryValues(entries[2].values, values3);
}

TEST_F(RedisStreamTest, RangeFromMinimumToMaximum) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 4);
  EXPECT_EQ(entries[0].key, id1.ToString());
  CheckStreamEntryValues(entries[0].values, values1);
  EXPECT_EQ(entries[1].key, id2.ToString());
  CheckStreamEntryValues(entries[1].values, values2);
  EXPECT_EQ(entries[2].key, id3.ToString());
  CheckStreamEntryValues(entries[2].values, values3);
  EXPECT_EQ(entries[3].key, id4.ToString());
  CheckStreamEntryValues(entries[3].values, values4);
}

TEST_F(RedisStreamTest, RangeFromMinimumToMinimum) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Minimum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RangeWithStartGreaterThanEnd) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Maximum();
  range_options.end = redis::StreamEntryID::Minimum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RangeWithStartAndEndAreEqual) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = id2;
  range_options.end = id2;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 1);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
}

TEST_F(RedisStreamTest, RangeWithStartAndEndAreEqualAndExludedStart) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = id2;
  range_options.exclude_start = true;
  range_options.end = id2;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RangeWithStartAndEndAreEqualAndExludedEnd) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = id2;
  range_options.end = id2;
  range_options.exclude_end = true;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RangeWithExcludedStart) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID{123456, 1};
  range_options.exclude_start = true;
  range_options.end = redis::StreamEntryID{123458, 3};
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
  EXPECT_EQ(entries[1].key, id3.ToString());
  CheckStreamEntryValues(entries[1].values, values3);
}

TEST_F(RedisStreamTest, RangeWithExcludedEnd) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID{123457, 2};
  range_options.end = redis::StreamEntryID{123459, 4};
  range_options.exclude_end = true;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
  EXPECT_EQ(entries[1].key, id3.ToString());
  CheckStreamEntryValues(entries[1].values, values3);
}

TEST_F(RedisStreamTest, RangeWithExcludedStartAndExcludedEnd) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID{123456, 1};
  range_options.exclude_start = true;
  range_options.end = redis::StreamEntryID{123459, 4};
  range_options.exclude_end = true;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
  EXPECT_EQ(entries[1].key, id3.ToString());
  CheckStreamEntryValues(entries[1].values, values3);
}

TEST_F(RedisStreamTest, RangeWithStartAsMaximumAndExlusion) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Maximum();
  range_options.exclude_start = true;
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(!s.ok());
}

TEST_F(RedisStreamTest, RangeWithEndAsMinimumAndExlusion) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Minimum();
  range_options.exclude_end = true;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(!s.ok());
}

TEST_F(RedisStreamTest, RangeWithCountEqualToZero) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID{123456, 0};
  range_options.end = redis::StreamEntryID{123459, 0};
  range_options.with_count = true;
  range_options.count = 0;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RangeWithCountGreaterThanRequiredElements) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID{123456, 0};
  range_options.end = redis::StreamEntryID{123459, 0};
  range_options.with_count = true;
  range_options.count = 3;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 3);
  EXPECT_EQ(entries[0].key, id1.ToString());
  CheckStreamEntryValues(entries[0].values, values1);
  EXPECT_EQ(entries[1].key, id2.ToString());
  CheckStreamEntryValues(entries[1].values, values2);
  EXPECT_EQ(entries[2].key, id3.ToString());
  CheckStreamEntryValues(entries[2].values, values3);
}

TEST_F(RedisStreamTest, RangeWithCountLessThanRequiredElements) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID{123456, 0};
  range_options.end = redis::StreamEntryID{123459, 0};
  range_options.with_count = true;
  range_options.count = 2;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id1.ToString());
  CheckStreamEntryValues(entries[0].values, values1);
  EXPECT_EQ(entries[1].key, id2.ToString());
  CheckStreamEntryValues(entries[1].values, values2);
}

TEST_F(RedisStreamTest, RevRangeWithStartAndEndSameMs) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345678, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{12345678, 1};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{12345679, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = redis::StreamEntryID{12345678, UINT64_MAX};
  range_options.end = redis::StreamEntryID{12345678, 0};
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
  EXPECT_EQ(entries[1].key, id1.ToString());
  CheckStreamEntryValues(entries[1].values, values1);
}

TEST_F(RedisStreamTest, RevRangeInterval) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = redis::StreamEntryID{123459, 0};
  range_options.end = redis::StreamEntryID{123456, 0};
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 3);
  EXPECT_EQ(entries[0].key, id3.ToString());
  CheckStreamEntryValues(entries[0].values, values3);
  EXPECT_EQ(entries[1].key, id2.ToString());
  CheckStreamEntryValues(entries[1].values, values2);
  EXPECT_EQ(entries[2].key, id1.ToString());
  CheckStreamEntryValues(entries[2].values, values1);
}

TEST_F(RedisStreamTest, RevRangeFromMaximumToMinimum) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = redis::StreamEntryID::Maximum();
  range_options.end = redis::StreamEntryID::Minimum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 4);
  EXPECT_EQ(entries[0].key, id4.ToString());
  CheckStreamEntryValues(entries[0].values, values4);
  EXPECT_EQ(entries[1].key, id3.ToString());
  CheckStreamEntryValues(entries[1].values, values3);
  EXPECT_EQ(entries[2].key, id2.ToString());
  CheckStreamEntryValues(entries[2].values, values2);
  EXPECT_EQ(entries[3].key, id1.ToString());
  CheckStreamEntryValues(entries[3].values, values1);
}

TEST_F(RedisStreamTest, RevRangeFromMinimumToMinimum) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Minimum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RevRangeWithStartLessThanEnd) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RevRangeStartAndEndAreEqual) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = id2;
  range_options.end = id2;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 1);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
}

TEST_F(RedisStreamTest, RevRangeStartAndEndAreEqualAndExcludedStart) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = id2;
  range_options.exclude_start = true;
  range_options.end = id2;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RevRangeStartAndEndAreEqualAndExcludedEnd) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = id2;
  range_options.end = id2;
  range_options.exclude_end = true;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 0);
}

TEST_F(RedisStreamTest, RevRangeWithExcludedStart) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = redis::StreamEntryID{123458, 3};
  range_options.exclude_start = true;
  range_options.end = redis::StreamEntryID{123456, 1};
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
  EXPECT_EQ(entries[1].key, id1.ToString());
  CheckStreamEntryValues(entries[1].values, values1);
}

TEST_F(RedisStreamTest, RevRangeWithExcludedEnd) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = redis::StreamEntryID{123458, 3};
  range_options.end = redis::StreamEntryID{123456, 1};
  range_options.exclude_end = true;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id3.ToString());
  CheckStreamEntryValues(entries[0].values, values3);
  EXPECT_EQ(entries[1].key, id2.ToString());
  CheckStreamEntryValues(entries[1].values, values2);
}

TEST_F(RedisStreamTest, RevRangeWithExcludedStartAndExcludedEnd) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 1};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 2};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 3};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 4};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamRangeOptions range_options;
  range_options.reverse = true;
  range_options.start = redis::StreamEntryID{123459, 4};
  range_options.exclude_start = true;
  range_options.end = redis::StreamEntryID{123456, 1};
  range_options.exclude_end = true;
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id3.ToString());
  CheckStreamEntryValues(entries[0].values, values3);
  EXPECT_EQ(entries[1].key, id2.ToString());
  CheckStreamEntryValues(entries[1].values, values2);
}

TEST_F(RedisStreamTest, DeleteFromNonExistingStream) {
  std::vector<redis::StreamEntryID> ids = {redis::StreamEntryID{12345, 6789}};
  uint64_t deleted = 0;
  auto s = stream_->DeleteEntries(name_, ids, &deleted);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(deleted, 0);
}

TEST_F(RedisStreamTest, DeleteExistingEntry) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345, 6789};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, add_options, values, &id);
  EXPECT_TRUE(s.ok());

  std::vector<redis::StreamEntryID> ids = {id};
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, ids, &deleted);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(deleted, 1);
}

TEST_F(RedisStreamTest, DeleteNonExistingEntry) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345, 6789};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, add_options, values, &id);
  EXPECT_TRUE(s.ok());

  std::vector<redis::StreamEntryID> ids = {redis::StreamEntryID{123, 456}};
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, ids, &deleted);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(deleted, 0);
}

TEST_F(RedisStreamTest, DeleteMultipleEntries) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  std::vector<redis::StreamEntryID> ids = {redis::StreamEntryID{123456, 0}, redis::StreamEntryID{1234567, 89},
                                           redis::StreamEntryID{123458, 0}};
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, ids, &deleted);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(deleted, 2);

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id2.ToString());
  CheckStreamEntryValues(entries[0].values, values2);
  EXPECT_EQ(entries[1].key, id4.ToString());
  CheckStreamEntryValues(entries[1].values, values4);
}

TEST_F(RedisStreamTest, LenOnNonExistingStream) {
  uint64_t length = 0;
  auto s = stream_->Len(name_, redis::StreamLenOptions{}, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 0);
}

TEST_F(RedisStreamTest, LenOnEmptyStream) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345, 6789};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, add_options, values, &id);
  EXPECT_TRUE(s.ok());

  std::vector<redis::StreamEntryID> ids = {id};
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, ids, &deleted);
  EXPECT_TRUE(s.ok());

  uint64_t length = 0;
  s = stream_->Len(name_, redis::StreamLenOptions{}, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 0);
}

TEST_F(RedisStreamTest, Len) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  uint64_t length = 0;
  s = stream_->Len(name_, redis::StreamLenOptions{}, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 2);
}

TEST_F(RedisStreamTest, LenWithStartOptionGreaterThanLastEntryID) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;

  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, {"key1", "val1"}, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, {"key2", "val2"}, &id2);
  EXPECT_TRUE(s.ok());

  uint64_t length = 0;
  redis::StreamLenOptions len_options;
  len_options.with_entry_id = true;
  len_options.entry_id = redis::StreamEntryID{id2.ms + 10, 0};
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 0);

  len_options.to_first = true;
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 2);
}

TEST_F(RedisStreamTest, LenWithStartOptionEqualToLastEntryID) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;

  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, {"key1", "val1"}, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, {"key2", "val2"}, &id2);
  EXPECT_TRUE(s.ok());

  uint64_t length = 0;
  redis::StreamLenOptions len_options;
  len_options.with_entry_id = true;
  len_options.entry_id = redis::StreamEntryID{id2.ms, id2.seq};
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 0);

  len_options.to_first = true;
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 1);
}

TEST_F(RedisStreamTest, LenWithStartOptionLessThanFirstEntryID) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;

  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, {"key1", "val1"}, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, {"key2", "val2"}, &id2);
  EXPECT_TRUE(s.ok());

  uint64_t length = 0;
  redis::StreamLenOptions len_options;
  len_options.with_entry_id = true;
  len_options.entry_id = redis::StreamEntryID{123, 0};
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 2);

  len_options.to_first = true;
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 0);
}

TEST_F(RedisStreamTest, LenWithStartOptionEqualToFirstEntryID) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;

  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, {"key1", "val1"}, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, {"key2", "val2"}, &id2);
  EXPECT_TRUE(s.ok());

  uint64_t length = 0;
  redis::StreamLenOptions len_options;
  len_options.with_entry_id = true;
  len_options.entry_id = id1;
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 1);

  len_options.to_first = true;
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 0);
}

TEST_F(RedisStreamTest, LenWithStartOptionEqualToExistingEntryID) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;

  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, {"key1", "val1"}, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, {"key2", "val2"}, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, {"key3", "val3"}, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, {"key4", "val4"}, &id4);
  EXPECT_TRUE(s.ok());

  uint64_t length = 0;
  redis::StreamLenOptions len_options;
  len_options.with_entry_id = true;
  len_options.entry_id = id2;
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 2);

  len_options.to_first = true;
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 1);
}

TEST_F(RedisStreamTest, LenWithStartOptionNotEqualToExistingEntryID) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;

  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, {"key1", "val1"}, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, {"key2", "val2"}, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, {"key3", "val3"}, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, {"key4", "val4"}, &id4);
  EXPECT_TRUE(s.ok());

  uint64_t length = 0;
  redis::StreamLenOptions len_options;
  len_options.with_entry_id = true;
  len_options.entry_id = redis::StreamEntryID{id1.ms, id1.seq + 10};
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 3);

  len_options.to_first = true;
  s = stream_->Len(name_, len_options, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 1);
}

TEST_F(RedisStreamTest, TrimNonExistingStream) {
  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MaxLen;
  options.max_len = 10;
  uint64_t trimmed = 0;
  auto s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 0);
}

TEST_F(RedisStreamTest, TrimEmptyStream) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345, 6789};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, add_options, values, &id);
  EXPECT_TRUE(s.ok());
  std::vector<redis::StreamEntryID> ids = {id};
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, ids, &deleted);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MaxLen;
  options.max_len = 10;
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 0);
}

TEST_F(RedisStreamTest, TrimWithNoStrategySpecified) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345, 6789};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, add_options, values, &id);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.min_id = redis::StreamEntryID{123456, 0};
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 0);
}

TEST_F(RedisStreamTest, TrimWithMaxLenGreaterThanStreamSize) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MaxLen;
  options.max_len = 10;
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 0);
}

TEST_F(RedisStreamTest, TrimWithMaxLenEqualToStreamSize) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MaxLen;
  options.max_len = 4;
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 0);
}

TEST_F(RedisStreamTest, TrimWithMaxLenLessThanStreamSize) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MaxLen;
  options.max_len = 2;
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 2);

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id3.ToString());
  CheckStreamEntryValues(entries[0].values, values3);
  EXPECT_EQ(entries[1].key, id4.ToString());
  CheckStreamEntryValues(entries[1].values, values4);
}

TEST_F(RedisStreamTest, TrimWithMaxLenEqualTo1) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MaxLen;
  options.max_len = 1;
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 3);

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 1);
  EXPECT_EQ(entries[0].key, id4.ToString());
  CheckStreamEntryValues(entries[0].values, values4);
}

TEST_F(RedisStreamTest, TrimWithMaxLenZero) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MaxLen;
  options.max_len = 0;
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 4);
  uint64_t length = 0;
  s = stream_->Len(name_, redis::StreamLenOptions{}, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 0);
}

TEST_F(RedisStreamTest, TrimWithMinIdLessThanFirstEntryID) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MinID;
  options.min_id = redis::StreamEntryID{12345, 0};
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 0);
}

TEST_F(RedisStreamTest, TrimWithMinIdEqualToFirstEntryID) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MinID;
  options.min_id = redis::StreamEntryID{123456, 0};
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 0);
}

TEST_F(RedisStreamTest, TrimWithMinId) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MinID;
  options.min_id = redis::StreamEntryID{123457, 10};
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 2);

  redis::StreamRangeOptions range_options;
  range_options.start = redis::StreamEntryID::Minimum();
  range_options.end = redis::StreamEntryID::Maximum();
  std::vector<redis::StreamEntry> entries;
  s = stream_->Range(name_, range_options, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].key, id3.ToString());
  CheckStreamEntryValues(entries[0].values, values3);
  EXPECT_EQ(entries[1].key, id4.ToString());
  CheckStreamEntryValues(entries[1].values, values4);
}

TEST_F(RedisStreamTest, TrimWithMinIdGreaterThanLastEntryID) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MinID;
  options.min_id = redis::StreamEntryID{12345678, 0};
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(trimmed, 4);

  uint64_t length = 0;
  s = stream_->Len(name_, redis::StreamLenOptions{}, &length);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(length, 0);
}

TEST_F(RedisStreamTest, StreamInfoOnNonExistingStream) {
  redis::StreamInfo info;
  auto s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.IsNotFound());
}

TEST_F(RedisStreamTest, StreamInfoOnEmptyStream) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345, 6789};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, add_options, values, &id);
  EXPECT_TRUE(s.ok());

  std::vector<redis::StreamEntryID> ids = {id};
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, ids, &deleted);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(info.last_generated_id.ToString(), id.ToString());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), id.ToString());
  EXPECT_EQ(info.entries_added, 1);
  EXPECT_EQ(info.recorded_first_entry_id.ToString(), "0-0");
  EXPECT_FALSE(info.first_entry);
  EXPECT_FALSE(info.last_entry);
}

TEST_F(RedisStreamTest, StreamInfoOneEntry) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{12345, 6789};
  std::vector<std::string> values = {"key1", "val1"};
  redis::StreamEntryID id;
  auto s = stream_->Add(name_, add_options, values, &id);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.size, 1);
  EXPECT_EQ(info.last_generated_id.ToString(), id.ToString());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), "0-0");
  EXPECT_EQ(info.entries_added, 1);
  EXPECT_EQ(info.recorded_first_entry_id.ToString(), id.ToString());
  EXPECT_TRUE(info.first_entry);
  EXPECT_EQ(info.first_entry->key, id.ToString());
  CheckStreamEntryValues(info.first_entry->values, values);
  EXPECT_TRUE(info.last_entry);
  EXPECT_EQ(info.last_entry->key, id.ToString());
  CheckStreamEntryValues(info.last_entry->values, values);
}

TEST_F(RedisStreamTest, StreamInfoOnStreamWithElements) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.size, 3);
  EXPECT_EQ(info.last_generated_id.ToString(), id3.ToString());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), "0-0");
  EXPECT_EQ(info.entries_added, 3);
  EXPECT_EQ(info.recorded_first_entry_id.ToString(), id1.ToString());
  EXPECT_TRUE(info.first_entry);
  EXPECT_EQ(info.first_entry->key, id1.ToString());
  CheckStreamEntryValues(info.first_entry->values, values1);
  EXPECT_TRUE(info.last_entry);
  EXPECT_EQ(info.last_entry->key, id3.ToString());
  CheckStreamEntryValues(info.last_entry->values, values3);
  EXPECT_EQ(info.entries.size(), 0);
}

TEST_F(RedisStreamTest, StreamInfoOnStreamWithElementsFullOption) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, true, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.size, 3);
  EXPECT_EQ(info.last_generated_id.ToString(), id3.ToString());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), "0-0");
  EXPECT_EQ(info.entries_added, 3);
  EXPECT_EQ(info.recorded_first_entry_id.ToString(), id1.ToString());
  EXPECT_FALSE(info.first_entry);
  EXPECT_FALSE(info.last_entry);
  EXPECT_EQ(info.entries.size(), 3);
  EXPECT_EQ(info.entries[0].key, id1.ToString());
  CheckStreamEntryValues(info.entries[0].values, values1);
  EXPECT_EQ(info.entries[1].key, id2.ToString());
  CheckStreamEntryValues(info.entries[1].values, values2);
  EXPECT_EQ(info.entries[2].key, id3.ToString());
  CheckStreamEntryValues(info.entries[2].values, values3);
}

TEST_F(RedisStreamTest, StreamInfoCheckAfterLastEntryDeletion) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  std::vector<redis::StreamEntryID> ids = {id3};
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, ids, &deleted);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.size, 2);
  EXPECT_EQ(info.last_generated_id.ToString(), id3.ToString());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), id3.ToString());
  EXPECT_EQ(info.entries_added, 3);
  EXPECT_EQ(info.recorded_first_entry_id.ToString(), id1.ToString());
  EXPECT_TRUE(info.first_entry);
  EXPECT_EQ(info.first_entry->key, id1.ToString());
  CheckStreamEntryValues(info.first_entry->values, values1);
  EXPECT_TRUE(info.last_entry);
  EXPECT_EQ(info.last_entry->key, id2.ToString());
  CheckStreamEntryValues(info.last_entry->values, values2);
  EXPECT_EQ(info.entries.size(), 0);
}

TEST_F(RedisStreamTest, StreamInfoCheckAfterFirstEntryDeletion) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());

  std::vector<redis::StreamEntryID> ids = {id1};
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, ids, &deleted);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.size, 2);
  EXPECT_EQ(info.last_generated_id.ToString(), id3.ToString());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), id1.ToString());
  EXPECT_EQ(info.entries_added, 3);
  EXPECT_EQ(info.recorded_first_entry_id.ToString(), id2.ToString());
  EXPECT_TRUE(info.first_entry);
  EXPECT_EQ(info.first_entry->key, id2.ToString());
  CheckStreamEntryValues(info.first_entry->values, values2);
  EXPECT_TRUE(info.last_entry);
  EXPECT_EQ(info.last_entry->key, id3.ToString());
  CheckStreamEntryValues(info.last_entry->values, values3);
  EXPECT_EQ(info.entries.size(), 0);
}

TEST_F(RedisStreamTest, StreamInfoCheckAfterTrimMinId) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MinID;
  options.min_id = redis::StreamEntryID{123458, 0};
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.size, 2);
  EXPECT_EQ(info.last_generated_id.ToString(), id4.ToString());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), id2.ToString());
  EXPECT_EQ(info.entries_added, 4);
  EXPECT_EQ(info.recorded_first_entry_id.ToString(), id3.ToString());
  EXPECT_TRUE(info.first_entry);
  EXPECT_EQ(info.first_entry->key, id3.ToString());
  CheckStreamEntryValues(info.first_entry->values, values3);
  EXPECT_TRUE(info.last_entry);
  EXPECT_EQ(info.last_entry->key, id4.ToString());
  CheckStreamEntryValues(info.last_entry->values, values4);
  EXPECT_EQ(info.entries.size(), 0);
}

TEST_F(RedisStreamTest, StreamInfoCheckAfterTrimMaxLen) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MaxLen;
  options.max_len = 2;
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.size, 2);
  EXPECT_EQ(info.last_generated_id.ToString(), id4.ToString());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), id2.ToString());
  EXPECT_EQ(info.entries_added, 4);
  EXPECT_EQ(info.recorded_first_entry_id.ToString(), id3.ToString());
  EXPECT_TRUE(info.first_entry);
  EXPECT_EQ(info.first_entry->key, id3.ToString());
  CheckStreamEntryValues(info.first_entry->values, values3);
  EXPECT_TRUE(info.last_entry);
  EXPECT_EQ(info.last_entry->key, id4.ToString());
  CheckStreamEntryValues(info.last_entry->values, values4);
  EXPECT_EQ(info.entries.size(), 0);
}

TEST_F(RedisStreamTest, StreamInfoCheckAfterTrimAllEntries) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123457, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  s = stream_->Add(name_, add_options, values2, &id2);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123458, 0};
  std::vector<std::string> values3 = {"key3", "val3"};
  redis::StreamEntryID id3;
  s = stream_->Add(name_, add_options, values3, &id3);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123459, 0};
  std::vector<std::string> values4 = {"key4", "val4"};
  redis::StreamEntryID id4;
  s = stream_->Add(name_, add_options, values4, &id4);
  EXPECT_TRUE(s.ok());

  redis::StreamTrimOptions options;
  options.strategy = redis::StreamTrimStrategy::MaxLen;
  options.max_len = 0;
  uint64_t trimmed = 0;
  s = stream_->Trim(name_, options, &trimmed);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(info.last_generated_id.ToString(), id4.ToString());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), id4.ToString());
  EXPECT_EQ(info.entries_added, 4);
  EXPECT_EQ(info.recorded_first_entry_id.ToString(), "0-0");
  EXPECT_FALSE(info.first_entry);
  EXPECT_FALSE(info.last_entry);
  EXPECT_EQ(info.entries.size(), 0);
}

TEST_F(RedisStreamTest, StreamSetIdNonExistingStreamCreatesEmptyStream) {
  redis::StreamEntryID last_id(5, 0);
  std::optional<redis::StreamEntryID> max_del_id = redis::StreamEntryID{2, 0};
  uint64_t entries_added = 3;
  auto s = stream_->SetId("some-non-existing-stream1", last_id, entries_added, max_del_id);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo("some-non-existing-stream1", false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.last_generated_id.ToString(), last_id.ToString());
  EXPECT_EQ(info.entries_added, entries_added);
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), max_del_id->ToString());

  s = stream_->SetId("some-non-existing-stream2", last_id, std::nullopt, max_del_id);
  EXPECT_FALSE(s.ok());

  s = stream_->SetId("some-non-existing-stream3", last_id, entries_added, std::nullopt);
  EXPECT_FALSE(s.ok());
}

TEST_F(RedisStreamTest, StreamSetIdLastIdLessThanExisting) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());

  s = stream_->SetId(name_, {1, 0}, std::nullopt, std::nullopt);
  EXPECT_FALSE(s.ok());
}

TEST_F(RedisStreamTest, StreamSetIdEntriesAddedLessThanStreamSize) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values2 = {"key2", "val2"};
  redis::StreamEntryID id2;
  stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());

  s = stream_->SetId(name_, {id2.ms + 1, 0}, 1, std::nullopt);
  EXPECT_FALSE(s.ok());
}

TEST_F(RedisStreamTest, StreamSetIdLastIdEqualToExisting) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());

  s = stream_->SetId(name_, {id1.ms, id1.seq}, std::nullopt, std::nullopt);
  EXPECT_TRUE(s.ok());
}

TEST_F(RedisStreamTest, StreamSetIdMaxDeletedIdLessThanCurrent) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, {id1}, &deleted);
  EXPECT_TRUE(s.ok());

  std::optional<redis::StreamEntryID> max_del_id = redis::StreamEntryID{1, 0};
  s = stream_->SetId(name_, {id1.ms, id1.seq}, std::nullopt, max_del_id);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), max_del_id->ToString());
}

TEST_F(RedisStreamTest, StreamSetIdMaxDeletedIdIsZero) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, {id1}, &deleted);
  EXPECT_TRUE(s.ok());

  std::optional<redis::StreamEntryID> max_del_id = redis::StreamEntryID{0, 0};
  s = stream_->SetId(name_, {id1.ms, id1.seq}, std::nullopt, max_del_id);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), id1.ToString());
}

TEST_F(RedisStreamTest, StreamSetIdMaxDeletedIdGreaterThanLastGeneratedId) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());
  uint64_t deleted = 0;
  s = stream_->DeleteEntries(name_, {id1}, &deleted);
  EXPECT_TRUE(s.ok());

  std::optional<redis::StreamEntryID> max_del_id = redis::StreamEntryID{id1.ms + 1, 0};
  s = stream_->SetId(name_, {id1.ms, id1.seq}, std::nullopt, max_del_id);
  EXPECT_FALSE(s.ok());
}

TEST_F(RedisStreamTest, StreamSetIdLastIdGreaterThanExisting) {
  redis::StreamAddOptions add_options;
  add_options.with_entry_id = true;
  add_options.entry_id = redis::NewStreamEntryID{123456, 0};
  std::vector<std::string> values1 = {"key1", "val1"};
  redis::StreamEntryID id1;
  auto s = stream_->Add(name_, add_options, values1, &id1);
  EXPECT_TRUE(s.ok());

  s = stream_->SetId(name_, {id1.ms + 1, id1.seq}, std::nullopt, std::nullopt);
  EXPECT_TRUE(s.ok());

  uint64_t added = 10;
  s = stream_->SetId(name_, {id1.ms + 1, id1.seq}, added, std::nullopt);
  EXPECT_TRUE(s.ok());

  redis::StreamInfo info;
  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.entries_added, added);

  added = 5;
  std::optional<redis::StreamEntryID> max_del_id = redis::StreamEntryID{5, 0};
  s = stream_->SetId(name_, {id1.ms + 1, id1.seq}, added, max_del_id);
  EXPECT_TRUE(s.ok());

  s = stream_->GetStreamInfo(name_, false, 0, &info);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(info.entries_added, added);
  EXPECT_EQ(info.max_deleted_entry_id.ToString(), max_del_id->ToString());
}
