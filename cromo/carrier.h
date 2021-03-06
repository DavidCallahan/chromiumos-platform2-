// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_CARRIER_H_
#define CROMO_CARRIER_H_

#include <map>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

#include "cromo/modem.h"
#include "cromo/utilities.h"

typedef unsigned long carrier_id_t;  // NOLINT(runtime/int)

class Carrier {
 public:
  enum ActivationMethod {
    kOmadm,
    kOtasp,
    kNone,
  };

  Carrier(const char* name,
          const char* firmware_directory,
          carrier_id_t carrier_id,
          int carrier_type,
          ActivationMethod activation_method,
          const char* activation_code)
      : name_(name),
        firmware_directory_(firmware_directory),
        carrier_id_(carrier_id),
        carrier_type_(carrier_type),
        activation_method_(activation_method),
        activation_code_(activation_code) {}

  virtual ~Carrier() {}

  // Called after Modem.Simple.Status has filled the property map, but
  // before the property map has been returned.
  virtual void ModifyModemStatusReturn(
      utilities::DBusPropertyMap* properties) const {}

  // Carrier-specific activation code can run here; returns true if
  // this code completely consumed the activation event, false if
  // activation should proceed as normal.  immediate_return gives an
  // enum from MM_MODEM_CDMA_ACTIVATION_ERROR which is meaningful only
  // if true is returned.
  virtual bool CdmaCarrierSpecificActivate(
      const utilities::DBusPropertyMap& status,  // Result of
                                                 // Modem.Simple.GetStatus()
      Modem* modem,
      uint32_t* immediate_return) const {
    return false;
  }

  const char* name() const { return name_; }
  const char* firmware_directory() const { return firmware_directory_; }
  carrier_id_t carrier_id() const {
    return carrier_id_;
  }
  int carrier_type() const { return carrier_type_; }
  ActivationMethod activation_method() const { return activation_method_; }
  const char* activation_code() const { return activation_code_; }

 protected:
  const char* name_;
  const char* firmware_directory_;
  carrier_id_t carrier_id_;
  int carrier_type_;
  ActivationMethod activation_method_;
  const char* activation_code_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Carrier);
};

class CromoServer;
void AddBaselineCarriers(CromoServer* server);

#endif  // CROMO_CARRIER_H_
