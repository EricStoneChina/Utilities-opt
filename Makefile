#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
# Set toolchain location in an environment var for future use, this will change
# to use a system environment var in the future.
#---------------------------------------------------------------------------------
ifeq ($(strip $(FXCGSDK)),)
export FXCGSDK := $(abspath ../../)
endif

PRIZM_RULES := $(FXCGSDK)/common/prizm_rules
ifeq ($(wildcard $(PRIZM_RULES)),)
PRIZM_RULES := $(FXCGSDK)/toolchain/prizm_rules
endif
include $(PRIZM_RULES)


#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	src src/picoc src/picoc/cstdlib src/picoc/platform
DATA		:=	data  
INCLUDES	:=

#---------------------------------------------------------------------------------
# git version controlling mechanism
#---------------------------------------------------------------------------------
GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always)
GIT_TIMESTAMP = "$(shell git log --pretty=format:%aD -1)"
#---------------------------------------------------------------------------------
# options for code and add-in generation
#---------------------------------------------------------------------------------

MKG3AFLAGS := -r 02.01.0001 -n basic:Utilities -n internal:UTILS -i uns:../unselected.bmp -i sel:../selected.bmp
ifeq ($(wildcard $(FXCGSDK)/toolchain/prizm_rules),)
else
MKG3AFLAGS := -V 02.01.0001 -n basic:Utilities -n internal:UTILS -i uns:../unselected.bmp -i sel:../selected.bmp
endif

COMMONFLAGS = -Os -flto -fno-exceptions -fno-builtin-calloc -Wall $(MACHDEP) $(INCLUDE) -D__GIT_VERSION=\"$(GIT_VERSION)\" -D__GIT_TIMESTAMP=\"$(GIT_TIMESTAMP)\" $(CUSTOM_FLAGS)
CFLAGS	= -std=c99 $(COMMONFLAGS)
CXXFLAGS	=	-std=c++11 $(COMMONFLAGS) -fno-rtti

LINKER_SCRIPT := $(FXCGSDK)/common/prizm.ld
ifeq ($(wildcard $(LINKER_SCRIPT)),)
LINKER_SCRIPT := $(FXCGSDK)/toolchain/prizm.x
endif

LDFLAGS	= $(MACHDEP) -T$(LINKER_SCRIPT) -Os -flto -Wl,-static -Wl,-gc-sections

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=	 -lfxcg -lc -lgcc

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o)

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) -I$(LIBFXCG_INC)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					-L$(LIBFXCG_LIB)

export OUTPUT	:=	$(CURDIR)/$(TARGET)
.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
ifeq ($(OS),Windows_NT)
	@if not exist "$@" mkdir "$@"
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
else
	@mkdir -p "$@"
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
endif

#---------------------------------------------------------------------------------
export CYGWIN := nodosfilewarning
clean:
ifeq ($(OS),Windows_NT)
	@powershell.exe -NoProfile -NonInteractive -Command "if (Test-Path -LiteralPath '$(BUILD)') { Remove-Item -LiteralPath '$(BUILD)' -Recurse -Force }"
	@powershell.exe -NoProfile -NonInteractive -Command "if (Test-Path -LiteralPath '$(OUTPUT).bin') { Remove-Item -LiteralPath '$(OUTPUT).bin' -Force }"
	@powershell.exe -NoProfile -NonInteractive -Command "if (Test-Path -LiteralPath '$(OUTPUT).g3a') { Remove-Item -LiteralPath '$(OUTPUT).g3a' -Force }"
else
	$(RM) -fr $(BUILD) $(OUTPUT).bin $(OUTPUT).g3a
endif

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).g3a: $(OUTPUT).bin
$(OUTPUT).bin: $(OFILES)


-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
