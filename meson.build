project('libuca', 'c',
    version: '2.4.0',
    default_options: ['c_std=c11', 'warning_level=2']
)

version = meson.project_version()
components = version.split('.')
version_major = components[0]
version_minor = components[1]
version_patch = components[2]

gnome = import('gnome')

glib_dep = dependency('glib-2.0', version: '>= 2.56')
gio_dep = dependency('gio-2.0', version: '>= 2.56')
gobject_dep = dependency('gobject-2.0', version: '>= 2.56')
gmodule_dep = dependency('gmodule-2.0', version: '>= 2.56')

deps = [glib_dep, gio_dep, gobject_dep, gmodule_dep]

subdir('src')

include_dir = include_directories('src')

subdir('bin')
subdir('plugins')
subdir('test')
