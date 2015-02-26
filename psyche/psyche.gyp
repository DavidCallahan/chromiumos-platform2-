{
  'target_defaults': {
    'defines': [
      '__STDC_FORMAT_MACROS',
    ],
  },
  'targets': [
    {
      'target_name': 'libpsyche',
      'type': 'static_library',
      'sources': [
        'psyche.cc',
      ],
    },
    {
      'target_name': 'psyched',
      'type': 'executable',
      'sources': [
        'psyched.cc',
      ],
      'dependencies': [
        'libpsyche',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'psyche_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': ['libpsyche'],
          'sources': [
            'psyche_testrunner.cc',
          ],
        },
      ],
    }],
  ],
}