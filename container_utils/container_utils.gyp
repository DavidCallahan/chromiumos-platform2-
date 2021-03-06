# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'dbus-c++-1',
      ],
      'enable_exceptions': 1,
    },
  },
  'targets': [
    {
      'target_name': 'permission-broker-proxy',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'proxy',
        'xml2cpp_in_dir': '../permission_broker/dbus_bindings',
        'xml2cpp_out_dir': 'include',
      },
      'sources': [
        '<(xml2cpp_in_dir)/org.chromium.PermissionBroker.xml',
      ],
      'includes': ['../../platform2/common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'broker_service',
      'type': 'executable',
      'variables': {
        'deps': ['libbrillo-<(libbase_ver)'],
      },
      'dependencies': [
        'permission-broker-proxy',
      ],
      'sources': [
        'broker_service.cc',
        'broker_service.h',
        'org.chromium.PermissionBroker.h',
      ],
    },
  ],
}
