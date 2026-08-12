project(
	'wl-restart',
	'c',
	version: '0.3',
	license: 'GPL-3',
	meson_version: '>=0.55.0',
	default_options: [
		'warning_level=2',
	],
)
scdoc = dependency('scdoc', required: get_option('man-pages'), version: '>= 1.9.7')

if scdoc.found()
	scdoc_prog = find_program(scdoc.get_variable(pkgconfig: 'scdoc'), native: true)
	mandir = get_option('mandir')

	foreach src : ['wl-restart.1.scd']
		topic = src.split('.')[0]
		section = src.split('.')[1]
		output = '@0@.@1@'.format(topic, section)

		custom_target(
			output,
			input: 'man/' + src,
			output: output,
			command: [
				'sh', '-c', '@0@ < @INPUT@ > @1@'.format(scdoc_prog.full_path(), output)
			],
			install: true,
			install_dir: '@0@/man@1@'.format(mandir, section)
		)
	endforeach
endif
