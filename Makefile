NANOOS_DIR ?= /Users/jwalr/nano-gles2/NanoOS
APP_NAME   := openlara
APP_USE_LIBC := 1

APP_CPPFLAGS := \
	-D_OS_NANO7 \
	-D_GAPI_GLES2 \
	-DUSE_ATLAS_RGBA16 \
	-DNO_SOUND \
	-DNO_VIDEO \
	-DGL2_BUFFER_BYTES=65536 \
	-DGL2_MAX_BUFFERS=8 \
	-DGL2_MAX_TEXTURES=16 \
	-DGL2_MAX_TEXTURE_SIZE=512 \
	-DGL2_TEXTURE_SLOT_BYTES=131072u \
	-DGL2_MAX_SHADERS=16 \
	-DGL2_MAX_PROGRAMS=8 \
	-DGL2_SHADER_SOURCE_BYTES=16384

APP_CFLAGS := -Isrc -Iengine/src -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
APP_CXXFLAGS := -Isrc -Iengine/src -std=c++11 -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Wno-sign-compare -Wno-reorder -Wno-switch -Wno-missing-field-initializers -Wno-invalid-offsetof

APP_SRCS := \
	src/main.cpp \
	src/openlara_fs.c \
	src/nano_libc.c \
	engine/src/libs/tinf/tinflate.c

include $(NANOOS_DIR)/sdk/app.mk
