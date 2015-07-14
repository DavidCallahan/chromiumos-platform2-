# Code shared by the libpsyche and psyche packages.
{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'include_dirs': [
      'lib',
    ],
  },
  'targets': [
    {
      'target_name': 'libprotobinderproto',
      'type': 'static_library',
      'variables': {
        # TODO(leecam): Fix '//'' hack due to http://brbug.com/684
        # duping issues.
        'proto_in_dir': '<(sysroot)//usr/share/proto',
        'proto_out_dir': 'include/psyche/proto_bindings',
        'gen_bidl': 1,
      },
      'sources': [
        '<(proto_in_dir)/binder.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'libcommon',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libprotobinder',
          'protobuf-lite',
        ],
        'proto_in_dir': 'common',
        'proto_out_dir': 'include/psyche/proto_bindings',
        'gen_bidl': 1,
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        '<(proto_in_dir)/psyche.proto',
        'common/constants.cc',
      ],
      'dependencies': [
        'libprotobinderproto',
      ],
      'includes': ['../../platform2/common-mk/protoc.gypi'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          # Shared code used by tests.
          'target_name': 'libcommontest',
          'type': 'static_library',
          'dependencies': [ 'libcommon' ],
          'sources': [
            'common/binder_test_base.cc',
          ],
        },
      ],
    }],
  ],
}