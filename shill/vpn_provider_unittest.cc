// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>

#include "shill/vpn_provider.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_device_info.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_profile.h"
#include "shill/mock_store.h"
#include "shill/mock_vpn_driver.h"
#include "shill/mock_vpn_service.h"
#include "shill/nice_mock_control.h"

using std::string;
using std::unique_ptr;
using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;

namespace shill {

class VPNProviderTest : public testing::Test {
 public:
  VPNProviderTest()
      : metrics_(nullptr),
        manager_(&control_, nullptr, &metrics_, nullptr),
        device_info_(&control_, nullptr, &metrics_, &manager_),
        provider_(&control_, nullptr, &metrics_, &manager_) {}

  virtual ~VPNProviderTest() {}

 protected:
  static const char kHost[];
  static const char kName[];

  string GetServiceFriendlyName(const ServiceRefPtr &service) {
    return service->friendly_name();
  }

  void SetConnectState(const ServiceRefPtr &service,
                       Service::ConnectState state) {
    service->state_ = state;
  }

  void AddService(const VPNServiceRefPtr &service) {
    provider_.services_.push_back(service);
  }

  VPNServiceRefPtr GetServiceAt(int idx) {
    return provider_.services_[idx];
  }

  size_t GetServiceCount() const {
    return provider_.services_.size();
  }

  NiceMockControl control_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  VPNProvider provider_;
};

const char VPNProviderTest::kHost[] = "10.8.0.1";
const char VPNProviderTest::kName[] = "vpn-name";

TEST_F(VPNProviderTest, GetServiceNoType) {
  KeyValueStore args;
  Error e;
  args.SetString(kTypeProperty, kTypeVPN);
  ServiceRefPtr service = provider_.GetService(args, &e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_FALSE(service);
}

TEST_F(VPNProviderTest, GetServiceUnsupportedType) {
  KeyValueStore args;
  Error e;
  args.SetString(kTypeProperty, kTypeVPN);
  args.SetString(kProviderTypeProperty, "unknown-vpn-type");
  args.SetString(kProviderHostProperty, kHost);
  args.SetString(kNameProperty, kName);
  ServiceRefPtr service = provider_.GetService(args, &e);
  EXPECT_EQ(Error::kNotSupported, e.type());
  EXPECT_FALSE(service);
}

TEST_F(VPNProviderTest, GetService) {
  KeyValueStore args;
  args.SetString(kTypeProperty, kTypeVPN);
  args.SetString(kProviderTypeProperty, kProviderOpenVpn);
  args.SetString(kProviderHostProperty, kHost);
  args.SetString(kNameProperty, kName);

  {
    Error error;
    ServiceRefPtr service = provider_.FindSimilarService(args, &error);
    EXPECT_EQ(Error::kNotFound, error.type());
    EXPECT_EQ(nullptr, service.get());
  }

  EXPECT_EQ(0, GetServiceCount());

  ServiceRefPtr service;
  {
    Error error;
    EXPECT_CALL(manager_, device_info()).WillOnce(Return(&device_info_));
    EXPECT_CALL(manager_, RegisterService(_));
    service = provider_.GetService(args, &error);
    EXPECT_TRUE(error.IsSuccess());
    ASSERT_TRUE(service);
    testing::Mock::VerifyAndClearExpectations(&manager_);
  }

  EXPECT_EQ("vpn_10_8_0_1_vpn_name", service->GetStorageIdentifier());
  EXPECT_EQ(kName, GetServiceFriendlyName(service));

  EXPECT_EQ(1, GetServiceCount());

  // Configure the service to set its properties (including Provider.Host).
  {
    Error error;
    service->Configure(args, &error);
    EXPECT_TRUE(error.IsSuccess());
  }

  // None of the calls below should cause a new service to be registered.
  EXPECT_CALL(manager_, RegisterService(_)).Times(0);

  // A second call should return the same service.
  {
    Error error;
    ServiceRefPtr get_service = provider_.GetService(args, &error);
    EXPECT_TRUE(error.IsSuccess());
    ASSERT_EQ(service, get_service);
  }

  EXPECT_EQ(1, GetServiceCount());

  // FindSimilarService should also return this service.
  {
    Error error;
    ServiceRefPtr similar_service = provider_.FindSimilarService(args, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(service, similar_service);
  }

  EXPECT_EQ(1, GetServiceCount());

  // However, CreateTemporaryService should create a different service.
  {
    Error error;
    ServiceRefPtr temporary_service =
        provider_.CreateTemporaryService(args, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_NE(service, temporary_service);

    // However this service will not be part of the provider.
    EXPECT_EQ(1, GetServiceCount());
  }
}

TEST_F(VPNProviderTest, OnDeviceInfoAvailable) {
  const string kInterfaceName("tun0");
  const int kInterfaceIndex = 1;

  unique_ptr<MockVPNDriver> bad_driver(new MockVPNDriver());
  EXPECT_CALL(*bad_driver.get(), ClaimInterface(_, _))
      .Times(2)
      .WillRepeatedly(Return(false));
  provider_.services_.push_back(new VPNService(&control_, nullptr, &metrics_,
                                               nullptr, bad_driver.release()));

  EXPECT_FALSE(provider_.OnDeviceInfoAvailable(kInterfaceName,
                                               kInterfaceIndex));

  unique_ptr<MockVPNDriver> good_driver(new MockVPNDriver());
  EXPECT_CALL(*good_driver.get(), ClaimInterface(_, _))
      .WillOnce(Return(true));
  provider_.services_.push_back(new VPNService(&control_, nullptr, &metrics_,
                                               nullptr, good_driver.release()));

  unique_ptr<MockVPNDriver> dup_driver(new MockVPNDriver());
  EXPECT_CALL(*dup_driver.get(), ClaimInterface(_, _))
      .Times(0);
  provider_.services_.push_back(new VPNService(&control_, nullptr, &metrics_,
                                               nullptr, dup_driver.release()));

  EXPECT_TRUE(provider_.OnDeviceInfoAvailable(kInterfaceName, kInterfaceIndex));
  provider_.services_.clear();
}

TEST_F(VPNProviderTest, RemoveService) {
  scoped_refptr<MockVPNService> service0(
      new MockVPNService(&control_, nullptr, &metrics_, nullptr, nullptr));
  scoped_refptr<MockVPNService> service1(
      new MockVPNService(&control_, nullptr, &metrics_, nullptr, nullptr));
  scoped_refptr<MockVPNService> service2(
      new MockVPNService(&control_, nullptr, &metrics_, nullptr, nullptr));

  provider_.services_.push_back(service0.get());
  provider_.services_.push_back(service1.get());
  provider_.services_.push_back(service2.get());

  ASSERT_EQ(3, provider_.services_.size());

  provider_.RemoveService(service1);

  EXPECT_EQ(2, provider_.services_.size());
  EXPECT_EQ(service0, provider_.services_[0]);
  EXPECT_EQ(service2, provider_.services_[1]);

  provider_.RemoveService(service2);

  EXPECT_EQ(1, provider_.services_.size());
  EXPECT_EQ(service0, provider_.services_[0]);

  provider_.RemoveService(service0);
  EXPECT_EQ(0, provider_.services_.size());
}

MATCHER_P(ServiceWithStorageId, storage_id, "") {
  return arg->GetStorageIdentifier() == storage_id;
}

TEST_F(VPNProviderTest, CreateServicesFromProfile) {
  scoped_refptr<MockProfile> profile(
      new NiceMock<MockProfile>(&control_, &metrics_, &manager_, ""));
  NiceMock<MockStore> storage;
  EXPECT_CALL(*profile, GetConstStorage()).WillRepeatedly(Return(&storage));
  EXPECT_CALL(storage, GetString(_, _, _)).WillRepeatedly(Return(false));

  std::set<string> groups;

  const string kNonVPNIdentifier("foo_1");
  groups.insert(kNonVPNIdentifier);

  const string kVPNIdentifierNoProvider("vpn_no_provider");
  groups.insert(kVPNIdentifierNoProvider);

  const string kVPNIdentifierNoName("vpn_no_name");
  groups.insert(kVPNIdentifierNoName);
  const string kOpenVPNProvider(kProviderOpenVpn);
  EXPECT_CALL(storage,
              GetString(kVPNIdentifierNoName, kProviderTypeProperty, _))
      .WillRepeatedly(
           DoAll(SetArgumentPointee<2>(kOpenVPNProvider), Return(true)));

  const string kVPNIdentifierNoHost("vpn_no_host");
  groups.insert(kVPNIdentifierNoHost);
  EXPECT_CALL(storage,
              GetString(kVPNIdentifierNoHost, kProviderTypeProperty, _))
      .WillRepeatedly(
           DoAll(SetArgumentPointee<2>(kOpenVPNProvider), Return(true)));
  const string kName("name");
  EXPECT_CALL(storage, GetString(kVPNIdentifierNoHost, kNameProperty, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kName), Return(true)));

  const string kVPNIdentifierValid("vpn_valid");
  groups.insert(kVPNIdentifierValid);
  EXPECT_CALL(storage, GetString(kVPNIdentifierValid, kProviderTypeProperty, _))
      .WillRepeatedly(
           DoAll(SetArgumentPointee<2>(kOpenVPNProvider), Return(true)));
  EXPECT_CALL(storage, GetString(kVPNIdentifierValid, kNameProperty, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kName), Return(true)));
  const string kHost("1.2.3.4");
  EXPECT_CALL(storage, GetString(kVPNIdentifierValid, kProviderHostProperty, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(kHost), Return(true)));

  EXPECT_CALL(storage, GetGroupsWithKey(kProviderTypeProperty))
      .WillRepeatedly(Return(groups));

  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(manager_,
              RegisterService(ServiceWithStorageId(kVPNIdentifierValid)));
  EXPECT_CALL(*profile,
              ConfigureService(ServiceWithStorageId(kVPNIdentifierValid)))
      .WillOnce(Return(true));
  provider_.CreateServicesFromProfile(profile);

  GetServiceAt(0)->driver()->args()->SetString(kProviderHostProperty, kHost);
  // Calling this again should not create any more services (checked by the
  // Times(1) above).
  provider_.CreateServicesFromProfile(profile);
}

TEST_F(VPNProviderTest, CreateService) {
  static const char kName[] = "test-vpn-service";
  static const char kStorageID[] = "test_vpn_storage_id";
  static const char *kTypes[] = {
    kProviderOpenVpn,
    kProviderL2tpIpsec,
    kProviderThirdPartyVpn
  };
  const size_t kTypesCount = arraysize(kTypes);
  EXPECT_CALL(manager_, device_info())
      .Times(kTypesCount)
      .WillRepeatedly(Return(&device_info_));
  EXPECT_CALL(manager_, RegisterService(_)).Times(kTypesCount);
  for (size_t i = 0; i < kTypesCount; i++) {
    Error error;
    VPNServiceRefPtr service =
        provider_.CreateService(kTypes[i], kName, kStorageID, &error);
    ASSERT_TRUE(service) << kTypes[i];
    ASSERT_TRUE(service->driver()) << kTypes[i];
    EXPECT_EQ(kTypes[i], service->driver()->GetProviderType());
    EXPECT_EQ(kName, GetServiceFriendlyName(service)) << kTypes[i];
    EXPECT_EQ(kStorageID, service->GetStorageIdentifier()) << kTypes[i];
    EXPECT_TRUE(error.IsSuccess()) << kTypes[i];
  }
  Error error;
  VPNServiceRefPtr unknown_service =
      provider_.CreateService("unknown-vpn-type", kName, kStorageID, &error);
  EXPECT_FALSE(unknown_service);
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(VPNProviderTest, HasActiveService) {
  EXPECT_FALSE(provider_.HasActiveService());

  scoped_refptr<MockVPNService> service0(
      new MockVPNService(&control_, nullptr, &metrics_, nullptr, nullptr));
  scoped_refptr<MockVPNService> service1(
      new MockVPNService(&control_, nullptr, &metrics_, nullptr, nullptr));
  scoped_refptr<MockVPNService> service2(
      new MockVPNService(&control_, nullptr, &metrics_, nullptr, nullptr));

  AddService(service0);
  AddService(service1);
  AddService(service2);
  EXPECT_FALSE(provider_.HasActiveService());

  SetConnectState(service1, Service::kStateAssociating);
  EXPECT_TRUE(provider_.HasActiveService());

  SetConnectState(service1, Service::kStateOnline);
  EXPECT_TRUE(provider_.HasActiveService());
}

}  // namespace shill