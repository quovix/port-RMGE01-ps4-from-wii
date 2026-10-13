# SPDX-License-Identifier: Apache-2.0
# @orig none — build do produto (quovix). Baseado no sample Makefile da OpenOrbis.
#
# Casca v0 do PKG: boot + verificação de mídia (BYO-dump). Sem motor ainda.
# Requer: OO_PS4_TOOLCHAIN setado; LLVM "puro" (brew llvm@18); tools sob Rosetta.

# --- metadata do pacote (derivado da ASIRN) ---------------------------------
TITLE       := quovix RMGE01 (BYO-dump)
VERSION     := 1.01
TITLE_ID    := QVOX00001
CONTENT_ID  := UP0001-QVOX00001_00-7E0C9848AC532465

LIBS        := -lc -lkernel -lc++ -lSceSystemService -lSceVideoOut

# --- fontes (nosso código; o motor entra depois) ----------------------------
SRC_C       := src/main.c ../product/media/media_verify.c ../product/media/sha1.c \
               ../runtime/compat/scalar_math.c ../runtime/compat/math_smoke.c \
               ../runtime/video/video_out.c ../runtime/video/text.c \
               ../product/art/art_splash.c ../product/art/yaz0.c \
               ../product/art/rarc.c ../product/art/tpl.c \
               ../runtime/platform/orbis/mtx.c ../runtime/platform/orbis/gx_vi.c \
               ../runtime/platform/orbis/os.c ../runtime/platform/orbis/dvd.c \
               ../runtime/platform/orbis/backend_bringup.c
SRC_CPP     := ../runtime/compat/heap_new.cpp ../runtime/compat/heap_smoke.cpp
INCLUDES    := -I../product/media -I../runtime/compat -I../runtime/video -I../product/art \
               -I../runtime/platform/orbis

# --- toolchain --------------------------------------------------------------
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
LLVM        ?= /opt/homebrew/opt/llvm@18
CC          := $(LLVM)/bin/clang
CXX         := $(LLVM)/bin/clang++
LD          := $(LLVM)/bin/ld.lld
CDIR        := macos
INTDIR      := build
PKG_STAGING := $(INTDIR)/pkg-staging

OBJS        := $(patsubst %.c,$(INTDIR)/%.o,$(notdir $(SRC_C))) \
               $(patsubst %.cpp,$(INTDIR)/%.o,$(notdir $(SRC_CPP)))

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c \
               -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include $(INCLUDES) -MMD -MP \
               -fno-fast-math -ffp-contract=off
CXXFLAGS    := $(CFLAGS) -std=c++98 -fno-exceptions -fno-rtti
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
               -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

VPATH       := src:../product/media:../runtime/compat:../runtime/video:../product/art:../runtime/platform/orbis

_unused     := $(shell mkdir -p $(INTDIR))

all: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	@mkdir -p $(PKG_STAGING)
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< $(PKG_STAGING)
	mv $(PKG_STAGING)/$(CONTENT_ID).pkg $@

# Dependências locais da casca: não incorporar arquivos arbitrários da pasta.
LIBMODULES := sce_module/libc.prx sce_module/libSceFios2.prx

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES)
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

eboot.bin: $(OBJS) Makefile
	$(LD) $(OBJS) -o $(INTDIR)/eboot.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself-macos -in=$(INTDIR)/eboot.elf -out=$(INTDIR)/eboot.oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: %.cpp Makefile
	$(CXX) $(CXXFLAGS) -o $@ $<

-include $(OBJS:.o=.d)

clean:
	rm -f $(CONTENT_ID).pkg pkg.gp4 sce_sys/param.sfo eboot.bin $(INTDIR)/*.o $(INTDIR)/*.elf $(INTDIR)/*.oelf

.DELETE_ON_ERROR:
.PHONY: all clean
