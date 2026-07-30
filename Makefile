ARCH := i386
BUILD_DIR := build
KERNEL := $(BUILD_DIR)/kayaos.kernel
ISO := $(BUILD_DIR)/kayaos.iso
ISO_DIR := $(BUILD_DIR)/iso

CC ?= gcc
ASM ?= nasm
ASMFLAGS := -f elf32 -g -F dwarf
GCC_PRIVATE_INCLUDE := $(shell $(CC) -print-file-name=include)

# Base Kernel CFLAGS
CFLAGS := -m32 -std=gnu11 -ffreestanding -O2 -Wall -Wextra  \
          -Iinclude -I.  \
          -fno-builtin -fno-stack-protector -fno-pic -fno-pie -g \
          -mno-sse -mno-sse2 \
          -fno-asynchronous-unwind-tables -fno-unwind-tables -D_FORTIFY_SOURCE=0
          
# Base Kernel LDFLAGS
LDFLAGS := -m32 -T linker.ld -ffreestanding -nostdlib -static -no-pie \
           -Wl,-n -Wl,--build-id=none -Wl,--allow-multiple-definition 

GENERATED_DIR := src/generated

# ==============================================================================
#  NEWLIB (libc.a) YAPILANDIRMASI
# ==============================================================================
# Makefile CFLAGS içine eklenecekler:
CFLAGS += -DNO_GZCOMPRESS -DNO_GZIP -DZ_SOLO

LIBC_DIR := lib
LIBC_LIBS := -L$(LIBC_DIR) -lc -lm

# ==============================================================================
#  LUA KLASÖR YAPILANDIRMASI
# ==============================================================================
LUA_BASE_DIR := include/lua
LUA_CFLAGS   := -I$(LUA_BASE_DIR)
LUA_SOURCES  := $(wildcard $(LUA_BASE_DIR)/*.c) $(wildcard $(LUA_BASE_DIR)/*/*.c)

# ==============================================================================
#  FREETYPE KLASÖR YAPILANDIRMASI
# ==============================================================================
FT_BASE_DIR   := include/freetype
FT_VENDOR_DIR := $(FT_BASE_DIR)/vendor
FT_KAYAOS_DIR := $(FT_BASE_DIR)/kayaos

FT_CFLAGS   := -I$(FT_VENDOR_DIR)/include \
               -DFT2_BUILD_LIBRARY \
               -DFT_CONFIG_OPTIONS_H="\"$(FT_KAYAOS_DIR)/ftoption_kayaos.h\"" \
               -DFT_CONFIG_MODULES_H="\"$(FT_KAYAOS_DIR)/kayaos_ftmodule.h\""

FT_SOURCES   := $(FT_VENDOR_DIR)/src/base/ftbase.c \
               $(FT_VENDOR_DIR)/src/base/ftinit.c \
               $(FT_VENDOR_DIR)/src/base/ftbbox.c \
               $(FT_VENDOR_DIR)/src/base/ftglyph.c \
               $(FT_VENDOR_DIR)/src/base/ftbitmap.c \
               $(FT_VENDOR_DIR)/src/truetype/truetype.c \
               $(FT_VENDOR_DIR)/src/sfnt/sfnt.c \
               $(FT_VENDOR_DIR)/src/smooth/smooth.c \
               $(FT_KAYAOS_DIR)/ftsystem_kayaos.c \
               $(FT_KAYAOS_DIR)/ftdebug_kayaos.c \
               $(FT_KAYAOS_DIR)/kayaos_freetype.c

# ==============================================================================
#  C KAYNAK DOSYALARI VE TCC SÜZME İŞLEMİ (Unity-Build Filtresi)
# ==============================================================================
# LUA_SOURCES listeye eklendi!
RAW_C_SOURCES := $(wildcard src/kernel/*.c) $(wildcard src/kernel/*/*.c) \
                 $(wildcard src/kernel/*/*/*.c) $(wildcard src/kernel/*/*/*/*.c) \
                 $(FT_SOURCES) $(LUA_SOURCES) $(wildcard $(GENERATED_DIR)/*.c)

TCC_EXCLUDES := src/kernel/tcc/tccpp.c src/kernel/tcc/tccgen.c src/kernel/tcc/tccelf.c \
                src/kernel/tcc/tccrun.c src/kernel/tcc/tccasm.c src/kernel/tcc/tccdbg.c \
                src/kernel/tcc/i386-gen.c src/kernel/tcc/i386-link.c src/kernel/tcc/i386-asm.c

C_SOURCES := $(filter-out $(TCC_EXCLUDES), $(RAW_C_SOURCES))

BOOT_SOURCES        := $(wildcard src/boot/*.s)
KERNEL_ASM_SOURCES := $(wildcard src/kernel/*.asm)

# ==============================================================================
#  NESNE (OBJECT) DOSYALARI EŞLEŞTİRMESİ
# ==============================================================================
BOOT_OBJECTS := $(BOOT_SOURCES:%.s=$(BUILD_DIR)/%.o)
ASM_OBJECTS  := $(KERNEL_ASM_SOURCES:%.asm=$(BUILD_DIR)/%.o)
C_OBJECTS    := $(C_SOURCES:%.c=$(BUILD_DIR)/%.o)

OBJECTS := $(BOOT_OBJECTS) $(ASM_OBJECTS) $(C_OBJECTS)

.PHONY: all iso run assets clean

all: $(KERNEL)

iso: $(ISO)

run: $(ISO)
	sudo qemu-system-i386 \
	-m 2048 \
	-smp cores=4 \
	-cdrom build/kayaos.iso \
	-serial stdio \
	-device virtio-gpu-pci \
	-device virtio-net-pci

assets:
	python3 tools/embed_assets.py assets $(GENERATED_DIR)

$(KERNEL): assets $(OBJECTS) linker.ld
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBC_LIBS) -o $@ -lgcc
	grub-file --is-x86-multiboot $@

# DÜZELTİLDİ: initrd.tar ve nsfb dosyaları ISO klasörüne kopyalanıyor!
$(ISO): $(KERNEL) boot/grub/grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kayaos.kernel
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	@if [ -f initrd.tar ]; then cp initrd.tar $(ISO_DIR)/boot/initrd.tar; fi
	@if [ -f nsfb ]; then cp nsfb $(ISO_DIR)/boot/nsfb; fi
	grub-mkrescue -o $@ $(ISO_DIR)

# ==============================================================================
#  DERLEME KURALLARI VE BAYRAK ENJEKSİYONLARI
# ==============================================================================
$(BUILD_DIR)/$(FT_BASE_DIR)/%.o: CFLAGS += $(FT_CFLAGS)
$(BUILD_DIR)/$(LUA_BASE_DIR)/%.o: CFLAGS += $(LUA_CFLAGS)
$(BUILD_DIR)/src/kernel/tcc/%.o: CFLAGS += -DCONFIG_TCC_BACKTRACE=0

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm
	mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)