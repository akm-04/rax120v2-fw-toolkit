HAL_TOP=./

-include ./file_list.mk

#sources of the HAL samples
HAL_SAMPLE_SOURCES = \
	$(HAL_TOP)samples/dummy_plat.c \
	$(HAL_TOP)samples/crypto.c \
	$(HAL_TOP)samples/msg_ipc.c \
	$(HAL_TOP)samples/udma_fast.c \
	$(HAL_TOP)samples/eth.c \

HAL_EX_SAMPLE_SOURCES = \

ifeq ($(AL_DEV_ID), )
OUTPUT = output/alpine_v1_m0/
HAL_PLATFORM_INCLUDE_PATH = $(HAL_PLATFORM_INCLUDE_PATH_ALPINE_V1)
else
ifeq ($(AL_DEV_ID), AL_DEV_ID_ALPINE_V1)
ifeq ($(AL_DEV_REV_ID), 1)
OUTPUT = output/alpine_v1_a0/
HAL_PLATFORM_INCLUDE_PATH = $(HAL_PLATFORM_INCLUDE_PATH_ALPINE_V1)
else
OUTPUT = output/alpine_v1_m0/
HAL_PLATFORM_INCLUDE_PATH = $(HAL_PLATFORM_INCLUDE_PATH_ALPINE_V1)
endif
else
OUTPUT = output/alpine_v2/
HAL_PLATFORM_INCLUDE_PATH = $(HAL_PLATFORM_INCLUDE_PATH_ALPINE_V2)
HAL_SAMPLE_SOURCES += $(HAL_TOP)samples/tdm.c
endif
endif

DEFINES = \
	-DAL_DDR_ERR_PRINTOUT_DISABLED=0 \
	-DAL_DDR_ASSERT_DISABLED=0 \
	-DAL_DDR_VERIF_SHORT_TIME=1 \
	-DAL_DEV_ID_ALPINE_V1=0 \
	-DAL_DEV_ID_ALPINE_V2=1 \

ifneq ($(AL_DEV_ID), )
	DEFINES += -DAL_DEV_ID=$(AL_DEV_ID)
endif
ifneq ($(AL_DEV_REV_ID), )
	DEFINES += -DAL_DEV_REV_ID=$(AL_DEV_REV_ID)
endif

ifneq ($(AL_ETH_SUPPORT_DDP), )
	DEFINES += -DAL_ETH_SUPPORT_DDP
endif

CC ?=gcc
 override CFLAGS+=$(IDIR) $(DEFINES) -Wextra -Wall -Werror

SOURCES := \
	$(HAL_DRIVER_SOURCES) \
	$(HAL_INIT_SOURCES_GENERIC) \
	$(HAL_SAMPLE_SOURCES) \

ifneq ($(AL_HAL_EX), )
SOURCES += \
	$(HAL_EX_DRIVER_SOURCES) \
	$(HAL_EX_INIT_SOURCES_GENERIC) \
	$(HAL_EX_SAMPLE_SOURCES)
endif

IDIR := \
	-I$(HAL_TOP)/include/plat_api/sample \
	$(HAL_USER_INCLUDE_PATH) \
	$(HAL_DRIVER_INCLUDE_PATH) \
	$(HAL_INIT_INCLUDE_PATH) \
	$(HAL_PLATFORM_INCLUDE_PATH) \

ifneq ($(AL_HAL_EX), )
IDIR += \
	$(HAL_EX_USER_INCLUDE_PATH) \
	$(HAL_EX_DRIVER_INCLUDE_PATH) \
	$(HAL_EX_INIT_INCLUDE_PATH)
endif

objs = $(patsubst %.c,$(OUTPUT)/%.o,$(patsubst %.S,$(OUTPUT)/%.o,$(SOURCES)))

all: $(OUTPUT)/hal.lib

$(OUTPUT)/hal.lib: $(objs)
	@echo "Linking..."
	@$(CC) $(objs) -o $@

# pull in dependency info for *existing* .o files
-include $(objs:%.o=%.d)

define object_rule
$2: $1
	@mkdir -p `dirname $2`
	@echo Compiling $1...
	@$(CC) -MD -c -o $2 $1 $(CFLAGS)
endef

$(foreach source,$(SOURCES), $(eval $(call object_rule, $(source), $(patsubst %.c,$(OUTPUT)/%.o,$(patsubst %.S,$(OUTPUT)/%.o,$(source))))))

.PHONY: clean doc

clean:
	rm -rf $(OUTPUT)
doc:
	@sed -i -re 's/(^.*@code) \*\//\1/' samples/*
	@(cat ./doc/Doxyfile ; echo "PROJECT_NUMBER=`cat version.txt`") | doxygen -
	@sed -i -re 's/(^.*@code)/\1 \*\//' samples/*

cscope:
	git ls-tree -r HEAD --name-only  > cscope.files
	cscope -b -q -k
