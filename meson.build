project('blame-browse', 'c', version : '0.1')

cc = meson.get_compiler('c')

foreach a : ['-Wall', '-Wuninitialized', '-Wempty-body', '-Wformat',
      '-Wformat-security', '-Winit-self', '-Wundef',
      '-Wvla', '-Wpointer-arith', '-Wmissing-declarations']
  if cc.has_argument(a)
    add_project_arguments(a, language : ['c'])
  endif
endforeach

gnome = import('gnome')

bb_datadir = get_option('prefix') / get_option('datadir')
bb_srcdir = meson.current_source_dir()

configinc = include_directories('.')

cdata = configuration_data()

cdata.set_quoted('PACKAGE_VERSION', meson.project_version())

gtk_dep = dependency('gtk+-3.0')

subdir('src')

configure_file(output : 'config.h', configuration : cdata)

