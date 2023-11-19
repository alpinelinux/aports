{
  'targets': [
    {
      'target_name': 'base64',
      'type': 'shared_library',
      'include_dirs': ['/usr/include'],
      'direct_dependent_settings': {
        'include_dirs': ['/usr/include'],
        'linkflags': ['-lbase64'],
        'ldflags': ['-lbase64'],
        'libraries': ['-lbase64']
      }
    },
  ]
}
