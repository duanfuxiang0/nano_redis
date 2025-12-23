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

#ifndef NANO_REDIS_VERSION_H_
#define NANO_REDIS_VERSION_H_

namespace nano_redis {

constexpr int kMajorVersion = 0;
constexpr int kMinorVersion = 1;
constexpr int kPatchVersion = 0;

constexpr const char* kVersionString = "0.1.0";

inline const char* GetVersion() { return kVersionString; }

}  // namespace nano_redis

#endif  // NANO_REDIS_VERSION_H_
