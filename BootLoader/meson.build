project('GD32F4xx_cimc_board-v1.0', 'c', 'cpp',
    version: '1.0.0',
    default_options: [
        'warning_level=3',
        'werror=false',
        'layout=mirror',
        'cpp_std=c++23'
    ]
)
subdir('Driver'/'HAL')
subdir('Driver'/'GD32F4xx_SPL')
subdir('CherryRB')
subdir('Driver'/'Hardware')
subdir('SEGGER_RTT')

dependencies = [
  dependency('HAL'),
  dependency('gd32f4xx_spl'),
  dependency('CherryRB'),
  dependency('Hardware'),
  dependency('SEGGER_RTT'),
]

lds = meson.current_source_dir()/'bootloader_flash.ld'

# ===================== Bootloader 可执行文件 =====================
executable('bootloader.elf', files(
    'Function'/'main.cpp',
),
    dependencies: dependencies,
    link_args: [
        '-T', lds,
        '-Wl,-Map=bootloader.map',
        '-u', '_SEGGER_RTT',
    ],
    install: false,
)