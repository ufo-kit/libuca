tiff_dep = dependency('libtiff-4')

plugin = shared_library('ucafile',
    sources: ['uca-file-camera.c'],
    include_directories: include_dir,
    dependencies: deps + [tiff_dep],
    link_with: lib,
    install: true,
    install_dir: plugindir,
)
