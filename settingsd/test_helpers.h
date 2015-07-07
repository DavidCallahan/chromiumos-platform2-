// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_TEST_HELPERS_H_
#define SETTINGSD_TEST_HELPERS_H_

#include <memory>
#include <string>

#include <base/values.h>

namespace settingsd {

std::unique_ptr<base::Value> MakeIntValue(int i);
std::unique_ptr<base::Value> MakeNullValue();
std::unique_ptr<base::Value> MakeStringValue(const std::string& str);

}  // namespace settingsd

#endif  // SETTINGSD_TEST_HELPERS_H_