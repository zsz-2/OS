#!/usr/bin/env bash
SHELL =/bin/bash
BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500
AS = nasm
CC = gcc
LD = ld
LIB = -I ./lib -I ./lib/kernel -I ./lib/user -I ./device  -I ./kernel -I ./thread -I ./userprog -I ./device/fs
ASFLAGS = -f elf
CFLAGS = -Wall $(LIB) -m32 -c -fno-builtin -W -Wstrict-prototypes \
	-Wmissing-prototypes -fno-stack-protector 
LDFLAGS = -Ttext $(ENTRY_POINT) -m elf_i386   -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o  $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o \
	$(BUILD_DIR)/timer.o  $(BUILD_DIR)/kernel.o  $(BUILD_DIR)/print.o \
	$(BUILD_DIR)/debug.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/bitmap.o \
	$(BUILD_DIR)/string.o $(BUILD_DIR)/thread.o  $(BUILD_DIR)/list.o \
	$(BUILD_DIR)/switch.o $(BUILD_DIR)/func_tool.o $(BUILD_DIR)/sync.o\
	$(BUILD_DIR)/console.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioqueue.o\
	$(BUILD_DIR)/tss.o  $(BUILD_DIR)/process.o $(BUILD_DIR)/syscall-init.o\
	$(BUILD_DIR)/syscall.o  $(BUILD_DIR)/stdio.o $(BUILD_DIR)/ide.o\
	$(BUILD_DIR)/std-kernel.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/ide.o \
	$(BUILD_DIR)/dir.o  $(BUILD_DIR)/file.o $(BUILD_DIR)/inode.o\
	$(BUILD_DIR)/fork.o


#############################  C代码编译 #####################################
$(BUILD_DIR)/main.o: ./kernel/main.c ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/init.h ./thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: ./kernel/init.c ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/init.h  ./device/timer.h ./lib/kernel/interrupt.h\
		./thread/thread.h  ./lib/kernel/memory.h  ./device/keyboard.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: ./kernel/interrupt_change.c ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/io.h   ./lib/kernel/interrupt.h ./lib/kernel/global.h\
		./lib/kernel/debug.h  
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o: ./device/timer.c ./lib/kernel/print.h  ./thread/thread.h\
		./lib/stdint.h ./lib/kernel/io.h   ./device/timer.h  ./lib/kernel/debug.h\
		./lib/kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o:  ./lib/string.c  ./lib/kernel/global.h \
		./lib/kernel/debug.h ./lib/string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: ./kernel/debug.c ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/interrupt.h   ./lib/kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o: ./kernel/memory.c  ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/memory.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o:  ./kernel/bitmap.c ./lib/string.h  ./lib/kernel/bitmap.h \
		./lib/stdint.h  ./lib/kernel/interrupt.h  ./lib/kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/thread.o: ./thread/thread.c  ./lib/string.h ./lib/kernel/global.h  ./lib/stdint.h ./thread/sync.h\
		./lib/kernel/memory.h  ./thread/thread.h ./lib/kernel/interrupt.h  ./lib/kernel/debug.h ./lib/kernel/list.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o: ./kernel/list.c  ./lib/kernel/list.h ./lib/kernel/interrupt.h 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o: ./thread/sync.c  ./thread/sync.h  ./lib/kernel/interrupt.h  \
		./lib/kernel/list.h  ./thread/thread.h ./lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o: ./device/console.c  ./device/console.h ./thread/sync.h \
		./lib/kernel/print.h  ./thread/thread.h  ./lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o: ./device/keyboard.c  ./device/keyboard.h  ./lib/kernel/global.h\
		./lib/kernel/print.h  ./lib/kernel/interrupt.h  ./lib/kernel/io.h   ./lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ioqueue.o: ./device/ioqueue.c  ./device/ioqueue.h ./lib/kernel/interrupt.h \
		./lib/kernel/debug.h ./lib/kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/tss.o: ./userprog/tss.c  ./userprog/tss.h  ./lib/kernel/global.h\
		./lib/stdint.h  ./lib/kernel/io.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/process.o: ./userprog/process.c  ./userprog/tss.h  ./userprog/userprog.h ./lib/kernel/global.h\
		./lib/stdint.h ./thread/thread.h  ./lib/kernel/list.h  ./lib/kernel/io.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fork.o: ./userprog/fork.c  ./userprog/process.c  ./userprog/tss.h  ./userprog/userprog.h ./lib/kernel/global.h\
		./lib/stdint.h ./thread/thread.h  ./lib/kernel/list.h  ./lib/kernel/io.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall.o: ./lib/user/syscall.c ./lib/user/syscall.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall-init.o: ./userprog/syscall-init.c  ./userprog/syscall-init.h  ./thread/thread.h \
		./lib/stdint.h  ./lib/kernel/global.h  ./lib/kernel/interrupt.h  ./lib/kernel/io.h
	$(CC) $(CFLAGS) $< -o $@ 

$(BUILD_DIR)/stdio.o: ./lib/stdio.c ./lib/stdio.h  ./lib/string.h  ./lib/user/syscall.h  ./lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/std-kernel.o:./kernel/stdio-kernel.c ./lib/kernel/stdio-kernel.h  ./lib/stdio.h  ./lib/string.h \
		./lib/user/syscall.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ide.o: ./device/fs/ide.c  ./device/fs/ide.h  ./lib/kernel/interrupt.h  ./device/timer.h  ./lib/kernel/stdio-kernel.h\
		./lib/kernel/io.h    ./lib/kernel/global.h  ./lib/stdint.h  ./lib/kernel/bitmap.h   ./thread/sync.h ./lib/kernel/list.h
	$(CC) $(CFLAGS) $< -o $@


$(BUILD_DIR)/fs.o: ./device/fs/fs.c  ./device/fs/ide.h  ./lib/kernel/interrupt.h  ./device/timer.h  ./lib/kernel/stdio-kernel.h\
		./lib/kernel/io.h    ./lib/kernel/global.h  ./lib/stdint.h  ./lib/kernel/bitmap.h   ./thread/sync.h ./lib/kernel/list.h\
		./device/fs/super_block.h  ./device/fs/fs.h  ./device/fs/dir.h
	$(CC) $(CFLAGS) $< -o $@


$(BUILD_DIR)/ide.o: ./device/fs/ide.c  ./device/fs/ide.h  ./lib/kernel/interrupt.h  ./device/timer.h  ./lib/kernel/stdio-kernel.h\
		./lib/kernel/io.h    ./lib/kernel/global.h  ./lib/stdint.h  ./lib/kernel/bitmap.h   ./thread/sync.h ./lib/kernel/list.h\
		./device/fs/super_block.h  ./device/fs/fs.h  ./device/fs/dir.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/dir.o: ./device/fs/dir.c ./device/fs/ide.h  ./lib/kernel/interrupt.h  ./device/timer.h  ./lib/kernel/stdio-kernel.h\
		./lib/kernel/io.h    ./lib/kernel/global.h  ./lib/stdint.h  ./lib/kernel/bitmap.h   ./thread/sync.h ./lib/kernel/list.h\
		 ./device/fs/super_block.h  ./device/fs/fs.h  ./device/fs/dir.h  
	$(CC) $(CFLAGS) $< -o $@ 

$(BUILD_DIR)/file.o: ./device/fs/file.c ./device/fs/ide.h  ./lib/kernel/interrupt.h  ./device/timer.h  ./lib/kernel/stdio-kernel.h\
		./lib/kernel/io.h    ./lib/kernel/global.h  ./lib/stdint.h  ./lib/kernel/bitmap.h   ./thread/sync.h ./lib/kernel/list.h\
		 ./device/fs/super_block.h  ./device/fs/fs.h  ./device/fs/dir.h
	$(CC) $(CFLAGS) $< -o $@ 

$(BUILD_DIR)/inode.o: ./device/fs/inode.c ./device/fs/ide.h  ./lib/kernel/interrupt.h  ./device/timer.h  ./lib/kernel/stdio-kernel.h\
		./lib/kernel/io.h    ./lib/kernel/global.h  ./lib/stdint.h  ./lib/kernel/bitmap.h   ./thread/sync.h ./lib/kernel/list.h\
		 ./device/fs/super_block.h  ./device/fs/fs.h  ./device/fs/dir.h
	$(CC) $(CFLAGS) $< -o $@ 

############################# 汇编代码编译 ###################################
$(BUILD_DIR)/print.o: ./kernel/print.S 
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kernel.o: ./kernel/kernel_change.S 
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/switch.o: ./thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/func_tool.o: ./kernel/func_tool.S
	$(AS) $(ASFLAGS) $< -o $@
############################  链接所有目标文件 #############################
$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@



.PHONY: all hd mkdir clean

mkdir: 
	if [[ ! -d $(BUILD_DIR) ]]; then mkdir $(BUILD_DIR); fi

hd:
	dd if=$(BUILD_DIR)/kernel.bin \
		of=/home/zhang/Desktop/bochs1/hd60M.img \
		bs=512 count=200 seek=9 conv=notrunc


clean:
	cd $(BUILD_DIR) && rm -f ./*
build: $(BUILD_DIR)/kernel.bin

all: build hd mkdir
