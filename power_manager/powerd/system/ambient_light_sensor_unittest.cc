// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ambient_light_sensor.h"

#include <memory>

#include <base/compiler_specific.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_number_conversions.h>
#include <gtest/gtest.h>

#include "power_manager/common/test_main_loop_runner.h"
#include "power_manager/powerd/system/ambient_light_observer.h"

namespace power_manager {
namespace system {

namespace {

// Abort if it an expected brightness change hasn't been received after this
// many milliseconds.
const int kUpdateTimeoutMs = 5000;

// Frequency with which the ambient light sensor file is polled.
const int kPollIntervalMs = 100;

// Simple AmbientLightObserver implementation that runs the event loop
// until it receives notification that the ambient light level has changed.
class TestObserver : public AmbientLightObserver {
 public:
  TestObserver() {}
  virtual ~TestObserver() {}

  // Runs |loop_| until OnAmbientLightUpdated() is called.
  bool RunUntilAmbientLightUpdated() {
    return loop_runner_.StartLoop(
        base::TimeDelta::FromMilliseconds(kUpdateTimeoutMs));
  }

  // AmbientLightObserver implementation:
  void OnAmbientLightUpdated(AmbientLightSensorInterface* sensor) override {
    loop_runner_.StopLoop();
  }

 private:
  TestMainLoopRunner loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class AmbientLightSensorTest : public ::testing::Test {
 public:
  AmbientLightSensorTest() {}
  virtual ~AmbientLightSensorTest() {}

  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath device_dir = temp_dir_.path().Append("device0");
    CHECK(base::CreateDirectory(device_dir));
    data_file_ = device_dir.Append("illuminance0_input");
    sensor_.reset(new AmbientLightSensor);
    sensor_->set_device_list_path_for_testing(temp_dir_.path());
    sensor_->set_poll_interval_ms_for_testing(kPollIntervalMs);
    sensor_->AddObserver(&observer_);
    sensor_->Init();
  }

  void TearDown() override {
    sensor_->RemoveObserver(&observer_);
  }

 protected:
  // Writes |lux| to |data_file_| to simulate the ambient light sensor reporting
  // a new light level.
  void WriteLux(int lux) {
    std::string lux_string = base::IntToString(lux);
    int bytes_written =
        base::WriteFile(data_file_, lux_string.data(), lux_string.size());
    CHECK(bytes_written == static_cast<int>(lux_string.size()))
        << "Wrote " << bytes_written << " byte(s) instead of "
        << lux_string.size() << " to " << data_file_.value();
  }

  // Temporary directory mimicking a /sys directory containing a set of sensor
  // devices.
  base::ScopedTempDir temp_dir_;

  // Illuminance file containing the sensor's current brightness level.
  base::FilePath data_file_;

  TestObserver observer_;

  std::unique_ptr<AmbientLightSensor> sensor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensorTest);
};

TEST_F(AmbientLightSensorTest, Basic) {
  WriteLux(100);
  ASSERT_TRUE(observer_.RunUntilAmbientLightUpdated());
  EXPECT_EQ(100, sensor_->GetAmbientLightLux());

  WriteLux(200);
  ASSERT_TRUE(observer_.RunUntilAmbientLightUpdated());
  EXPECT_EQ(200, sensor_->GetAmbientLightLux());

  // When the lux value doesn't change, we should still be called.
  WriteLux(200);
  ASSERT_TRUE(observer_.RunUntilAmbientLightUpdated());
  EXPECT_EQ(200, sensor_->GetAmbientLightLux());
}

TEST_F(AmbientLightSensorTest, GiveUpAfterTooManyFailures) {
  // Test that the timer is eventually stopped after many failures.
  base::DeleteFile(data_file_, false);
  for (int i = 0; i < AmbientLightSensor::kNumInitAttemptsBeforeGivingUp; ++i) {
    EXPECT_TRUE(sensor_->TriggerPollTimerForTesting());
    EXPECT_LT(sensor_->GetAmbientLightLux(), 0);
  }

  EXPECT_FALSE(sensor_->TriggerPollTimerForTesting());
  EXPECT_LT(sensor_->GetAmbientLightLux(), 0);
}

}  // namespace system
}  // namespace power_manager
