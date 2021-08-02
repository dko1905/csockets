threads = dependency('threads')

udpchat = executable(
	'udpchat',
	['main.c', 'util/net.c'],
	include_directories: inc,
	dependencies: [threads],
)
