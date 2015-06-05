// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_SETTINGS_MAP_H_
#define SETTINGSD_SETTINGS_MAP_H_

#include <base/macros.h>
#include <base/values.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "settingsd/settings_document.h"

namespace settingsd {

// Interface for accessing configuration values.
class SettingsMap {
 public:
  virtual ~SettingsMap() {}

  // Clears the settings map.
  virtual void Clear() = 0;

  // Retrieves the currently active value for the setting identified by |key|.
  // If no such setting is currently set, this method returns a nullptr.
  virtual const base::Value* GetValue(const Key& key) const = 0;

  // Returns the list of currently active settings whose keys have |key| as an
  // ancestor.
  virtual std::set<Key> GetKeys(const Key& prefix) const = 0;

  // Inserts a settings document into the settings map. Returns true if the
  // insertion is successful. If the document insertion fails due to a collision
  // with a previously inserted document, returns false. In this case
  // SettingsMap is unchanged. If the out parameter |modified_keys| is not the
  // |nullptr|, keys that have been added or deleted by the insertion are
  // inserted into the set. Note that this only includes all keys for which the
  // return value of GetValue() has changed. If the out parameter
  // |unreferenced_documents| is not the |nullptr|, |unreferenced_documents|
  // will be replaced with a list of documents that are now unreferenced due
  // to the insertion.
  virtual bool InsertDocument(
      const SettingsDocument* document,
      std::set<Key>* modified_keys,
      std::vector<const SettingsDocument*>* unreferenced_documents) = 0;

  // Removes a settings document from the settings map and deletes it. The
  // attempt to remove a document that not currently not contained in
  // SettingsMap is a noop. If the out parameter |modified_keys| is not the
  // |nullptr|, keys that have been added or deleted by the deletion are
  // inserted into the set. Note that this only includes all keys for which the
  // return value of GetValue() has changed. If the out parameter
  // |unreferenced_documents| is not the |nullptr|, |unreferenced_documents|
  // will be replaced with a list of documents that are now unreferenced due
  // to the insertion.
  virtual void RemoveDocument(
      const SettingsDocument* document,
      std::set<Key>* modified_keys,
      std::vector<const SettingsDocument*>* unreferenced_documents) = 0;

 private:
  DISALLOW_ASSIGN(SettingsMap);
};

}  // namespace settingsd

#endif  // SETTINGSD_SETTINGS_MAP_H_