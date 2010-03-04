// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_AUTHENTICATOR_H_
#define CRYPTOHOME_MOCK_AUTHENTICATOR_H_

#include "cryptohome/authenticator.h"

#include <gmock/gmock.h>

namespace cryptohome {
class Credentials;

class MockAuthenticator : public Authenticator {
 public:
  MockAuthenticator() {}
  ~MockAuthenticator() {}
  MOCK_METHOD0(Init, bool());
  MOCK_CONST_METHOD1(TestAllMasterKeys, bool(const Credentials&));
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_AUTHENTICATOR_H_
