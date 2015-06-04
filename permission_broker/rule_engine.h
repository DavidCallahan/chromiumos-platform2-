// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_RULE_ENGINE_H_
#define PERMISSION_BROKER_RULE_ENGINE_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>

#include "permission_broker/rule.h"

struct udev;

namespace permission_broker {

class RuleEngine {
 public:
  RuleEngine(const std::string& udev_run_path, int poll_interval_msecs);
  virtual ~RuleEngine();

  // Adds |rule| to the end of the existing rule chain. Takes ownership of
  // |rule|.
  void AddRule(Rule* rule);

  // Invokes each of the rules in order on |path| until either a rule explicitly
  // denies access to the path or until there are no more rules left. If, after
  // executing all of the stored rules, no rule has explicitly allowed access to
  // the path then access is denied. If _any_ rule denies access to |path| then
  // processing the rules is aborted early and access is denied.
  Rule::Result ProcessPath(const std::string& path);

 protected:
  // This constructor is for use by test code only.
  RuleEngine();

 private:
  friend class RuleEngineTest;

  // Waits for all queued udev events to complete before returning. Is
  // equivalent to invoking 'udevadm settle', but without the external
  // dependency and overhead.
  virtual void WaitForEmptyUdevQueue();

  struct udev* udev_;
  std::vector<Rule*> rules_;

  int poll_interval_msecs_;
  std::string udev_run_path_;

  DISALLOW_COPY_AND_ASSIGN(RuleEngine);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_RULE_ENGINE_H_
