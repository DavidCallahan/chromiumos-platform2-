// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DEVICE_POLICY_SERVICE_H_
#define LOGIN_MANAGER_DEVICE_POLICY_SERVICE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <crypto/scoped_nss_types.h>

#include "login_manager/crossystem.h"
#include "login_manager/owner_key_loss_mitigator.h"
#include "login_manager/policy_service.h"
#include "login_manager/vpd_process.h"

namespace crypto {
class RSAPrivateKey;
}

namespace enterprise_management {
class ChromeDeviceSettingsProto;
class PolicyFetchResponse;
}

namespace login_manager {
class KeyGenerator;
class LoginMetrics;
class NssUtil;
class OwnerKeyLossMitigator;
// Forward declaration.
typedef struct PK11SlotInfoStr PK11SlotInfo;

// A policy service specifically for device policy, adding in a few helpers for
// generating a new key for the device owner, handling key loss mitigation,
// storing owner properties etc.
class DevicePolicyService : public PolicyService {
 public:
  virtual ~DevicePolicyService();

  // Instantiates a regular (non-testing) device policy service instance.
  static DevicePolicyService* Create(
      LoginMetrics* metrics,
      PolicyKey* owner_key,
      OwnerKeyLossMitigator* mitigator,
      NssUtil* nss,
      Crossystem* crossystem,
      VpdProcess* vpd_process);

  // Checks whether the given |current_user| is the device owner. The result of
  // the check is returned in |is_owner|. If so, it is validated that the device
  // policy settings are set up appropriately:
  // - If |current_user| has the owner key, put her on the login white list.
  // - If policy claims |current_user| is the device owner but she doesn't
  //   appear to have the owner key, run key mitigation.
  // Returns true on success. Fills in |error| upon encountering an error.
  virtual bool CheckAndHandleOwnerLogin(const std::string& current_user,
                                        PK11SlotInfo* module,
                                        bool* is_owner,
                                        Error* error);

  // Ensures that the public key in |buf| is legitimately paired with a private
  // key held by the current user, signs and stores some ownership-related
  // metadata, and then stores this key off as the new device owner key. Returns
  // true if successful, false otherwise
  virtual bool ValidateAndStoreOwnerKey(const std::string& current_user,
                                        const std::string& buf,
                                        PK11SlotInfo* module);

  // Checks whether the key is missing.
  virtual bool KeyMissing();

  // Checks whether key loss is being mitigated.
  virtual bool Mitigating();

  // Loads policy key and policy blob from disk. Returns true if at least the
  // key can be loaded (policy may not be present yet, which is OK).
  virtual bool Initialize();

  // Given info about whether we were able to load the Owner key and the
  // device policy, report the state of these files via |metrics_|.
  virtual void ReportPolicyFileMetrics(bool key_success, bool policy_success);

  // Gets the value of the StartUpFlags policy as a vector of strings to be
  // supplied to Chrome when it is started.
  virtual std::vector<std::string> GetStartUpFlags();

  // Returns the currently active device settings.
  virtual const enterprise_management::ChromeDeviceSettingsProto& GetSettings();

  // Returns whether the device is enrolled by checking enterprise mode in
  // install attributes from disk.
  virtual bool InstallAttributesEnterpriseMode();

  // Returns whether system settings can be updated by checking that PolicyKey
  // is populated and the device is running on Chrome OS firmware.
  bool MayUpdateSystemSettings();

  // Updates the system settings flags in NVRAM and in VPD. A failure in NVRAM
  // update is not considered a fatal error because new functionality relies on
  // VPD when checking the settings. The old code is using NVRAM however, which
  // means we have to update that memory too. Returns whether VPD process
  // started succesfully and is running in a separate process. In this case,
  // |vpd_process_| is responsible for running |completion|; otherwise,
  // OnPolicyPersisted() is.
  bool UpdateSystemSettings(const Completion& completion);

  // PolicyService:
  bool Store(const uint8_t* policy_blob,
             uint32_t len,
             const Completion& completion,
             int flags) override;
  void PersistPolicyOnLoop(const Completion& completion) override;

  static const char kPolicyPath[];
  static const char kSerialRecoveryFlagFile[];

  // Format of this string is documented in device_management_backend.proto.
  static const char kDevicePolicyType[];

  // These are defined in Chromium source at
  // chrome/browser/chromeos/policy/enterprise_install_attributes.cc.  Sadly,
  // the protobuf contains a trailing zero after kEnterpriseDeviceMode.
  static const char kAttrEnterpriseMode[];
  static const char kEnterpriseDeviceMode[];

 private:
  friend class DevicePolicyServiceTest;
  friend class MockDevicePolicyService;

  // Takes ownership of |policy_store|.
  DevicePolicyService(const base::FilePath& serial_recovery_flag_file,
                      const base::FilePath& policy_file,
                      const base::FilePath& install_attributes_file,
                      std::unique_ptr<PolicyStore> policy_store,
                      PolicyKey* owner_key,
                      LoginMetrics* metrics,
                      OwnerKeyLossMitigator* mitigator,
                      NssUtil* nss,
                      Crossystem* crossystem,
                      VpdProcess* vpd_process);

  // Returns true if |policy| allows arbitrary new users to sign in.
  // Only exposed for testing.
  static bool PolicyAllowsNewUsers(
      const enterprise_management::PolicyFetchResponse& policy);

  // Given the private half of the owner keypair, this call whitelists
  // |current_user| and sets a property indicating
  // |current_user| is the owner in the current policy and schedules a
  // PersistPolicy().
  // Returns false on failure, with |error| set appropriately. |error| can be
  // NULL, should you wish to ignore the particulars.
  bool StoreOwnerProperties(const std::string& current_user,
                            crypto::RSAPrivateKey* signing_key,
                            Error* error);

  // Checks the user's NSS database to see if she has the private key.
  // Returns a pointer to it if so.
  crypto::RSAPrivateKey* GetOwnerKeyForGivenUser(
      const std::vector<uint8_t>& key,
      PK11SlotInfo* module,
      Error* error);

  // Returns true if the |current_user| is listed in |policy_| as the
  // device owner.  Returns false if not, or if that cannot be determined.
  bool GivenUserIsOwner(const std::string& current_user);

  // Checks the serial number recovery flag and updates the flag file.
  // TODO(mnissler): Remove once bogus enterprise serials are fixed.
  void UpdateSerialNumberRecoveryFlagFile();

  const base::FilePath serial_recovery_flag_file_;
  const base::FilePath policy_file_;
  const base::FilePath install_attributes_file_;
  LoginMetrics* metrics_;
  OwnerKeyLossMitigator* mitigator_;
  NssUtil* nss_;
  Crossystem* crossystem_;     // Owned by the caller.
  VpdProcess* vpd_process_;    // Owned by the caller.

  // Cached copy of the decoded device settings. Decoding happens on first
  // access, the cache is cleared whenever a new policy gets installed via
  // Store().
  std::unique_ptr<enterprise_management::ChromeDeviceSettingsProto> settings_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyService);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_DEVICE_POLICY_SERVICE_H_
