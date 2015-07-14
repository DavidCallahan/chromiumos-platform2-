// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_SERVICE_OBSERVER_H_
#define PSYCHE_PSYCHED_SERVICE_OBSERVER_H_

namespace psyche {

class ServiceInterface;

// Interface for observing changes to a ServiceInterface object.
class ServiceObserver {
 public:
  virtual ~ServiceObserver() = default;

  // Called when the observed service's proxy has been set (in response to the
  // service being registered) or cleared (in response to a binder death
  // notification).
  virtual void OnServiceProxyChange(ServiceInterface* service) = 0;
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_SERVICE_OBSERVER_H_