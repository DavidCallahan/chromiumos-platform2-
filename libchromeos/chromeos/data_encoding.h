// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_DATA_ENCODING_H_
#define LIBCHROMEOS_CHROMEOS_DATA_ENCODING_H_

#include <string>
#include <utility>
#include <vector>

#include <chromeos/chromeos_export.h>

namespace chromeos {
namespace data_encoding {

using WebParamList = std::vector<std::pair<std::string, std::string>>;

// Encode/escape string to be used in the query portion of a URL.
// If |encodeSpaceAsPlus| is set to true, spaces are encoded as '+' instead
// of "%20"
CHROMEOS_EXPORT std::string UrlEncode(const char* data, bool encodeSpaceAsPlus);

inline std::string UrlEncode(const char* data) {
  return UrlEncode(data, true);
}

// Decodes/unescapes a URL. Replaces all %XX sequences with actual characters.
// Also replaces '+' with spaces.
CHROMEOS_EXPORT std::string UrlDecode(const char* data);

// Converts a list of key-value pairs into a string compatible with
// 'application/x-www-form-urlencoded' content encoding.
CHROMEOS_EXPORT std::string WebParamsEncode(const WebParamList& params,
                                            bool encodeSpaceAsPlus);

inline std::string WebParamsEncode(const WebParamList& params) {
  return WebParamsEncode(params, true);
}

// Parses a string of '&'-delimited key-value pairs (separated by '=') and
// encoded in a way compatible with 'application/x-www-form-urlencoded'
// content encoding.
CHROMEOS_EXPORT WebParamList WebParamsDecode(const std::string& data);

}  // namespace data_encoding
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DATA_ENCODING_H_