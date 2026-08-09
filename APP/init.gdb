#target remote localhost:50000
#monitor reset 2
#load
#set *(unsigned int*)0xE000ED08 = 0x08011000
#set $sp = *(unsigned int*)0x08011000
#set $pc = *(unsigned int*)0x08011004
