// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_GSM_CARD_PROXY_H_
#define SHILL_MODEM_GSM_CARD_PROXY_H_

#include <string>

#include <base/macros.h>

#include "dbus_proxies/org.freedesktop.ModemManager.Modem.Gsm.Card.h"
#include "shill/modem_gsm_card_proxy_interface.h"

namespace shill {

// A proxy to (old) ModemManager.Modem.Gsm.Card.
class ModemGSMCardProxy : public ModemGSMCardProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Gsm.Card DBus
  // object proxy at |path| owned by |service|.
  ModemGSMCardProxy(DBus::Connection *connection,
                    const std::string &path,
                    const std::string &service);
  ~ModemGSMCardProxy() override;

  // Inherited from ModemGSMCardProxyInterface.
  virtual void GetIMEI(Error *error, const GSMIdentifierCallback &callback,
                       int timeout);
  virtual void GetIMSI(Error *error, const GSMIdentifierCallback &callback,
                       int timeout);
  virtual void GetSPN(Error *error, const GSMIdentifierCallback &callback,
                      int timeout);
  virtual void GetMSISDN(Error *error, const GSMIdentifierCallback &callback,
                         int timeout);
  virtual void EnablePIN(const std::string &pin, bool enabled,
                         Error *error, const ResultCallback &callback,
                         int timeout);
  virtual void SendPIN(const std::string &pin,
                       Error *error, const ResultCallback &callback,
                       int timeout);
  virtual void SendPUK(const std::string &puk, const std::string &pin,
                       Error *error, const ResultCallback &callback,
                       int timeout);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error, const ResultCallback &callback,
                         int timeout);
  virtual uint32_t EnabledFacilityLocks();

 private:
  class Proxy
      : public org::freedesktop::ModemManager::Modem::Gsm::Card_proxy,
        public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    ~Proxy() override;

   private:
    // Signal callbacks inherited from ModemManager::Modem::Gsm::Card_proxy.
    // [None]

    // Method callbacks inherited from ModemManager::Modem::Gsm::Card_proxy.
    virtual void GetImeiCallback(const std::string &imei,
                                 const DBus::Error &dberror, void *data);
    virtual void GetImsiCallback(const std::string &imsi,
                                 const DBus::Error &dberror, void *data);
    virtual void GetSpnCallback(const std::string &spn,
                                 const DBus::Error &dberror, void *data);
    virtual void GetMsIsdnCallback(const std::string &msisdn,
                                 const DBus::Error &dberror, void *data);
    virtual void EnablePinCallback(const DBus::Error &dberror, void *data);
    virtual void SendPinCallback(const DBus::Error &dberror, void *data);
    virtual void SendPukCallback(const DBus::Error &dberror, void *data);
    virtual void ChangePinCallback(const DBus::Error &dberror, void *data);

    virtual void GetIdCallback(const std::string &id,
                               const DBus::Error &dberror, void *data);
    virtual void PinCallback(const DBus::Error &dberror, void *data);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  template<typename TraceMsgT, typename CallT, typename CallbackT,
      typename... ArgTypes>
      void BeginCall(
          const TraceMsgT &trace_msg, const CallT &call,
          const CallbackT &callback, Error *error, int timeout,
          ArgTypes... rest);

  DISALLOW_COPY_AND_ASSIGN(ModemGSMCardProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_GSM_CARD_PROXY_H_