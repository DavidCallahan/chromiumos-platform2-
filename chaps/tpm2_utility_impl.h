// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_TPM2_UTILITY_IMPL_H_
#define CHAPS_TPM2_UTILITY_IMPL_H_

#include "chaps/tpm_utility.h"

#include <map>
#include <set>
#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>

#include "trunks/hmac_session.h"
#include "trunks/tpm_generated.h"
#include "trunks/tpm_utility.h"
#include "trunks/trunks_factory.h"

namespace chaps {

const uint32_t kMinModulusSize = 256;

class TPM2UtilityImpl : public TPMUtility {
 public:
  TPM2UtilityImpl();
  // Does not take ownership of |factory|.
  explicit TPM2UtilityImpl(trunks::TrunksFactory* factory);
  virtual ~TPM2UtilityImpl();
  bool Init() override;
  bool IsTPMAvailable() override;
  bool Authenticate(int slot_id,
                    const brillo::SecureBlob& auth_data,
                    const std::string& auth_key_blob,
                    const std::string& encrypted_master_key,
                    brillo::SecureBlob* master_key) override;
  bool ChangeAuthData(int slot_id,
                      const brillo::SecureBlob& old_auth_data,
                      const brillo::SecureBlob& new_auth_data,
                      const std::string& old_auth_key_blob,
                      std::string* new_auth_key_blob) override;
  bool GenerateRandom(int num_bytes, std::string* random_data) override;
  bool StirRandom(const std::string& entropy_data) override;
  bool GenerateKey(int slot,
                   int modulus_bits,
                   const std::string& public_exponent,
                   const brillo::SecureBlob& auth_data,
                   std::string* key_blob,
                   int* key_handle) override;
  bool GetPublicKey(int key_handle,
                    std::string* public_exponent,
                    std::string* modulus) override;
  bool WrapKey(int slot,
               const std::string& public_exponent,
               const std::string& modulus,
               const std::string& prime_factor,
               const brillo::SecureBlob& auth_data,
               std::string* key_blob,
               int* key_handle) override;
  bool LoadKey(int slot,
               const std::string& key_blob,
               const brillo::SecureBlob& auth_data,
               int* key_handle) override;
  bool LoadKeyWithParent(int slot,
                         const std::string& key_blob,
                         const brillo::SecureBlob& auth_data,
                         int parent_key_handle,
                         int* key_handle) override;
  void UnloadKeysForSlot(int slot) override;
  bool Bind(int key_handle,
            const std::string& input,
            std::string* output) override;
  bool Unbind(int key_handle,
              const std::string& input,
              std::string* output) override;
  bool Sign(int key_handle,
            const std::string& input,
            std::string* signature) override;
  bool Verify(int key_handle,
              const std::string& input,
              const std::string& signature) override;
  bool IsSRKReady() override;

 private:
  scoped_ptr<trunks::TrunksFactory> default_factory_;
  trunks::TrunksFactory* factory_;
  bool is_initialized_;
  bool is_enabled_ready_;
  bool is_enabled_;
  scoped_ptr<trunks::HmacSession> session_;
  scoped_ptr<trunks::TpmUtility> trunks_tpm_utility_;
  std::map<int, std::set<int>> slot_handles_;
  std::map<int, brillo::SecureBlob> handle_auth_data_;
  std::map<int, std::string> handle_name_;

  FRIEND_TEST(TPM2UtilityTest, IsTPMAvailable);
  FRIEND_TEST(TPM2UtilityTest, LoadKeySuccess);
  FRIEND_TEST(TPM2UtilityTest, UnloadKeysTest);

  DISALLOW_COPY_AND_ASSIGN(TPM2UtilityImpl);
};

}  // namespace chaps

#endif  // CHAPS_TPM2_UTILITY_IMPL_H_