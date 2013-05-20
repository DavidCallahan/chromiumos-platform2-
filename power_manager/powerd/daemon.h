// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_DAEMON_H_
#define POWER_MANAGER_POWERD_DAEMON_H_
#pragma once

#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <ctime>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "power_manager/common/dbus_handler.h"
#include "power_manager/common/inotify.h"
#include "power_manager/common/prefs_observer.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/file_tagger.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/policy/backlight_controller_observer.h"
#include "power_manager/powerd/policy/dark_resume_policy.h"
#include "power_manager/powerd/policy/input_controller.h"
#include "power_manager/powerd/policy/keyboard_backlight_controller.h"
#include "power_manager/powerd/policy/suspender.h"
#include "power_manager/powerd/system/audio_observer.h"
#include "power_manager/powerd/system/peripheral_battery_watcher.h"
#include "power_manager/powerd/system/power_supply.h"
#include "power_manager/powerd/system/power_supply_observer.h"

// Forward declarations of structs from libudev.h.
struct udev;
struct udev_monitor;

class MetricsLibraryInterface;

namespace power_manager {

class DBusSenderInterface;
class MetricsReporter;
class Prefs;

namespace policy {
class StateController;
}  // namespace policy

namespace system {
class AudioClient;
class Input;
}  // namespace system

enum PluggedState {
  PLUGGED_STATE_DISCONNECTED,
  PLUGGED_STATE_CONNECTED,
  PLUGGED_STATE_UNKNOWN,
};

// Main class within the powerd daemon that ties all other classes together.
class Daemon : public policy::BacklightControllerObserver,
               public policy::InputController::Delegate,
               public system::AudioObserver,
               public system::PowerSupplyObserver {
 public:
  // Ownership of pointers remains with the caller.  |backlight_controller|
  // and |keyboard_controller| may be NULL.
  Daemon(PrefsInterface* prefs,
         MetricsLibraryInterface* metrics_lib,
         policy::BacklightController* backlight_controller,
         policy::KeyboardBacklightController* keyboard_controller,
         const base::FilePath& run_dir);
  ~Daemon();

  void Init();
  void Run();
  void SetPlugged(bool plugged);

  void OnRequestRestart();
  void OnRequestShutdown();

  // Shuts the system down in response to a failed suspend attempt.
  void ShutdownForFailedSuspend();

  // Notify chrome that an idle event happened.
  void IdleEventNotify(int64 threshold);

  // Overridden from policy::BacklightControllerObserver:
  virtual void OnBrightnessChanged(
      double brightness_percent,
      policy::BacklightController::BrightnessChangeCause cause,
      policy::BacklightController* source) OVERRIDE;

  // Called by |suspender_| before other processes are informed that the
  // system will be suspending soon.
  void PrepareForSuspendAnnouncement();

  // Called by |suspender_| if a suspend request is aborted before
  // PrepareForSuspend() has been called.
  void HandleCanceledSuspendAnnouncement();

  // Called by |suspender_| just before a suspend attempt begins.
  void PrepareForSuspend();

  // Called by |suspender_| after the completion of a suspend/resume cycle
  // (which did not necessarily succeed).
  void HandleResume(bool suspend_was_successful,
                    int num_suspend_retries,
                    int max_suspend_retries);

  // Overridden from policy::InputController::Delegate:
  virtual void HandleLidClosed() OVERRIDE;
  virtual void HandleLidOpened() OVERRIDE;
  virtual void SendPowerButtonMetric(bool down, base::TimeTicks timestamp)
      OVERRIDE;

  // Overridden from system::AudioObserver:
  virtual void OnAudioActivity(base::TimeTicks last_audio_time) OVERRIDE;

  // Overridden from system::PowerSupplyObserver:
  virtual void OnPowerStatusUpdate(const system::PowerStatus& status) OVERRIDE;

 private:
  enum ShutdownState {
    SHUTDOWN_STATE_NONE,
    SHUTDOWN_STATE_RESTARTING,
    SHUTDOWN_STATE_POWER_OFF
  };

  class StateControllerDelegate;
  class SuspenderDelegate;

  // Decrease / increase the keyboard brightness; direction should be +1 for
  // increase and -1 for decrease.
  void AdjustKeyboardBrightness(int direction);

  // Shared code between keyboard and screen brightness changed handling
  void SendBrightnessChangedSignal(
      double brightness_percent,
      policy::BacklightController::BrightnessChangeCause cause,
      const std::string& signal_name);

  // Handles power supply udev events.
  static gboolean UdevEventHandler(GIOChannel*,
                                   GIOCondition condition,
                                   gpointer data);

  // Registers udev event handler with GIO.
  void RegisterUdevEventHandler();

  // Registers the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Handles changes to D-Bus name ownership.
  void HandleDBusNameOwnerChanged(const std::string& name,
                                  const std::string& old_owner,
                                  const std::string& new_owner);

  // Callbacks for handling dbus messages.
  bool HandleCleanShutdownSignal(DBusMessage* message);
  bool HandleSessionStateChangedSignal(DBusMessage* message);
  bool HandleUpdateEngineStatusUpdateSignal(DBusMessage* message);
  DBusMessage* HandleRequestShutdownMethod(DBusMessage* message);
  DBusMessage* HandleRequestRestartMethod(DBusMessage* message);
  DBusMessage* HandleRequestSuspendMethod(DBusMessage* message);
  DBusMessage* HandleDecreaseScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleIncreaseScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleGetScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleSetScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleDecreaseKeyboardBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleIncreaseKeyboardBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleRequestIdleNotificationMethod(DBusMessage* message);
  DBusMessage* HandleGetPowerSupplyPropertiesMethod(DBusMessage* message);
  DBusMessage* HandleVideoActivityMethod(DBusMessage* message);
  DBusMessage* HandleUserActivityMethod(DBusMessage* message);
  DBusMessage* HandleSetIsProjectingMethod(DBusMessage* message);
  DBusMessage* HandleSetPolicyMethod(DBusMessage* message);

  // Timeout handler for clean shutdown. If we don't hear back from
  // clean shutdown because the stopping is taking too long or hung,
  // go through with the shutdown now.
  SIGNAL_CALLBACK_0(Daemon, gboolean, CleanShutdownTimedOut);

  // Handles information from the session manager about the session state.
  void OnSessionStateChange(const std::string& state_str);

  void StartCleanShutdown();
  void Shutdown();
  void Suspend();
  void SuspendDisable();
  void SuspendEnable();

  // Updates state in |backlight_controller_| and |keyboard_controller_|
  // (if non-NULL).
  void SetBacklightsDimmedForInactivity(bool dimmed);
  void SetBacklightsOffForInactivity(bool off);
  void SetBacklightsSuspended(bool suspended);
  void SetBacklightsDocked(bool docked);

  scoped_ptr<StateControllerDelegate> state_controller_delegate_;

  policy::BacklightController* backlight_controller_;
  PrefsInterface* prefs_;
  policy::KeyboardBacklightController* keyboard_controller_;  // non-owned

  scoped_ptr<MetricsReporter> metrics_reporter_;
  scoped_ptr<DBusSenderInterface> dbus_sender_;
  scoped_ptr<system::Input> input_;
  scoped_ptr<policy::StateController> state_controller_;
  scoped_ptr<policy::InputController> input_controller_;
  scoped_ptr<system::AudioClient> audio_client_;
  scoped_ptr<system::PeripheralBatteryWatcher> peripheral_battery_watcher_;

  bool clean_shutdown_initiated_;
  bool low_battery_;
  int64 clean_shutdown_timeout_ms_;
  guint clean_shutdown_timeout_id_;
  PluggedState plugged_state_;
  FileTagger file_tagger_;
  ShutdownState shutdown_state_;
  scoped_ptr<system::PowerSupply> power_supply_;
  scoped_ptr<policy::DarkResumePolicy> dark_resume_policy_;
  scoped_ptr<SuspenderDelegate> suspender_delegate_;
  policy::Suspender suspender_;
  base::FilePath run_dir_;
  base::TimeTicks session_start_;
  bool is_power_status_stale_;

  // Last session state that we have been informed of. Initialized as stopped.
  SessionState session_state_;

  // Keep a local copy of power status reading from power_supply.  This way,
  // requests for each field of the power status can be read directly from
  // this struct.  Otherwise we'd have to read the whole struct from
  // power_supply since it doesn't support reading individual fields.
  system::PowerStatus power_status_;

  // For listening to udev events.
  struct udev_monitor* udev_monitor_;
  struct udev* udev_;

  // This is the DBus helper object that dispatches DBus messages to handlers
  util::DBusHandler dbus_handler_;

  // String that indicates reason for shutting down.  See power_constants.cc for
  // valid values.
  std::string shutdown_reason_;

  // Has |state_controller_| been initialized?  Daemon::Init() invokes a
  // bunch of event-handling functions directly, but events shouldn't be
  // passed to |state_controller_| until it's been initialized.
  bool state_controller_initialized_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_DAEMON_H_
