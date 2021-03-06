// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_AP_MANAGER_CLIENT_H_
#define BUFFET_AP_MANAGER_CLIENT_H_

#include <memory>
#include <string>

#include <apmanager/dbus-proxies.h>
#include <base/callback.h>
#include <base/memory/ref_counted.h>

namespace buffet {

// Manages soft AP for wifi bootstrapping.
// Once created can handle multiple Start/Stop requests.
class ApManagerClient final {
 public:
  explicit ApManagerClient(const scoped_refptr<dbus::Bus>& bus);
  ~ApManagerClient();

  void Start(const std::string& ssid);
  void Stop();

  std::string GetSsid() const { return ssid_; }

 private:
  void RemoveService(const dbus::ObjectPath& object_path);

  void OnManagerAdded(
      org::chromium::apmanager::ManagerProxyInterface* manager_proxy);
  void OnServiceAdded(
      org::chromium::apmanager::ServiceProxyInterface* service_proxy);

  void OnSsidSet(bool success);

  void OnServiceRemoved(const dbus::ObjectPath& object_path);
  void OnManagerRemoved(const dbus::ObjectPath& object_path);

  scoped_refptr<dbus::Bus> bus_;

  std::unique_ptr<org::chromium::apmanager::ObjectManagerProxy>
      object_manager_proxy_;
  org::chromium::apmanager::ManagerProxyInterface* manager_proxy_{nullptr};

  dbus::ObjectPath service_path_;
  org::chromium::apmanager::ServiceProxyInterface* service_proxy_{nullptr};

  std::string ssid_;

  base::WeakPtrFactory<ApManagerClient> weak_ptr_factory_{this};
};

}  // namespace buffet

#endif  // BUFFET_AP_MANAGER_CLIENT_H_
