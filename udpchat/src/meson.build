threads = dependency('threads')

udpchat = executable(
	'udpchat',
	['main.c', 'util/net.c', 'users.c', 'msg_formatter.c'],
	include_directories: inc,
	dependencies: [threads],
)
