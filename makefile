#####################################
#		Environment					#
#####################################
BUILD_DIR = ./build
OUT_DIR = ./out

#####################################
#		Build Tools					#
#####################################
AS = nasm
CC = gcc
LD = ld

#####################################
#		Build Flags					#
#####################################
KERNEL_ENTRY = 0xc0001000
ASFLAGS = -f elf
CFLAGS = -c -m32 -fno-stack-protector -fno-builtin -nostdinc
LDFLAGS = -m elf_i386 -Ttext $(KERNEL_ENTRY) -e main -z noexecstack

#####################################
#		Include Files				#
#####################################
LIB_INC = ./lib/inc
KERNEL_INC = ./kernel/inc
USR_INC = ./usr/inc
TEST_INC = ./test/inc

#####################################
#		Source Files				#
#####################################
LIB_SRC = ./lib/src
KERNEL_SRC = ./kernel/src
USR_SRC = ./usr/src
TEST_SRC = ./test/src

#####################################
#		Output Files				#
#####################################
lib_obj :=
kernel_obj :=
usr_obj :=

#####################################
#		MBR: mbr.bin				#
#####################################
$(OUT_DIR)/mbr.bin: boot/mbr.s
	$(AS) -i boot/ $< -o $@

#####################################
#		Bootloader: loader.bin		#
#####################################
$(OUT_DIR)/loader.bin: boot/loader.s
	$(AS) -I boot/ $< -o $@

 #####################################
#		Object Files: C				#
#####################################
$(BUILD_DIR)/%.o: $(LIB_SRC)/%.c
	$(CC) $(CFLAGS) -I $(LIB_INC) $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_SRC)/%.c
	$(CC) $(CFLAGS) -I $(LIB_INC) -I $(KERNEL_INC) -I $(TEST_INC) $< -o $@

$(BUILD_DIR)/%.o: $(USR_SRC)/%.c
	$(CC) $(CFLAGS) -I $(LIB_INC) -I $(USR_INC) $< -o $@

$(BUILD_DIR)/%.o: $(TEST_SRC)/%.c
	$(CC) $(CFLAGS) -I $(LIB_INC) -I $(KERNEL_INC) -I $(TEST_INC) $< -o $@

#####################################
#      	Object Files: Assembly		#
#####################################
$(BUILD_DIR)/%.o: $(LIB_SRC)/%.s
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_SRC)/%.s
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(USR_SRC)/%.s
	$(AS) $(ASFLAGS) $< -o $@

#####################################
#		Lib Object Files			#
#####################################
lib_obj		+= $(BUILD_DIR)/string.o 

#####################################
#		Test Object Files			#
#####################################
test_objs := $(BUILD_DIR)/test_all.o
test_objs += $(BUILD_DIR)/test_print.o
test_objs += $(BUILD_DIR)/test_string.o
test_objs += $(BUILD_DIR)/test_bitmap.o
test_objs += $(BUILD_DIR)/test_list.o
test_objs += $(BUILD_DIR)/test_memory.o
test_objs += $(BUILD_DIR)/test_interrupt.o

$(BUILD_DIR)/test.o: $(test_objs)
	$(LD) -m elf_i386 -r $^ -o $@

#####################################
#		Kernel Object Files			#
#####################################
kernel_obj += $(BUILD_DIR)/main.o
kernel_obj += $(BUILD_DIR)/test.o
kernel_obj += $(BUILD_DIR)/print.o
kernel_obj += $(BUILD_DIR)/bitmap.o
kernel_obj += $(BUILD_DIR)/list.o
kernel_obj += $(BUILD_DIR)/init.o
kernel_obj += $(BUILD_DIR)/memory.o
kernel_obj	+= $(BUILD_DIR)/kernel.o
kernel_obj	+= $(BUILD_DIR)/interrupt.o
kernel_obj += $(lib_obj)

#####################################
#		User Object Files			#
#####################################
usr_obj += $(lib_obj)

#####################################
#		Kernel: kernel.bin			#
#####################################
$(OUT_DIR)/kernel.bin: $(kernel_obj)
	$(LD) $(LDFLAGS) $^ -o $@

#####################################
#		User Program				#
###s##################################


#####################################
#		Command						#
#####################################
.PHONY: dir clean all

dir:
	@echo ">>>Start $@..."
	@echo "mkdir $(BUILD_DIR)"
	@if [ ! -d $(BUILD_DIR) ];then mkdir $(BUILD_DIR);fi
	@echo "mkdir $(OUT_DIR)"
	@if [ ! -d $(OUT_DIR) ];then mkdir $(OUT_DIR);fi
	@echo ">>>make $@ done."

clean:
	@echo ">>>Start $@..."
	rm -f $(BUILD_DIR)/*
	rm -f $(OUT_DIR)/*
	@echo ">>>make $@ done."

all: dir $(OUT_DIR)/mbr.bin $(OUT_DIR)/loader.bin $(OUT_DIR)/kernel.bin
	@echo ">>>make $@ done."