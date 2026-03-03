dependencies = [
  dependency('gtest'),
  dependency('gmock'),
  dependency('gtest_main'),
  libchips_dep
]

sources = [
  'ccl/format-ccl.cpp',
  'logic/cclp_tws_playthrough.cpp',
  'tws/format-tws.cpp'
]

gtest_libchips_test = executable(
  'libchips-test',
  sources,
  dependencies: dependencies,
)

test('gtest test', gtest_libchips_test, protocol: 'gtest', verbose: true)
