#!/usr/bin/env bash
SHELL =/bin/bash
BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500
AS = nasm
CC = gcc
LD = ld
LIB = -I ./lib -I ./lib/kernel -I ./lib/user -I ./device  -I ./kernel
ASFLAGS = -f elf
CFLAGS = -Wall $(LIB) -m32 -c -fno-builtin -W -Wstrict-prototypes \
	-Wmissing-prototypes -fno-stack-protector 
LDFLAGS = -Ttext $(ENTRY_POINT) -m elf_i386   -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o  $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o \
	$(BUILD_DIR)/timer.o  $(BUILD_DIR)/kernel.o  $(BUILD_DIR)/print.o \
	$(BUILD_DIR)/debug.o


#############################  C代码编译 #####################################
$(BUILD_DIR)/main.o: ./kernel/main.c ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/init.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: ./kernel/init.c ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/init.h  ./device/timer.h ./lib/kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: ./kernel/interrupt_change.c ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/io.h   ./lib/kernel/interrupt.h ./lib/kernel/global.h
	$(CC) $(CFLAGS) $< -o $@


$(BUILD_DIR)/timer.o: ./device/timer.c ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/io.h   ./device/timer.h 
	$(CC) $(CFLAGS) $< -o $@


$(BUILD_DIR)/debug.o: ./kernel/debug.c ./lib/kernel/print.h \
		./lib/stdint.h ./lib/kernel/interrupt.h   ./lib/kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@


############################# 汇编代码编译 ###################################
$(BUILD_DIR)/print.o: ./kernel/print.S 
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/kernel.o: ./kernel/kernel_change.S 
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
