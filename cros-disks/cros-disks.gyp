{
  'variables': {
    'libbase_ver': 242728,
  },
  'target_defaults': {
    'dependencies': [
      '../libchromeos/libchromeos-<(libbase_ver).gyp:libchromeos-<(libbase_ver)',
      '../metrics/libmetrics-<(libbase_ver).gyp:libmetrics-<(libbase_ver)',
    ],
    'libraries': [
      '-lminijail',
      '-lrootdev',
    ],
    'variables': {
      'deps': [
        'blkid',
        'dbus-c++-1',
        'glib-2.0',
        'gobject-2.0',
        'gthread-2.0',
        'libchrome-<(libbase_ver)',
        'libudev',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libdisks-adaptors',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'adaptor',
        'xml2cpp_in_dir': 'dbus_bindings',
        'xml2cpp_out_dir': 'include/cros-disks/dbus_adaptors',
      },
      'sources': [
        '<(xml2cpp_in_dir)/org.chromium.CrosDisks.xml',
      ],
      'includes': ['../common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'libdisks',
      'type': 'static_library',
      'dependencies': ['libdisks-adaptors'],
      'sources': [
        'archive_manager.cc',
        'cros_disks_server.cc',
        'daemon.cc',
        'device_ejector.cc',
        'device_event.cc',
        'device_event_moderator.cc',
        'device_event_queue.cc',
        'disk.cc',
        'disk_manager.cc',
        'exfat_mounter.cc',
        'external_mounter.cc',
        'file_reader.cc',
        'filesystem.cc',
        'format_manager.cc',
        'fuse_mounter.cc',
        'glib_process.cc',
        'metrics.cc',
        'mount_info.cc',
        'mount_manager.cc',
        'mount_options.cc',
        'mounter.cc',
        'ntfs_mounter.cc',
        'platform.cc',
        'process.cc',
        'sandboxed_process.cc',
        'session_manager_proxy.cc',
        'system_mounter.cc',
        'udev_device.cc',
        'usb_device_info.cc',
      ],
    },
    {
      'target_name': 'disks',
      'type': 'executable',
      'dependencies': ['libdisks'],
      'sources': [
        'main.cc',
      ]
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'disks_testrunner',
          'type': 'executable',
          'dependencies': ['libdisks'],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'archive_manager_unittest.cc',
            'device_event_moderator_unittest.cc',
            'device_event_queue_unittest.cc',
            'disk_manager_unittest.cc',
            'disk_unittest.cc',
            'disks_testrunner.cc',
            'external_mounter_unittest.cc',
            'file_reader_unittest.cc',
            'format_manager_unittest.cc',
            'glib_process_unittest.cc',
            'metrics_unittest.cc',
            'mount_info_unittest.cc',
            'mount_manager_unittest.cc',
            'mount_options_unittest.cc',
            'mounter_unittest.cc',
            'platform_unittest.cc',
            'process_unittest.cc',
            'system_mounter_unittest.cc',
            'udev_device_unittest.cc',
            'usb_device_info_unittest.cc',
          ]
        },
      ],
    }],
  ],
}
