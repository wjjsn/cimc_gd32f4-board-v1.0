freemodbus_root = 'freemodbus/modbus'

freemodbus_sources = files(
  freemodbus_root / 'mb.c',
  freemodbus_root / 'rtu' / 'mbrtu.c',
  freemodbus_root / 'rtu' / 'mbcrc.c',
  freemodbus_root / 'ascii' / 'mbascii.c',
  freemodbus_root / 'functions' / 'mbfuncinput.c',
  freemodbus_root / 'functions' / 'mbfuncholding.c',
  freemodbus_root / 'functions' / 'mbfunccoils.c',
  freemodbus_root / 'functions' / 'mbfuncdisc.c',
  freemodbus_root / 'functions' / 'mbfuncother.c',
  freemodbus_root / 'functions' / 'mbutils.c',
  'port' / 'portevent.c',
  'port' / 'portother.c',
  'port' / 'port_gd32f4.cpp',
  'modbus_slave.cpp',
  'port' / 'register_callbacks.cpp',
)

freemodbus_dep = declare_dependency(
  sources: freemodbus_sources,
  include_directories: include_directories(
    '.',
    'port',
    freemodbus_root / 'include',
    freemodbus_root / 'rtu',
    freemodbus_root / 'ascii',
  ),
)

meson.override_dependency('FreeModbus', freemodbus_dep)
