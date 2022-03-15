project('libuca', 'c',
    version: '2.3.0'
)

version = meson.project_version()
components = version.split('.')
version_major = components[0]
version_minor = components[1]
version_patch = components[2]

gnome = import('gnome')

glib_dep = dependency('glib-2.0', version: '>= 2.38')
gio_dep = dependency('gio-2.0', version: '>= 2.38')
gobject_dep = dependency('gobject-2.0', version: '>= 2.38')
gmodule_dep = dependency('gmodule-2.0', version: '>= 2.38')
zmq_dep = dependency('libzmq', required: false)
json_dep = dependency('json-glib-1.0', version: '>=1.1.0', required: false)

deps = [glib_dep, gio_dep, gobject_dep, gmodule_dep, zmq_dep, json_dep]

subdir('src')

include_dir = include_directories('src')

subdir('bin')
subdir('plugins')
subdir('test')
