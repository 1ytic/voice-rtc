/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "inference/common/DefaultMemoryManager.h"

#include <cstdlib>
#include <string>

namespace w2l {
namespace streaming {

DefaultMemoryManager::DefaultMemoryManager() {}

void* DefaultMemoryManager::allocate(size_t sizeInBytes) {
  return std::malloc(sizeInBytes);
}

void DefaultMemoryManager::free(void* ptr) {
  std::free(ptr);
}

std::string DefaultMemoryManager::debugString() const {
  return "Default Memory Manager";
}

} // namespace streaming
} // namespace w2l
