# This GYP include should be added to targets whose dependencies need to be
# exported (eg to a .pc file).
{
  'actions': [
    {
      'action_name': 'write-deps',
      'inputs': [
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/>(_target_name)-deps.txt',
      ],
      'action': ['sh', '-c', 'echo >@(deps) > >@(_outputs)'],
    },
  ]
}
