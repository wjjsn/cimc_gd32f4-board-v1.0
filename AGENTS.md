两个独立的meson工程，分别切换到对应的目录{'APP', 'BootLoader'}执行：

用这个命令来setup：`PATH=/usr/bin:/bin:/usr/sbin:/sbin meson setup build --cross-file arm-none-eabi.ini`

用这个命令来编译：`PATH=/usr/bin:/bin:/usr/sbin:/sbin meson compile -C build/`

用这个命令调试真实的设备：`gdb-multiarch -x init.gdb <path-to-elf> -ex <command> {-ex <command>...} -batch`