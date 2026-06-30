ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM")
endif
export PREFIX:=arm-none-eabi-
export CC:=$(PREFIX)gcc
export CXX:=$(PREFIX)g++
export AS:=$(PREFIX)as
export AR:=$(PREFIX)ar
export OBJCOPY:=$(PREFIX)objcopy
export LD:=$(PREFIX)gcc
include $(DEVKITARM)/3ds_rules
TARGET:=mivf_player_3ds
BUILD:=build
SOURCES:=source
ARCH:=-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS:=-g -Wall -O2 -mword-relocations -ffunction-sections $(ARCH) $(INCLUDE)
LDFLAGS:=-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS:=-lctru -lm
LIBDIRS:=$(CTRULIB)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT:=$(CURDIR)/$(TARGET)
export TOPDIR:=$(CURDIR)
export VPATH:=$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR:=$(CURDIR)/$(BUILD)
export CFILES:=$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export OFILES:=$(CFILES:.c=.o)
export INCLUDE:=$(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS:=$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# --- HOME-menu icon / banner metadata (edit these for your own release) ---
export APP_TITLE       := MIVF Player
export APP_DESCRIPTION := 3DS MIVF video player
export APP_AUTHOR      := MIVF
export APP_ICON        := $(TOPDIR)/meta/icon48.png

# Embed the SMDH (icon + title + author) into the .3dsx unless NO_SMDH=1.
ifeq ($(strip $(NO_SMDH)),)
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

.PHONY: all clean cia $(BUILD)
all: $(BUILD)
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

# Package an installable .cia. Requires makerom + bannertool in PATH
# (devkitPro/tools/bin). Both are third-party; see README for install links.
cia: all
	@command -v makerom >/dev/null 2>&1 || { echo "ERROR: 'makerom' not found. Install it (Project_CTR releases) into devkitPro/tools/bin."; exit 1; }
	@command -v bannertool >/dev/null 2>&1 || { echo "ERROR: 'bannertool' not found. Install it into devkitPro/tools/bin."; exit 1; }
	@bannertool makebanner -i $(TOPDIR)/meta/banner.png -a $(TOPDIR)/meta/banner.wav -o $(TOPDIR)/meta/banner.bnr
	@makerom -f cia -o $(TARGET).cia -rsf $(TOPDIR)/meta/app.rsf -elf $(OUTPUT).elf -icon $(OUTPUT).smdh -banner $(TOPDIR)/meta/banner.bnr -exefslogo
	@echo built ... $(TARGET).cia

clean:
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).map $(TARGET).smdh $(TARGET).cia $(TOPDIR)/meta/banner.bnr
else
DEPENDS:=$(OFILES:.o=.d)
$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
