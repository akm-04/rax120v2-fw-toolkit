EXECUTABLE = flash_img_obj_create

HAL_DIR := $(shell cd ../HAL; echo `pwd`/)

SOURCES = \
	${HAL_DIR}/al_flash_contents.c \
	flash_img_obj_create.c \

OBJS = $(patsubst %.c,obj_and_deps/%.o,$(patsubst %.S,obj_and_deps/%.o,$(SOURCES)))

LDFLAGS = \
	-g \

INCLUDES = \
	-I./ \
	-I${HAL_DIR}/ \

DEFINES = \

CFLAGS		= \
	-g \

all: mkdirs $(EXECUTABLE)

mkdirs:
	@mkdir -p obj_and_deps

$(EXECUTABLE) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)  

define object_rule
$2: $1
	@mkdir -p `dirname $2`
	$(CC) -MD $(INCLUDES) $(DEFINES) $(CFLAGS) -c -o $2 $1
endef

$(foreach source,$(SOURCES), $(eval $(call object_rule, $(source), $(patsubst %.c,obj_and_deps/%.o,$(patsubst %.S,obj_and_deps/%.o,$(source))))))

-include $(OBJS:%.o=%.d)

clean:
	rm -f $(EXECUTABLE) $(OBJS) $(OBJS:%.o=%.d)
