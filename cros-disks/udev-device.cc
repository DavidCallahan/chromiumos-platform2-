// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/udev-device.h"

#include <fcntl.h>
#include <libudev.h>
#include <parted/parted.h>
#include <stdlib.h>
#include <sys/statvfs.h>

#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <rootdev/rootdev.h>

#include "cros-disks/mount-info.h"
#include "cros-disks/usb-device-info.h"

using std::string;
using std::vector;

namespace {

const char kNullDeviceFile[] = "/dev/null";
const char kAttributeIdProduct[] = "idProduct";
const char kAttributeIdVendor[] = "idVendor";
const char kAttributePartition[] = "partition";
const char kAttributeRange[] = "range";
const char kAttributeReadOnly[] = "ro";
const char kAttributeRemovable[] = "removable";
const char kAttributeSize[] = "size";
const char kPropertyBlkIdFilesystemType[] = "TYPE";
const char kPropertyBlkIdFilesystemLabel[] = "LABEL";
const char kPropertyBlkIdFilesystemUUID[] = "UUID";
const char kPropertyCDROM[] = "ID_CDROM";
const char kPropertyCDROMDVD[] = "ID_CDROM_DVD";
const char kPropertyCDROMMedia[] = "ID_CDROM_MEDIA";
const char kPropertyDeviceType[] = "DEVTYPE";
const char kPropertyDeviceTypeUSBDevice[] = "usb_device";
const char kPropertyFilesystemUsage[] = "ID_FS_USAGE";
const char kPropertyModel[] = "ID_MODEL";
const char kPropertyPartitionSize[] = "UDISKS_PARTITION_SIZE";
const char kPropertyPresentationHide[] = "UDISKS_PRESENTATION_HIDE";
const char kPropertyRotationRate[] = "ID_ATA_ROTATION_RATE_RPM";
const char kVirtualDevicePathPrefix[] = "/sys/devices/virtual/";
const char kUSBDeviceInfoFile[] = "/opt/google/cros-disks/usb-device-info";
const char* kNonAutoMountableFilesystemLabels[] = {
  "C-ROOT", "C-STATE", NULL
};

}  // namespace

namespace cros_disks {

UdevDevice::UdevDevice(struct udev_device *dev)
    : dev_(dev),
      blkid_cache_(NULL) {
  CHECK(dev_) << "Invalid udev device";
  udev_device_ref(dev_);
}

UdevDevice::~UdevDevice() {
  if (blkid_cache_) {
    // It needs to call blkid_put_cache to deallocate the blkid cache.
    blkid_put_cache(blkid_cache_);
  }
  udev_device_unref(dev_);
}

string UdevDevice::EnsureUTF8String(const string& str) {
  return IsStringUTF8(str) ? str : "";
}

bool UdevDevice::IsValueBooleanTrue(const char *value) const {
  return value && strcmp(value, "1") == 0;
}

string UdevDevice::GetAttribute(const char *key) const {
  const char *value = udev_device_get_sysattr_value(dev_, key);
  return (value) ? value : "";
}

bool UdevDevice::IsAttributeTrue(const char *key) const {
  const char *value = udev_device_get_sysattr_value(dev_, key);
  return IsValueBooleanTrue(value);
}

bool UdevDevice::HasAttribute(const char *key) const {
  const char *value = udev_device_get_sysattr_value(dev_, key);
  return value != NULL;
}

string UdevDevice::GetProperty(const char *key) const {
  const char *value = udev_device_get_property_value(dev_, key);
  return (value) ? value : "";
}

bool UdevDevice::IsPropertyTrue(const char *key) const {
  const char *value = udev_device_get_property_value(dev_, key);
  return IsValueBooleanTrue(value);
}

bool UdevDevice::HasProperty(const char *key) const {
  const char *value = udev_device_get_property_value(dev_, key);
  return value != NULL;
}

string UdevDevice::GetPropertyFromBlkId(const char *key) {
  string value;
  const char *dev_file = udev_device_get_devnode(dev_);
  if (dev_file) {
    // No cache file is used as it should always query information from
    // the device, i.e. setting cache file to /dev/null.
    if (blkid_cache_ || blkid_get_cache(&blkid_cache_, kNullDeviceFile) == 0) {
      blkid_dev dev = blkid_get_dev(blkid_cache_, dev_file, BLKID_DEV_NORMAL);
      if (dev) {
        char *tag_value = blkid_get_tag_value(blkid_cache_, key, dev_file);
        if (tag_value) {
          value = tag_value;
          free(tag_value);
        }
      }
    }
  }
  return value;
}

void UdevDevice::GetSizeInfo(uint64 *total_size, uint64 *remaining_size) const {
  static const int kSectorSize = 512;
  uint64 total = 0, remaining = 0;

  // If the device is mounted, obtain the total and remaining size in bytes
  // using statvfs.
  vector<string> mount_paths = GetMountPaths();
  if (!mount_paths.empty()) {
    struct statvfs stat;
    if (statvfs(mount_paths[0].c_str(), &stat) == 0) {
      total = stat.f_blocks * stat.f_frsize;
      remaining = stat.f_bfree * stat.f_frsize;
    }
  }

  // If the UDISKS_PARTITION_SIZE property is set, use it as the total size
  // instead. If the UDISKS_PARTITION_SIZE property is not set but sysfs
  // provides a size value, which is the actual size in bytes divided by 512,
  // use that as the total size instead.
  const char *partition_size =
      udev_device_get_property_value(dev_, kPropertyPartitionSize);
  int64 size = 0;
  if (partition_size) {
    base::StringToInt64(partition_size, &size);
    total = size;
  } else {
    const char *size_attr = udev_device_get_sysattr_value(dev_, kAttributeSize);
    if (size_attr) {
      base::StringToInt64(size_attr, &size);
      total = size * kSectorSize;
    }
  }

  if (total_size)
    *total_size = total;
  if (remaining_size)
    *remaining_size = remaining;
}

size_t UdevDevice::GetPrimaryPartitionCount() const {
  size_t partition_count = 0;
  const char *dev_file = udev_device_get_devnode(dev_);
  if (dev_file) {
    PedDevice* ped_device = ped_device_get(dev_file);
    if (ped_device) {
      PedDisk* ped_disk = ped_disk_new(ped_device);
      if (ped_disk) {
        partition_count = ped_disk_get_primary_partition_count(ped_disk);
        ped_disk_destroy(ped_disk);
      }
      ped_device_destroy(ped_device);
    }
  }
  return partition_count;
}

DeviceMediaType UdevDevice::GetDeviceMediaType() const {
  if (IsPropertyTrue(kPropertyCDROMDVD))
    return DEVICE_MEDIA_DVD;

  if (IsPropertyTrue(kPropertyCDROM))
    return DEVICE_MEDIA_OPTICAL_DISC;

  // Search up the parent device tree to obtain the vendor and product ID
  // of the first device with a device type "usb_device". Then look up the
  // media type based on the vendor and product ID from a USB device info file.
  for (struct udev_device *dev = dev_; dev; dev = udev_device_get_parent(dev)) {
    const char *device_type =
        udev_device_get_property_value(dev, kPropertyDeviceType);
    if (device_type && strcmp(device_type, kPropertyDeviceTypeUSBDevice) == 0) {
      const char *vendor_id =
          udev_device_get_sysattr_value(dev, kAttributeIdVendor);
      const char *product_id =
          udev_device_get_sysattr_value(dev, kAttributeIdProduct);
      if (vendor_id && product_id) {
        USBDeviceInfo info;
        info.RetrieveFromFile(kUSBDeviceInfoFile);
        return info.GetDeviceMediaType(vendor_id, product_id);
      }
    }
  }

  return DEVICE_MEDIA_UNKNOWN;
}

bool UdevDevice::IsMediaAvailable() const {
  bool is_media_available = true;
  if (IsAttributeTrue(kAttributeRemovable)) {
    if (IsPropertyTrue(kPropertyCDROM)) {
      is_media_available = IsPropertyTrue(kPropertyCDROMMedia);
    } else {
      const char *dev_file = udev_device_get_devnode(dev_);
      if (dev_file) {
        int fd = open(dev_file, O_RDONLY);
        if (fd < 0) {
          is_media_available = false;
        } else {
          close(fd);
        }
      }
    }
  }
  return is_media_available;
}

bool UdevDevice::IsAutoMountable() const {
  // TODO(benchan): Find a reliable way to detect if a device is a removable
  // storage as the removable attribute in sysfs does not always tell the truth.
  return !IsOnBootDevice() && !IsVirtual();
}

bool UdevDevice::IsHidden() {
  if (IsPropertyTrue(kPropertyPresentationHide))
    return true;

  // Hide a device that is neither marked as a partition nor a filesystem,
  // unless it has no valid partitions (e.g. the device is unformatted or
  // corrupted). An unformatted or corrupted device is visible in the file
  // the file browser so that we can provide a way to format it.
  if (!HasAttribute(kAttributePartition) &&
      !HasProperty(kPropertyFilesystemUsage) &&
      (GetPrimaryPartitionCount() > 0))
    return true;

  // TODO(benchan): Find a better way to filter out Chrome OS specific
  // partitions instead of excluding partitions with certain labels
  // (e.g. C-ROOT, C-STATE).
  string filesystem_label = GetPropertyFromBlkId(kPropertyBlkIdFilesystemLabel);
  if (!filesystem_label.empty()) {
    for (const char** label = kNonAutoMountableFilesystemLabels;
        *label; ++label) {
      if (strcmp(*label, filesystem_label.c_str()) == 0) {
        return true;
      }
    }
  }
  return false;
}

bool UdevDevice::IsOnBootDevice() const {
  // Obtain the boot device path, e.g. /dev/sda
  char boot_device_path[PATH_MAX];
  if (rootdev(boot_device_path, sizeof(boot_device_path), true, true)) {
    LOG(ERROR) << "Could not determine root device";
    // Assume it is on the boot device when there is any uncertainty.
    // This is to prevent a device, which is potentially on the boot device,
    // from being auto mounted and exposed to users.
    // TODO(benchan): Find a way to eliminate the uncertainty.
    return true;
  }

  // Compare the device file path of the current device and all its parents
  // with the boot device path. Any match indicates that the current device
  // is on the boot device.
  for (struct udev_device *dev = dev_; dev; dev = udev_device_get_parent(dev)) {
    const char *dev_file = udev_device_get_devnode(dev);
    if (dev_file) {
      if (strncmp(boot_device_path, dev_file, PATH_MAX) == 0) {
        return true;
      }
    }
  }
  return false;
}

bool UdevDevice::IsOnRemovableDevice() const {
  for (struct udev_device *dev = dev_; dev; dev = udev_device_get_parent(dev)) {
    const char *value = udev_device_get_sysattr_value(dev, kAttributeRemovable);
    if (IsValueBooleanTrue(value))
      return true;
  }
  return false;
}

bool UdevDevice::IsVirtual() const {
  const char *sys_path = udev_device_get_syspath(dev_);
  if (sys_path) {
    return StartsWithASCII(sys_path, kVirtualDevicePathPrefix, true);
  }
  // To be safe, mark it as virtual device if sys path cannot be determined.
  return true;
}

string UdevDevice::NativePath() const {
  const char *sys_path = udev_device_get_syspath(dev_);
  return sys_path ? sys_path : "";
}

vector<string> UdevDevice::GetMountPaths() const {
  const char *device_path = udev_device_get_devnode(dev_);
  if (device_path) {
    return GetMountPaths(device_path);
  }
  return vector<string>();
}

vector<string> UdevDevice::GetMountPaths(const string& device_path) {
  MountInfo mount_info;
  if (mount_info.RetrieveFromCurrentProcess()) {
    return mount_info.GetMountPaths(device_path);
  }
  return vector<string>();
}

Disk UdevDevice::ToDisk() {
  Disk disk;

  disk.set_is_auto_mountable(IsAutoMountable());
  disk.set_is_read_only(IsAttributeTrue(kAttributeReadOnly));
  disk.set_is_drive(HasAttribute(kAttributeRange));
  disk.set_is_rotational(HasProperty(kPropertyRotationRate));
  disk.set_is_hidden(IsHidden());
  disk.set_is_media_available(IsMediaAvailable());
  disk.set_is_on_boot_device(IsOnBootDevice());
  disk.set_is_virtual(IsVirtual());
  disk.set_media_type(GetDeviceMediaType());
  disk.set_filesystem_type(GetPropertyFromBlkId(kPropertyBlkIdFilesystemType));
  disk.set_uuid(GetPropertyFromBlkId(kPropertyBlkIdFilesystemUUID));
  disk.set_native_path(NativePath());

  // Drive model and filesystem label may not be UTF-8 encoded, so we
  // need to ensure that they are either set to a valid UTF-8 string or
  // an empty string before later passed to a DBus message iterator.
  disk.set_drive_model(EnsureUTF8String(GetProperty(kPropertyModel)));
  disk.set_label(
      EnsureUTF8String(GetPropertyFromBlkId(kPropertyBlkIdFilesystemLabel)));

  const char *dev_file = udev_device_get_devnode(dev_);
  if (dev_file)
    disk.set_device_file(dev_file);

  vector<string> mount_paths = GetMountPaths();
  disk.set_is_mounted(!mount_paths.empty());
  disk.set_mount_paths(mount_paths);

  uint64 total_size, remaining_size;
  GetSizeInfo(&total_size, &remaining_size);
  disk.set_device_capacity(total_size);
  disk.set_bytes_remaining(remaining_size);

  return disk;
}

}  // namespace cros_disks
