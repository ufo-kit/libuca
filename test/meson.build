test_mock = executable('test-mock', 
    'test-mock.c', include_directories: include_dir,
    dependencies: deps,
    link_with: lib,
)

test_ring_buffer = executable('test-ring-buffer', 
    'test-ring-buffer.c', include_directories: include_dir,
    dependencies: deps,
    link_with: lib,
)

test('mock', test_mock)
test('test-ring-buffer', test_ring_buffer)
