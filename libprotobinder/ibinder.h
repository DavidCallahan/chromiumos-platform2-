// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLOBINDER_IBINDER_H_
#define LIBBRILLOBINDER_IBINDER_H_

#define BINDER_EXPORT __attribute__((visibility("default")))

#include <stdint.h>
#include <sys/types.h>

#include <linux/android/binder.h>

namespace brillobinder {

class Parcel;
class BinderHost;
class BinderProxy;

// Wraps are 'binder' endpoint.
// Can be the local or remote side.
class BINDER_EXPORT IBinder {
 public:
  enum {
    FIRST_CALL_TRANSACTION = 0x00000001,
    LAST_CALL_TRANSACTION = 0x00ffffff,

    // These are used by Android
    PING_TRANSACTION = B_PACK_CHARS('_', 'P', 'N', 'G'),
    DUMP_TRANSACTION = B_PACK_CHARS('_', 'D', 'M', 'P'),
    INTERFACE_TRANSACTION = B_PACK_CHARS('_', 'N', 'T', 'F'),
    SYSPROPS_TRANSACTION = B_PACK_CHARS('_', 'S', 'P', 'R'),

    FLAG_ONEWAY = 0x00000001
  };
  IBinder();
  virtual ~IBinder();

  virtual int Transact(uint32_t code,
                       Parcel& data,
                       Parcel* reply,
                       uint32_t flags) = 0;

  virtual BinderHost* GetBinderHost();
  virtual BinderProxy* GetBinderProxy();
};
}
#endif