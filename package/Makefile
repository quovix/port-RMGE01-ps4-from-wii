# SPDX-License-Identifier: Apache-2.0
# @orig none — build do produto (quovix). Baseado no sample Makefile da OpenOrbis.
#
# Casca v0 do PKG: boot + verificação de mídia (BYO-dump). Sem motor ainda.
# Requer: OO_PS4_TOOLCHAIN setado; LLVM "puro" (brew llvm@18); tools sob Rosetta.

# --- metadata do pacote (derivado da ASIRN) ---------------------------------
TITLE       := quovix RMGE01 (BYO-dump)
VERSION     := 1.00
TITLE_ID    := QVOX00001
CONTENT_ID  := UP0001-QVOX00001_00-7E0C9848AC532465

LIBS        := -lc -lkernel -lc++ -lSceSystemService

# --- fontes (nosso código; o motor entra depois) ----------------------------
SRC_C       := src/main.c ../product/media/media_verify.c ../product/media/sha1.c
INCLUDES    := -I../product/media

# --- toolchain --------------------------------------------------------------
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
LLVM        ?= /opt/homebrew/opt/llvm@18
CC          := $(LLVM)/bin/clang
LD          := $(LLVM)/bin/ld.lld
CDIR        := macos
INTDIR      := build

OBJS        := $(patsubst %.c,$(INTDIR)/%.o,$(notdir $(SRC_C)))

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c \
               -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include $(INCLUDES)
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
               -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

VPATH       := src:../product/media

_unused     := $(shell mkdir -p $(INTDIR))

all: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	@mkdir -p sce_sys
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(OBJS)
	$(LD) $(INTDIR)/*.o -o $(INTDIR)/eboot.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself-macos -in=$(INTDIR)/eboot.elf -out=$(INTDIR)/eboot.oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(CONTENT_ID).pkg pkg.gp4 sce_sys/param.sfo eboot.bin $(INTDIR)/*.o $(INTDIR)/*.elf $(INTDIR)/*.oelf

.PHONY: all clean
