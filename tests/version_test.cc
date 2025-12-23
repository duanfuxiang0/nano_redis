// Copyright 2024 Nano-Redis Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "nano_redis/version.h"
#include "nano_redis/status.h"

#include <gtest/gtest.h>

namespace nano_redis {

TEST(VersionTest, GetVersionReturnsCorrectString) {
  EXPECT_EQ(GetVersion(), "0.1.0");
}

TEST(VersionTest, VersionConstants) {
  EXPECT_EQ(kMajorVersion, 0);
  EXPECT_EQ(kMinorVersion, 1);
  EXPECT_EQ(kPatchVersion, 0);
  EXPECT_STREQ(kVersionString, "0.1.0");
}

TEST(StatusTest, OKStatus) {
  Status s = Status::OK();
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ToString(), "OK");
  EXPECT_EQ(s.message(), "");
}

TEST(StatusTest, NotFoundStatus) {
  Status s = Status::NotFound("key not found");
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kNotFound);
  EXPECT_EQ(s.ToString(), "NOT_FOUND: key not found");
}

TEST(StatusTest, InvalidArgumentStatus) {
  Status s = Status::InvalidArgument("invalid command");
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(s.ToString(), "INVALID_ARGUMENT: invalid command");
}

TEST(StatusTest, InternalErrorStatus) {
  Status s = Status::Internal("allocation failed");
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kInternalError);
  EXPECT_EQ(s.ToString(), "INTERNAL: allocation failed");
}

TEST(StatusTest, AlreadyExistsStatus) {
  Status s = Status::AlreadyExists("key already exists");
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kAlreadyExists);
  EXPECT_EQ(s.ToString(), "ALREADY_EXISTS: key already exists");
}

}  // namespace nano_redis

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
