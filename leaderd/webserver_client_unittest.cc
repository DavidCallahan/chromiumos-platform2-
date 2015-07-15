// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/webserver_client.h"

#include <gtest/gtest.h>

namespace leaderd {

namespace {
const char kGroupId[] = "ABC";
}  // namespace

class WebServerClientTest : public testing::Test,
                            public WebServerClient::Delegate {
 public:
  WebServerClientTest() : webserver_(this, "protocol_handler_name") {}

  std::unique_ptr<base::DictionaryValue> ProcessDiscover(
      const base::DictionaryValue* input) {
    return webserver_.ProcessDiscover(input);
  }

  std::unique_ptr<base::DictionaryValue> ProcessChallenge(
      const base::DictionaryValue* input) {
    return webserver_.ProcessChallenge(input);
  }

  std::unique_ptr<base::DictionaryValue> ProcessAnnouncement(
      const base::DictionaryValue* input) {
    return webserver_.ProcessAnnouncement(input);
  }

  void SetWebServerPort(uint16_t port) override {}

  bool HandleLeaderDiscover(const std::string& in_guid,
                            std::string* out_leader) override {
    if (in_guid != kGroupId) return false;
    *out_leader = "This is my own ID.";
    return true;
  }

  bool HandleLeaderChallenge(const std::string& in_guid,
                             const std::string& in_uuid,
                             int32_t in_score,
                             std::string* out_leader,
                             std::string* out_my_uuid) override {
    if (in_guid != kGroupId) return false;
    *out_leader = in_uuid;
    *out_my_uuid = "This is my own ID.";
    return true;
  }

  bool HandleLeaderAnnouncement(const std::string& group_id,
                                const std::string& leader_id,
                                int32_t leader_score) override {
    if (group_id != kGroupId) return false;
    return true;
  }

  std::unique_ptr<base::DictionaryValue> GetValidDiscoverInput() {
    std::unique_ptr<base::DictionaryValue> input{new base::DictionaryValue()};
    input->SetString(http_api::kDiscoverGroupKey, kGroupId);
    return input;
  }

  std::unique_ptr<base::DictionaryValue> GetValidChallengeInput() {
    std::unique_ptr<base::DictionaryValue> input{new base::DictionaryValue()};
    input->SetInteger(http_api::kChallengeScoreKey, 23);
    input->SetString(http_api::kChallengeGroupKey, kGroupId);
    input->SetString(http_api::kChallengeIdKey, "this is the challenger's ID");
    return input;
  }

  std::unique_ptr<base::DictionaryValue> GetValidAnnouncementInput() {
    std::unique_ptr<base::DictionaryValue> input{new base::DictionaryValue()};
    input->SetInteger(http_api::kAnnounceScoreKey, 23);
    input->SetString(http_api::kAnnounceGroupKey, kGroupId);
    input->SetString(http_api::kAnnounceLeaderIdKey, "This is the leader's ID");
    return input;
  }

 private:
  WebServerClient webserver_;
};

TEST_F(WebServerClientTest, ChallengeBadData) {
  EXPECT_EQ(nullptr, ProcessChallenge(nullptr));
}

TEST_F(WebServerClientTest, ChallengeRejectsExtraFields) {
  auto input = GetValidChallengeInput();
  input->SetString("BogusField", kGroupId);
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
}

TEST_F(WebServerClientTest, ChallengeRejectsMissingFields) {
  // We need group to exist.
  auto input = GetValidChallengeInput();
  input->Remove(http_api::kChallengeGroupKey, nullptr);
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
  // Similarly, the challenger id
  input = GetValidChallengeInput();
  input->Remove(http_api::kChallengeIdKey, nullptr);
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
  // Similarly, the score.
  input = GetValidChallengeInput();
  input->Remove(http_api::kChallengeScoreKey, nullptr);
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
}

TEST_F(WebServerClientTest, ChallengeScoreAsTextFail) {
  auto input = GetValidChallengeInput();
  input->SetString(http_api::kChallengeScoreKey, "23");
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
}

TEST_F(WebServerClientTest, ChallengeDelegateFails) {
  auto input = GetValidChallengeInput();
  input->SetString(http_api::kChallengeGroupKey, "not-the-expected-value");
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
}

TEST_F(WebServerClientTest, ChallengeDelegateSuccess) {
  auto input = GetValidChallengeInput();
  std::unique_ptr<base::DictionaryValue> output = ProcessChallenge(input.get());
  EXPECT_NE(nullptr, output);
}

TEST_F(WebServerClientTest, AnnouncementBadData) {
  std::unique_ptr<base::DictionaryValue> output = ProcessAnnouncement(nullptr);
  EXPECT_EQ(nullptr, output);
}

TEST_F(WebServerClientTest, AnnouncementRejectsExtraFields) {
  auto input = GetValidAnnouncementInput();
  input->SetString("BogusField", kGroupId);
  std::unique_ptr<base::DictionaryValue> output =
      ProcessAnnouncement(input.get());
  EXPECT_EQ(nullptr, output);
}

TEST_F(WebServerClientTest, AnnouncementRejectsMissingFields) {
  // We need group to exist.
  auto input = GetValidAnnouncementInput();
  input->Remove(http_api::kAnnounceGroupKey, nullptr);
  std::unique_ptr<base::DictionaryValue> output =
      ProcessAnnouncement(input.get());
  EXPECT_EQ(nullptr, output);
  // Similarly, the leader id
  input = GetValidAnnouncementInput();
  input->Remove(http_api::kAnnounceLeaderIdKey, nullptr);
  output = ProcessAnnouncement(input.get());
  EXPECT_EQ(nullptr, output);
  // Similarly, the score.
  input = GetValidAnnouncementInput();
  input->Remove(http_api::kAnnounceScoreKey, nullptr);
  output = ProcessAnnouncement(input.get());
  EXPECT_EQ(nullptr, output);
}

TEST_F(WebServerClientTest, AnnouncementScoreAsTextFail) {
  auto input = GetValidAnnouncementInput();
  input->SetString(http_api::kAnnounceScoreKey, "23");
  std::unique_ptr<base::DictionaryValue> output =
      ProcessAnnouncement(input.get());
  EXPECT_EQ(nullptr, output);
}

TEST_F(WebServerClientTest, AnnouncementDelegateFails) {
  auto input = GetValidAnnouncementInput();
  input->SetString(http_api::kAnnounceGroupKey, "not-the-expected-value");
  std::unique_ptr<base::DictionaryValue> output =
      ProcessAnnouncement(input.get());
  EXPECT_EQ(nullptr, output);
}

TEST_F(WebServerClientTest, AnnouncementDelegateSuccess) {
  auto input = GetValidAnnouncementInput();
  std::unique_ptr<base::DictionaryValue> output =
      ProcessAnnouncement(input.get());
  EXPECT_NE(nullptr, output);
}

TEST_F(WebServerClientTest, DiscoverBadData) {
  std::unique_ptr<base::DictionaryValue> output = ProcessDiscover(nullptr);
  EXPECT_EQ(nullptr, output);
}

TEST_F(WebServerClientTest, DiscoverRejectsExtraFields) {
  auto input = GetValidDiscoverInput();
  input->SetString("BogusField", kGroupId);
  std::unique_ptr<base::DictionaryValue> output =
      ProcessDiscover(input.get());
  EXPECT_EQ(nullptr, output);
}

TEST_F(WebServerClientTest, DiscoverRejectsMissingFields) {
  // We need group to exist.
  auto input = GetValidDiscoverInput();
  input->Remove(http_api::kDiscoverGroupKey, nullptr);
  std::unique_ptr<base::DictionaryValue> output =
      ProcessDiscover(input.get());
  EXPECT_EQ(nullptr, output);
}

TEST_F(WebServerClientTest, DiscoverDelegateFails) {
  auto input = GetValidDiscoverInput();
  input->SetString(http_api::kAnnounceGroupKey, "not-the-expected-value");
  std::unique_ptr<base::DictionaryValue> output =
      ProcessDiscover(input.get());
  EXPECT_EQ(nullptr, output);
}

TEST_F(WebServerClientTest, DiscoverDelegateSuccess) {
  auto input = GetValidDiscoverInput();
  std::unique_ptr<base::DictionaryValue> output =
      ProcessDiscover(input.get());
  EXPECT_NE(nullptr, output);
}

}  // namespace leaderd