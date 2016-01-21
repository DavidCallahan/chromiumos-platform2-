// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/display/external_display.h"

#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

#include "power_manager/common/metrics_constants.h"
#include "power_manager/common/metrics_sender.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

// Maximum length permitted for an individual DDC message (see DDC/CI v1.1
// 6.5.1).
// TODO(derat): I2C permits messages up to 131 bytes, and DDC/CI messages can
// apparently be fragmented if they exceed 32 bytes. Might need to figure out
// how to support this (can it all be read in one go?) if longer messages are
// needed at some point.
const uint8_t kDdcMaxMessageLength = 32;

// Returns a hexadecimal string representation of |byte|.
std::string ByteToHex(uint8_t byte) {
  return "0x" + base::HexEncode(&byte, 1);
}

}  // namespace

const uint8_t ExternalDisplay::kDdcI2CAddress = 0x37;
const uint8_t ExternalDisplay::kDdcHostAddress = 0x51;
const uint8_t ExternalDisplay::kDdcDisplayAddress = 0x6e;
const uint8_t ExternalDisplay::kDdcVirtualHostAddress = 0x50;
const uint8_t ExternalDisplay::kDdcMessageBodyLengthMask = 0x80;
const uint8_t ExternalDisplay::kDdcGetCommand = 0x01;
const uint8_t ExternalDisplay::kDdcGetReplyCommand = 0x02;
const uint8_t ExternalDisplay::kDdcSetCommand = 0x03;
const uint8_t ExternalDisplay::kDdcBrightnessIndex = 0x10;
const int ExternalDisplay::kDdcSetDelayMs = 50;
const int ExternalDisplay::kDdcGetDelayMs = 40;
const int ExternalDisplay::kCachedBrightnessValidMs = 3000;

ExternalDisplay::RealDelegate::RealDelegate() : fd_(-1) {}

ExternalDisplay::RealDelegate::~RealDelegate() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

bool ExternalDisplay::RealDelegate::Init(const base::FilePath& i2c_path) {
  name_ = i2c_path.BaseName().value();
  fd_ = open(i2c_path.value().c_str(), O_RDWR);
  if (fd_ < 0) {
    PLOG(ERROR) << "Unable to open " << i2c_path.value();
    OpenResult result = OPEN_FAILURE_UNKNOWN;
    switch (errno) {
      case EACCES:
        result = OPEN_FAILURE_EACCES;
        break;
      case ENOENT:
        result = OPEN_FAILURE_ENOENT;
        break;
      default:
        result = OPEN_FAILURE_UNKNOWN;
        break;
    }
    SendEnumMetric(kMetricExternalDisplayOpenResultName,
                   result, kMetricExternalDisplayResultMax);
    return false;
  }
  SendEnumMetric(kMetricExternalDisplayOpenResultName,
                 OPEN_SUCCESS, kMetricExternalDisplayResultMax);
  return true;
}

std::string ExternalDisplay::RealDelegate::GetName() const { return name_; }

bool ExternalDisplay::RealDelegate::PerformI2COperation(
    struct i2c_rdwr_ioctl_data* data) {
  DCHECK(data);
  if (ioctl(fd_, I2C_RDWR, data) < 0) {
    PLOG(ERROR) << "I2C_RDWR ioctl to " << name_ << " failed";
    return false;
  }
  return true;
}

ExternalDisplay::TestApi::TestApi(ExternalDisplay* display)
    : display_(display) {
  display_->clock_.set_current_time_for_testing(
      base::TimeTicks::FromInternalValue(1000));  // Arbitrary.
}

ExternalDisplay::TestApi::~TestApi() {}

void ExternalDisplay::TestApi::AdvanceTime(base::TimeDelta interval) {
  display_->clock_.set_current_time_for_testing(
      display_->clock_.GetCurrentTime() + interval);
}

base::TimeDelta ExternalDisplay::TestApi::GetTimerDelay() const {
  return display_->timer_.IsRunning() ?
      display_->timer_.GetCurrentDelay() : base::TimeDelta();
}

bool ExternalDisplay::TestApi::TriggerTimeout() {
  if (!display_->timer_.IsRunning())
    return false;

  display_->timer_.Stop();
  display_->UpdateState();
  return true;
}

ExternalDisplay::ExternalDisplay(scoped_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      state_(STATE_IDLE),
      current_brightness_percent_(0.0),
      max_brightness_level_(0),
      pending_brightness_adjustment_percent_(0.0) {
}

ExternalDisplay::~ExternalDisplay() {}

void ExternalDisplay::AdjustBrightnessByPercent(double percent_offset) {
  pending_brightness_adjustment_percent_ += percent_offset;
  if (!timer_.IsRunning()) {
    DCHECK_EQ(STATE_IDLE, state_);
    UpdateState();
  }
}

uint16_t ExternalDisplay::BrightnessPercentToLevel(double percent) const {
  return static_cast<uint16_t>(lround(percent * max_brightness_level_ / 100.0));
}

bool ExternalDisplay::HaveCachedBrightness() {
  return max_brightness_level_ > 0 &&
      !last_brightness_update_time_.is_null() &&
      (clock_.GetCurrentTime() -
       last_brightness_update_time_).InMilliseconds() <=
      kCachedBrightnessValidMs;
}

bool ExternalDisplay::HavePendingBrightnessAdjustment() const {
  return pending_brightness_adjustment_percent_ < -kEpsilon ||
      pending_brightness_adjustment_percent_ > kEpsilon;
}

void ExternalDisplay::StartTimer(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay, this, &ExternalDisplay::UpdateState);
}

bool ExternalDisplay::RequestBrightness() {
  VLOG(1) << "Requesting brightness from " << delegate_->GetName();
  std::vector<uint8_t> message;
  message.push_back(kDdcGetCommand);
  message.push_back(kDdcBrightnessIndex);
  const SendResult result = SendMessage(message);
  SendEnumMetric(kMetricExternalBrightnessRequestResultName,
                 result, kMetricExternalDisplayResultMax);
  return result == SEND_SUCCESS;
}

bool ExternalDisplay::ReadBrightness() {
  std::vector<uint8_t> message(8);
  const ReceiveResult result = ReceiveMessage(&message);
  if (result != RECEIVE_SUCCESS) {
    SendEnumMetric(kMetricExternalBrightnessReadResultName,
                   result, kMetricExternalDisplayResultMax);
    return false;
  }

  // Validate the message.
  DCHECK_EQ(message.size(), 8u);
  if (message[0] != kDdcGetReplyCommand) {
    LOG(WARNING) << "Ignoring brightness reply from " << delegate_->GetName()
                 << " with command " << ByteToHex(message[0]) << " (expected "
                 << ByteToHex(kDdcGetReplyCommand) << ")";
    SendEnumMetric(kMetricExternalBrightnessReadResultName,
                   RECEIVE_BAD_COMMAND, kMetricExternalDisplayResultMax);
    return false;
  }
  if (message[1] != 0x0) {
    LOG(WARNING) << "Ignoring brightness reply from " << delegate_->GetName()
                 << " with non-zero result code " << ByteToHex(message[1]);
    SendEnumMetric(kMetricExternalBrightnessReadResultName,
                   RECEIVE_BAD_RESULT, kMetricExternalDisplayResultMax);
    return false;
  }
  if (message[2] != kDdcBrightnessIndex) {
    LOG(WARNING) << "Ignoring brightness reply from " << delegate_->GetName()
                 << " with feature index " << ByteToHex(message[2])
                 << " (expected " << ByteToHex(kDdcBrightnessIndex) << ")";
    SendEnumMetric(kMetricExternalBrightnessReadResultName,
                   RECEIVE_BAD_INDEX, kMetricExternalDisplayResultMax);
    return false;
  }
  // Don't bother checking the "VCP type code" in the fourth byte.

  const uint16_t max_level =
      (static_cast<uint16_t>(message[4]) << 8) + message[5];
  if (max_level == 0) {
    LOG(WARNING) << "Received maximum brightness of 0 from "
                 << delegate_->GetName();
    SendEnumMetric(kMetricExternalBrightnessReadResultName,
                   RECEIVE_ZERO_MAX_VALUE, kMetricExternalDisplayResultMax);
    return false;
  }

  if (max_brightness_level_ > 0 && max_level != max_brightness_level_) {
    LOG(WARNING) << "Maximum brightness from " << delegate_->GetName()
                 << " changed from " << max_brightness_level_
                 << " to " << max_level;
  }
  max_brightness_level_ = max_level;

  const uint64_t current_level =
      (static_cast<uint16_t>(message[6]) << 8) + message[7];
  current_brightness_percent_ = util::ClampPercent(
      static_cast<double>(current_level) / max_brightness_level_ * 100.0);

  VLOG(1) << "Received current brightness " << current_level
          << " (" << current_brightness_percent_ << "%) and maximum brightness "
          << max_level << " from " << delegate_->GetName();
  last_brightness_update_time_ = clock_.GetCurrentTime();
  SendEnumMetric(kMetricExternalBrightnessReadResultName,
                 RECEIVE_SUCCESS, kMetricExternalDisplayResultMax);
  return true;
}

bool ExternalDisplay::WriteBrightness() {
  const double new_percent = util::ClampPercent(
      current_brightness_percent_ + pending_brightness_adjustment_percent_);
  const uint16_t new_level = BrightnessPercentToLevel(new_percent);

  // Don't send anything if the brightness isn't changing, but update the
  // timestamp to indicate that what we have is still current: if the user is
  // mashing on a brightness key, they're probably not simultaneously hitting
  // the display's physical buttons.
  if (new_level == BrightnessPercentToLevel(current_brightness_percent_)) {
    last_brightness_update_time_ = clock_.GetCurrentTime();
    return false;
  }

  VLOG(1) << "Writing brightness " << new_level
          << " (" << new_percent << "%) to " << delegate_->GetName();
  std::vector<uint8_t> message;
  message.push_back(kDdcSetCommand);
  message.push_back(kDdcBrightnessIndex);
  message.push_back(new_level >> 8);    // High byte.
  message.push_back(new_level & 0xff);  // Low byte.
  const SendResult result = SendMessage(message);
  if (result != SEND_SUCCESS) {
    SendEnumMetric(kMetricExternalBrightnessWriteResultName,
                   result, kMetricExternalDisplayResultMax);
    return false;
  }

  current_brightness_percent_ = new_percent;
  last_brightness_update_time_ = clock_.GetCurrentTime();
  SendEnumMetric(kMetricExternalBrightnessWriteResultName,
                 SEND_SUCCESS, kMetricExternalDisplayResultMax);
  return true;
}

void ExternalDisplay::UpdateState() {
  switch (state_) {
    case STATE_IDLE:
      // Nothing to do.
      if (!HavePendingBrightnessAdjustment())
        return;

      // If the current brightness is cached, write the new brightness and then
      // start the timer to prevent another message from being sent too soon.
      if (HaveCachedBrightness()) {
        if (WriteBrightness())
          StartTimer(base::TimeDelta::FromMilliseconds(kDdcSetDelayMs));
        pending_brightness_adjustment_percent_ = 0.0;
        return;
      }

      // Otherwise, request the current brightness level and start the timer to
      // read the reply.
      if (!RequestBrightness()) {
        pending_brightness_adjustment_percent_ = 0.0;
        return;
      }
      state_ = STATE_WAITING_FOR_REPLY;
      StartTimer(base::TimeDelta::FromMilliseconds(kDdcGetDelayMs));
      return;

    case STATE_WAITING_FOR_REPLY:
      state_ = STATE_IDLE;
      // If reading the brightness failed, give up.
      if (!ReadBrightness()) {
        pending_brightness_adjustment_percent_ = 0.0;
        return;
      }

      // Otherwise, if there's still a pending adjustment, send it to the
      // display and start the timer to prevent another message from being sent
      // too soon. If the write fails, discard the pending adjustment to prevent
      // a buggy display from resulting in infinite retries.
      if (HavePendingBrightnessAdjustment() && WriteBrightness())
        StartTimer(base::TimeDelta::FromMilliseconds(kDdcSetDelayMs));
      pending_brightness_adjustment_percent_ = 0.0;
      return;
  }
}

ExternalDisplay::SendResult ExternalDisplay::SendMessage(
    const std::vector<uint8_t>& body) {
  // The body is preceded by an address byte and a length byte and followed by a
  // checksum byte.
  std::vector<uint8_t> message(body.size() + 3);
  DCHECK_LE(message.size(), kDdcMaxMessageLength);

  message[0] = kDdcHostAddress;
  message[1] = kDdcMessageBodyLengthMask | body.size();
  std::copy(body.begin(), body.end(), message.begin() + 2);

  // The final byte is a checksum consisting of the destination address and all
  // previous bytes XOR-ed together.
  message[message.size() - 1] = kDdcDisplayAddress;
  for (size_t i = 0; i < message.size() - 1; ++i)
    message[message.size() - 1] ^= message[i];

  struct i2c_msg i2c_message;
  i2c_message.addr = kDdcI2CAddress;
  i2c_message.flags = 0;
  i2c_message.len = message.size();
  i2c_message.buf = &(message[0]);

  struct i2c_rdwr_ioctl_data ioctl_data;
  ioctl_data.msgs = &i2c_message;
  ioctl_data.nmsgs = 1;

  VLOG(1) << "Sending data to " << delegate_->GetName()
          << ": " << base::HexEncode(&message[0], message.size());
  return delegate_->PerformI2COperation(&ioctl_data) ? SEND_SUCCESS :
      SEND_IOCTL_FAILED;
}

ExternalDisplay::ReceiveResult ExternalDisplay::ReceiveMessage(
    std::vector<uint8_t>* body) {
  DCHECK(body);

  // The message is preceded by an address and a length and followed by a
  // checksum byte.
  std::vector<uint8_t> message(body->size() + 3);
  DCHECK_LE(message.size(), kDdcMaxMessageLength);

  struct i2c_msg i2c_message;
  memset(&i2c_message, 0, sizeof(i2c_message));
  i2c_message.addr = kDdcI2CAddress;
  i2c_message.flags = I2C_M_RD;
  i2c_message.len = message.size();
  i2c_message.buf = &(message[0]);

  struct i2c_rdwr_ioctl_data ioctl_data;
  memset(&ioctl_data, 0, sizeof(ioctl_data));
  ioctl_data.msgs = &i2c_message;
  ioctl_data.nmsgs = 1;
  if (!delegate_->PerformI2COperation(&ioctl_data))
    return RECEIVE_IOCTL_FAILED;

  VLOG(1) << "Received data from " << delegate_->GetName()
          << ": " << base::HexEncode(&(message[0]), message.size());

  // The final byte in a message from the display is the "virtual host address"
  // XOR-ed with all other bytes in the message (excluding the final byte).
  const uint8_t received_checksum = message[message.size() - 1];
  uint8_t computed_checksum = kDdcVirtualHostAddress;
  for (size_t i = 0; i < message.size() - 1; ++i)
    computed_checksum ^= message[i];
  if (received_checksum != computed_checksum) {
    LOG(WARNING) << "Ignoring reply from " << delegate_->GetName()
                 << " with checksum " << ByteToHex(received_checksum)
                 << " (expected " << ByteToHex(computed_checksum)
                 << " for message "
                 << base::HexEncode(&(message[0]), message.size()) << ")";
    return RECEIVE_BAD_CHECKSUM;
  }
  if (message[0] != kDdcDisplayAddress) {
    LOG(WARNING) << "Ignoring reply from " << delegate_->GetName()
                 << " with source address " << ByteToHex(message[0])
                 << " (expected " << ByteToHex(kDdcDisplayAddress) << ")";
    return RECEIVE_BAD_ADDRESS;
  }
  const size_t received_length = message[1] & ~kDdcMessageBodyLengthMask;
  if (received_length != body->size()) {
    LOG(WARNING) << "Ignoring reply from " << delegate_->GetName()
                 << " containing body of length " << received_length
                 << " (expected " << body->size() << ")";
    return RECEIVE_BAD_LENGTH;
  }

  memcpy(&((*body)[0]), &(message[2]), body->size());
  return RECEIVE_SUCCESS;
}

}  // namespace system
}  // namespace power_manager
