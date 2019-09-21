# qemu -S -s -hda ./bin/ucore.img -monitor stdio # 必须在两个terminal中做。要写在一个脚本中，需要新开一个shell
gdb -tui -x tools/gdbinit
