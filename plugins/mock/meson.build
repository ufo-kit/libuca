libm = meson.get_compiler('c').find_library('m')

plugin = shared_library('ucamock',
    sources: ['uca-mock-camera.c'],
    include_directories: include_dir,
    dependencies: deps + [libm],
    link_with: lib,
    install: true,
    install_dir: plugindir,
)
