{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'openssl',
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'system_api',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'privetd_common',
      'type': 'static_library',
      'variables': {
        'dbus_adaptors_out_dir': 'include/privetd',
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
      },
      'sources': [
        'ap_manager_client.cc',
        'cloud_delegate.cc',
        'constants.cc',
        'daemon_state.cc',
        'dbus_bindings/org.chromium.privetd.Manager.xml',
        'device_delegate.cc',
        'dbus_manager.cc',
        'openssl_utils.cc',
        'peerd_client.cc',
        'privet_handler.cc',
        'privetd_conf_parser.cc',
        'security_manager.cc',
        'shill_client.cc',
        'wifi_bootstrap_manager.cc',
        'wifi_ssid_generator.cc',
      ],
      'actions': [
        {
          # Import D-Bus bindings from buffet.
          'action_name': 'generate-buffet-proxies',
          'variables': {
            'dbus_service_config': '../buffet/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/buffet/dbus-proxies.h'
          },
          'sources': [
            '../buffet/dbus_bindings/org.chromium.Buffet.Command.xml',
            '../buffet/dbus_bindings/org.chromium.Buffet.Manager.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
        {
          # Import D-Bus bindings from peerd.
          'action_name': 'generate-peerd-proxies',
          'variables': {
            'dbus_service_config': '../peerd/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/peerd/dbus-proxies.h'
          },
          'sources': [
            '../peerd/dbus_bindings/org.chromium.peerd.Manager.xml',
            '../peerd/dbus_bindings/org.chromium.peerd.Peer.xml',
            '../peerd/dbus_bindings/org.chromium.peerd.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
        {
          # Import D-Bus bindings from shill.
          'action_name': 'generate-shill-proxies',
          'variables': {
            'dbus_service_config': '../shill/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/shill/dbus-proxies.h'
          },
          'sources': [
            '../shill/dbus_bindings/org.chromium.flimflam.Device.xml',
            '../shill/dbus_bindings/org.chromium.flimflam.Manager.xml',
            '../shill/dbus_bindings/org.chromium.flimflam.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
        {
          # Import D-Bus bindings from apmanager.
          'action_name': 'generate-apmanager-proxies',
          'variables': {
            'dbus_service_config': '../apmanager/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/apmanager/dbus-proxies.h'
          },
          'sources': [
            '../apmanager/dbus_bindings/org.chromium.apmanager.Config.xml',
            '../apmanager/dbus_bindings/org.chromium.apmanager.Device.xml',
            '../apmanager/dbus_bindings/org.chromium.apmanager.Manager.xml',
            '../apmanager/dbus_bindings/org.chromium.apmanager.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
      'includes': [
        '../common-mk/generate-dbus-adaptors.gypi',
        '../common-mk/deps.gypi',
      ],
    },
    {
      'target_name': 'privetd',
      'type': 'executable',
      'dependencies': [
        'privetd_common',
      ],
      'variables': {
        'exported_deps': [
          'libwebserv-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'privetd_testrunner',
          'type': 'executable',
          'dependencies': [
            'privetd_common',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'privetd_testrunner.cc',
            'privetd_conf_parser_unittest.cc',
            'privet_handler_unittest.cc',
            'security_manager_unittest.cc',
            'wifi_ssid_generator_unittest.cc',
          ],
        },
      ],
    }],
  ],
}