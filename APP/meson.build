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
subdir('template_schedule')
subdir('FreeModbus')

dependencies = [
  dependency('HAL'),
  dependency('gd32f4xx_spl'),
  dependency('CherryRB'),
  dependency('Hardware'),
  dependency('SEGGER_RTT'),
  dependency('template_schedule'),
  dependency('FreeModbus'),
]

lds = meson.current_source_dir()/'app_flash.ld'

# ===================== App 可执行文件 =====================
executable('APP.elf', files(
    'Function'/'main.cpp',
),
    dependencies: dependencies,
    link_args: [
        '-T', lds,
        '-Wl,-Map=APP.map',
        '-u', '_SEGGER_RTT',
    ],
    install: false,
)
