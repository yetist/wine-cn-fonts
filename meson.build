project('wine-cn-fonts', 'c')

add_global_link_arguments('-municode', language:  ['c', 'cpp'])
cc = meson.get_compiler('c')

root = meson.get_external_property('root')

lib_shlwapi = cc.find_library('shlwapi',  dirs : [root])

main= executable('wine-cn-fonts',
    'main.c',
    dependencies: [lib_shlwapi],
)
