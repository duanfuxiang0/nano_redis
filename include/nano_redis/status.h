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

#ifndef NANO_REDIS_STATUS_H_
#define NANO_REDIS_STATUS_H_

#include <string>

namespace nano_redis {

enum class StatusCode : int {
  kOk = 0,
  kNotFound = 1,
  kInvalidArgument = 2,
  kInternalError = 3,
  kAlreadyExists = 4,
};

class Status {
 public:
  Status() : code_(StatusCode::kOk), message_("") {}
  Status(StatusCode code, const std::string& message)
      : code_(code), message_(message) {}

  static Status OK() { return Status(StatusCode::kOk, ""); }
  static Status NotFound(const std::string& msg) {
    return Status(StatusCode::kNotFound, msg);
  }
  static Status InvalidArgument(const std::string& msg) {
    return Status(StatusCode::kInvalidArgument, msg);
  }
  static Status Internal(const std::string& msg) {
    return Status(StatusCode::kInternalError, msg);
  }
  static Status AlreadyExists(const std::string& msg) {
    return Status(StatusCode::kAlreadyExists, msg);
  }

  bool ok() const { return code_ == StatusCode::kOk; }
  StatusCode code() const { return code_; }
  const std::string& message() const { return message_; }

  std::string ToString() const;

 private:
  StatusCode code_;
  std::string message_;
};

inline std::string Status::ToString() const {
  switch (code_) {
    case StatusCode::kOk:
      return "OK";
    case StatusCode::kNotFound:
      return "NOT_FOUND: " + message_;
    case StatusCode::kInvalidArgument:
      return "INVALID_ARGUMENT: " + message_;
    case StatusCode::kInternalError:
      return "INTERNAL: " + message_;
    case StatusCode::kAlreadyExists:
      return "ALREADY_EXISTS: " + message_;
    default:
      return "UNKNOWN";
  }
}

}  // namespace nano_redis

#endif  // NANO_REDIS_STATUS_H_
