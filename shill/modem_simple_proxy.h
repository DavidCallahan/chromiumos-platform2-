// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_SIMPLE_PROXY_H_
#define SHILL_MODEM_SIMPLE_PROXY_H_

#include <string>

#include <base/macros.h>

#include "dbus_proxies/org.freedesktop.ModemManager.Modem.Simple.h"
#include "shill/modem_simple_proxy_interface.h"

namespace shill {

// A proxy to (old) ModemManager.Modem.Simple.
class ModemSimpleProxy : public ModemSimpleProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Simple DBus object proxy at
  // |path| owned by |service|.
  ModemSimpleProxy(DBus::Connection *connection,
                   const std::string &path,
                   const std::string &service);
  ~ModemSimpleProxy() override;

  // Inherited from ModemSimpleProxyInterface.
  virtual void GetModemStatus(Error *error,
                              const DBusPropertyMapCallback &callback,
                              int timeout);
  virtual void Connect(const DBusPropertiesMap &properties,
                       Error *error,
                       const ResultCallback &callback,
                       int timeout);

 private:
  class Proxy : public org::freedesktop::ModemManager::Modem::Simple_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    ~Proxy() override;

   private:
    // Signal callbacks inherited from ModemManager::Modem::Simple_proxy.
    // [None]

    // Method callbacks inherited from ModemManager::Modem::Simple_proxy.
    virtual void GetStatusCallback(const DBusPropertiesMap &props,
                                   const DBus::Error &dberror, void *data);
    virtual void ConnectCallback(const DBus::Error &dberror, void *data);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemSimpleProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_SIMPLE_PROXY_H_