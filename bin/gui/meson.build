cc = meson.get_compiler('c')

libm = cc.find_library('m')
gtk_dep = dependency('gtk+-2.0', required: false)
gthread_dep = dependency('gthread-2.0')

if gtk_dep.found()
    gnome = import('gnome')

    resources = gnome.compile_resources(
        'resources', 'uca-camera-control.gresource.xml',
        source_dir: '.',
        c_name: 'foo'
    )

    sources = [
        'uca-camera-control.c',
        'egg-histogram-view.c',
        'egg-property-cell-renderer.c',
        'egg-property-tree-view.c',
        resources
    ]

    executable('uca-camera-control',
        sources: sources,
        include_directories: include_dir,
        dependencies: [libm, gtk_dep, gthread_dep],
        link_with: lib,
        install: true,
    )
endif
