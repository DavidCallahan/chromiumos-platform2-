// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_DAEMON_DELEGATE_H_
#define POWER_MANAGER_POWERD_DAEMON_DELEGATE_H_

#include <sys/types.h>

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>

namespace power_manager {

namespace policy {
class BacklightController;
}  // namespace policy

namespace system {
class AcpiWakeupHelperInterface;
class AmbientLightSensorInterface;
class AudioClientInterface;
class BacklightInterface;
class DarkResumeInterface;
class DBusWrapperInterface;
class DisplayPowerSetterInterface;
class DisplayWatcherInterface;
class EcWakeupHelperInterface;
class InputWatcherInterface;
class PeripheralBatteryWatcher;
class PowerSupplyInterface;
class UdevInterface;
}  // namespace system

class MetricsSenderInterface;
class PrefsInterface;

// Delegate class implementing functionality on behalf of the Daemon class.
// Create*() methods perform any necessary initialization of the returned
// objects.
class DaemonDelegate {
 public:
  DaemonDelegate() {}
  virtual ~DaemonDelegate() {}

  // Crashes if prefs can't be loaded (e.g. due to a missing directory).
  virtual std::unique_ptr<PrefsInterface> CreatePrefs() = 0;

  // Crashes if the connection to the system bus fails.
  virtual std::unique_ptr<system::DBusWrapperInterface> CreateDBusWrapper() = 0;

  // Crashes if udev initialization fails.
  virtual std::unique_ptr<system::UdevInterface> CreateUdev() = 0;

  virtual std::unique_ptr<system::AmbientLightSensorInterface>
  CreateAmbientLightSensor() = 0;

  virtual std::unique_ptr<system::DisplayWatcherInterface> CreateDisplayWatcher(
      system::UdevInterface* udev) = 0;

  virtual std::unique_ptr<system::DisplayPowerSetterInterface>
  CreateDisplayPowerSetter(system::DBusWrapperInterface* dbus_wrapper) = 0;

  virtual std::unique_ptr<policy::BacklightController>
  CreateExternalBacklightController(
      system::DisplayWatcherInterface* display_watcher,
      system::DisplayPowerSetterInterface* display_power_setter) = 0;

  // Returns null if the backlight couldn't be initialized.
  virtual std::unique_ptr<system::BacklightInterface> CreateInternalBacklight(
      const base::FilePath& base_path,
      const base::FilePath::StringType& pattern) = 0;

  virtual std::unique_ptr<policy::BacklightController>
  CreateInternalBacklightController(
      system::BacklightInterface* backlight,
      PrefsInterface* prefs,
      system::AmbientLightSensorInterface* sensor,
      system::DisplayPowerSetterInterface* power_setter) = 0;

  virtual std::unique_ptr<policy::BacklightController>
  CreateKeyboardBacklightController(
      system::BacklightInterface* backlight,
      PrefsInterface* prefs,
      system::AmbientLightSensorInterface* sensor,
      policy::BacklightController* display_backlight_controller,
      TabletMode initial_tablet_mode) = 0;

  virtual std::unique_ptr<system::InputWatcherInterface> CreateInputWatcher(
      PrefsInterface* prefs,
      system::UdevInterface* udev) = 0;

  virtual std::unique_ptr<system::AcpiWakeupHelperInterface>
  CreateAcpiWakeupHelper() = 0;

  virtual std::unique_ptr<system::EcWakeupHelperInterface>
  CreateEcWakeupHelper() = 0;

  // Test implementations may return null.
  virtual std::unique_ptr<system::PeripheralBatteryWatcher>
  CreatePeripheralBatteryWatcher(
      system::DBusWrapperInterface* dbus_wrapper) = 0;

  virtual std::unique_ptr<system::PowerSupplyInterface> CreatePowerSupply(
      const base::FilePath& power_supply_path,
      PrefsInterface* prefs,
      system::UdevInterface* udev) = 0;

  virtual std::unique_ptr<system::DarkResumeInterface> CreateDarkResume(
      system::PowerSupplyInterface* power_supply,
      PrefsInterface* prefs) = 0;

  virtual std::unique_ptr<system::AudioClientInterface> CreateAudioClient(
      system::DBusWrapperInterface* dbus_wrapper) = 0;

  virtual std::unique_ptr<MetricsSenderInterface> CreateMetricsSender() = 0;

  // Returns the process's PID.
  virtual pid_t GetPid() = 0;

  // Runs |command| asynchronously.
  virtual void Launch(const std::string& command) = 0;

  // Runs |command| synchronously.  The process's exit code is returned.
  virtual int Run(const std::string& command) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DaemonDelegate);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_DAEMON_DELEGATE_H_
