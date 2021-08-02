project(
	'udpchat', 'c',
	version: '0.0.1',
	license: 'GPL3.0-or-later',
	default_options: ['c_std=gnu99', 'warning_level=2']
)
# disable strict aliasning
if meson.get_compiler('c').has_argument('-fno-strict-aliasing')
	extra_args = ['-fno-strict-aliasing']
endif
# include header files
inc = include_directories('src')
# include source files
subdir('src')
