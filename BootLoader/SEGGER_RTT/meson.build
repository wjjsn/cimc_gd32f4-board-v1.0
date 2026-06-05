# project('SEGGER_RTT', 'c',
#     version: '8.38',
# )

segger_rtt_src = files(
  'SEGGER_RTT.c',
  'SEGGER_RTT_printf.c',
)

segger_rtt_dep = declare_dependency(
  sources: segger_rtt_src,
  include_directories: include_directories('.'),
)

meson.override_dependency('SEGGER_RTT', segger_rtt_dep)
