fatfs_source_dir = meson.project_source_root()/'..'/'common'/'FatFs'/'source'

# ff.c 会优先包含同目录的 ffconf.h。将上游源码和项目配置复制到构建目录，既能
# 使用自定义配置，又不会修改 Git 子模块，升级 FatFs 时也更容易核对差异。
fatfs_ff_c = configure_file(input: fatfs_source_dir/'ff.c', output: 'ff.c', copy: true)
fatfs_ff_h = configure_file(input: fatfs_source_dir/'ff.h', output: 'ff.h', copy: true)
fatfs_diskio_h = configure_file(input: fatfs_source_dir/'diskio.h', output: 'diskio.h', copy: true)
fatfs_integer_h = configure_file(input: fatfs_source_dir/'integer.h', output: 'integer.h', copy: true)
fatfs_unicode_c = configure_file(input: fatfs_source_dir/'ffunicode.c', output: 'ffunicode.c', copy: true)
fatfs_conf_h = configure_file(input: 'ffconf.h', output: 'ffconf.h', copy: true)

storage_inc = include_directories('.', '../SEGGER_RTT')

storage_lib = static_library('sd_storage',
    fatfs_ff_c,
    fatfs_unicode_c,
    'sdcard.c',
    'diskio.c',
    'sd_storage.c',
    'posix_fs.c',
    'picolibc_tls.c',
    'sd_self_test.c',
    include_directories: storage_inc,
    dependencies: dependency('gd32f4xx_spl'),
    # 裸机链接脚本不初始化 GOT；启用 PIC 会导致全局变量通过未初始化 GOT 访问。
    pic: false,
)

storage_dep = declare_dependency(
    include_directories: storage_inc,
    link_with: storage_lib,
)
meson.override_dependency('sd_storage', storage_dep)
