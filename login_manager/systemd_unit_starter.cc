// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/systemd_unit_starter.h"

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

namespace {

scoped_ptr<dbus::Response> CallEnvironmentMethod(
    dbus::ObjectProxy *proxy,
    const std::string &method_name,
    const std::vector<std::string> &args_keyvals) {
  DCHECK(proxy);
  dbus::MethodCall method_call(login_manager::SystemdUnitStarter::kInterface,
                               method_name);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfStrings(args_keyvals);

  return proxy->CallMethodAndBlock(
             &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
}

scoped_ptr<dbus::Response> SetEnvironment(
    dbus::ObjectProxy *proxy,
    const std::vector<std::string> &args_keyvals) {
  return CallEnvironmentMethod(
             proxy,
             login_manager::SystemdUnitStarter::kSetEnvironmentMethodName,
             args_keyvals);
}
scoped_ptr<dbus::Response> UnsetEnvironment(
     dbus::ObjectProxy *proxy,
     const std::vector<std::string> &args_keyvals) {
  std::vector<std::string> env_vars = args_keyvals;
  std::string::size_type i = std::string::npos;

  // Keep only the keys from environment array
  for (std::vector<std::string>::iterator it=env_vars.begin();
       it != env_vars.end();
       ++it ) {
    i = it->find("=");
    if (i != std::string::npos)
      it->erase(i, it->length());
  }
  return CallEnvironmentMethod(
             proxy,
             login_manager::SystemdUnitStarter::kUnsetEnvironmentMethodName,
              env_vars);
}

}  // namespace

namespace login_manager {

const char SystemdUnitStarter::kServiceName[] = "org.freedesktop.systemd1";
const char SystemdUnitStarter::kPath[] = "/org/freedesktop/systemd1";
const char SystemdUnitStarter::kInterface[] =
    "org.freedesktop.systemd1.Manager";
const char SystemdUnitStarter::kStartUnitMode[] = "replace";
const char SystemdUnitStarter::kStartUnitMethodName[] = "StartUnit";
const char SystemdUnitStarter::kSetEnvironmentMethodName[] = "SetEnvironment";
const char SystemdUnitStarter::kUnsetEnvironmentMethodName[] =
    "UnsetEnvironment";


SystemdUnitStarter::SystemdUnitStarter(dbus::ObjectProxy *proxy)
    : systemd_dbus_proxy_(proxy) {
}

SystemdUnitStarter::~SystemdUnitStarter() {
}

scoped_ptr<dbus::Response> SystemdUnitStarter::StartUnit(
    const std::string &unit_name,
    const std::vector<std::string> &args_keyvals ) {
  DLOG(INFO) << "Starting " << unit_name << " unit";

  scoped_ptr<dbus::Response> response;

  // If we are not able to properly set the environment for the
  // target unit, there is no point in going forward
  if (!SetEnvironment(systemd_dbus_proxy_, args_keyvals)) {
    DLOG(WARNING) << "Could not set environment for " << unit_name;
    return nullptr;
  }
  dbus::MethodCall method_call(SystemdUnitStarter::kInterface,
                               SystemdUnitStarter::kStartUnitMethodName);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(unit_name + ".target");
  writer.AppendString(SystemdUnitStarter::kStartUnitMode);

  response = systemd_dbus_proxy_->CallMethodAndBlock(
             &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);

  if (!UnsetEnvironment(systemd_dbus_proxy_, args_keyvals))
    DLOG(WARNING) << "Unable to unset environment after starting" << unit_name;

  return response;
}

scoped_ptr<dbus::Response> SystemdUnitStarter::TriggerImpulse(
    const std::string &unit_name,
    const std::vector<std::string> &args_keyvals) {
  return StartUnit(unit_name, args_keyvals);
}

}  // namespace login_manager
