// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/power_manager_dbus_proxy.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <google/protobuf/message_lite.h>

#include "power_manager/suspend.pb.h"
#include "wimax_manager/power_manager.h"

using std::string;
using std::vector;

namespace wimax_manager {

PowerManagerDBusProxy::PowerManagerDBusProxy(DBus::Connection *connection,
                                             PowerManager *power_manager)
    : DBusProxy(connection,
                power_manager::kPowerManagerServiceName,
                power_manager::kPowerManagerServicePath),
      power_manager_(power_manager) {
  CHECK(power_manager_);
}

PowerManagerDBusProxy::~PowerManagerDBusProxy() {
}

void PowerManagerDBusProxy::SuspendImminent(
    const vector<uint8> &serialized_proto) {
  power_manager_->OnSuspendImminent(serialized_proto);
}

void PowerManagerDBusProxy::PowerStateChanged(const string &new_power_state) {
  power_manager_->OnPowerStateChanged(new_power_state);
}

}  // namespace wimax_manager
