cc = meson.get_compiler('c')
libm = cc.find_library('m')

tiff_dep = dependency('libtiff-4', required: false)

config = configuration_data()
config.set('HAVE_LIBTIFF', tiff_dep.found())
configure_file(
    output: 'config.h',
    configuration: config
)

grab_deps = deps + [libm]

if tiff_dep.found()
    grab_deps += tiff_dep
endif

executable('uca-info',
    sources: ['info.c'],
    include_directories: include_dir,
    dependencies: deps,
    link_with: lib,
    install: true
)

executable('uca-grab',
    sources: ['grab.c', 'common.c'],
    include_directories: include_dir,
    dependencies: grab_deps,
    link_with: lib,
    install: true
)

executable('uca-benchmark',
    sources: ['benchmark.c', 'common.c'],
    include_directories: include_dir,
    dependencies: deps,
    link_with: lib,
    install: true
)
