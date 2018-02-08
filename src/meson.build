sources = [
    'uca-camera.c',
    'uca-plugin-manager.c',
    'uca-ring-buffer.c'
]

headers = [
    'uca-camera.h',
    'uca-plugin-manager.h',
]

plugindir = '@0@/@1@/uca'.format(get_option('prefix'), get_option('libdir'))

conf = configuration_data()
conf.set_quoted('UCA_PLUGINDIR', plugindir)
conf.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_2_38')
conf.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_2_38')

configure_file(
    input: 'config.h.meson.in',
    output: 'config.h',
    configuration: conf
)

enums = gnome.mkenums('uca-enums',
    sources: headers,
    h_template: 'uca-enums.h.template',
    c_template: 'uca-enums.c.template',
    install_header: true,
    install_dir: '', # this is horrible but works and prefixes correctly
)

enums_c = enums[0]
enums_h = enums[1]

sources += [enums_c, enums_h]

lib = library('uca',
    sources: sources,
    dependencies: [glib_dep, gobject_dep, gmodule_dep, gio_dep],
    version: version,
    soversion: version_major,
    install: true,
)

gnome.generate_gir(lib,
    namespace: 'Uca',
    nsversion: '@0@.0'.format(version_major),
    sources: sources + headers,
    install: true,
    includes: [
      'GLib-2.0',
      'GObject-2.0',
      'GModule-2.0',
    ],
)

pkg = import('pkgconfig')

pkg.generate(
    libraries: [lib],
    version: version,
    name: 'libuca',
    description: 'Library for unified scientific camera access',
    requires: ['glib-2.0', 'gobject-2.0'],
    variables: ['plugindir=${libdir}/uca'],
)

install_headers(headers, subdir: 'uca')
