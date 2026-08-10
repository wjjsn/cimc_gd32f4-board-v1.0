# project('CherryRB', 'c', 'cpp',
#     version: '1.0.0',
# )

cherrrb_src = files('chry_ringbuffer.c')

cherrrb_dep = declare_dependency(
  sources: cherrrb_src,
  include_directories: include_directories('.'),
)

meson.override_dependency('CherryRB', cherrrb_dep)
