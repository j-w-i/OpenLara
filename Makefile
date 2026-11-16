NANOOS_DIR ?= /Users/jwalr/nano-gles2/NanoOS
APP_NAME   := openlara
APP_USE_LIBC := 1

APP_CPPFLAGS := \
	-D_OS_NANO7 \
	-D_GAPI_GLES2 \
	-DUSE_ATLAS_RGBA16 \
	-DNO_SOUND \
	-DNO_VIDEO \
	-DGL2_BUFFER_POOL_BYTES=2621440u \
	-DGL2_MAX_BUFFERS=8 \
	-DGL2_MAX_TEXTURES=16 \
	-DGL2_TEXTURE_ARENA_BYTES=16777216u \
	-DGL2_MAX_TEXTURE_SIZE=1024 \
	-DGL2_MAX_FBO_SIZE=256 \
	-DGL2_TEXTURE_SLOT_BYTES=2097152u \
	-DGL2_ALIAS_BYTES=4096u \
	-DGL2_MAX_SHADERS=32 \
	-DGL2_MAX_PROGRAMS=32 \
	-DGL2_SHADER_SOURCE_BYTES=16384 \
	-DGL2_SCISSOR_IGNORE_DRAWS \
	-DGL2_VCACHE_WORDS=524288u

APP_CFLAGS := -Isrc -Iengine/src -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
APP_CXXFLAGS := -O2 -mfpu=vfpv3-d16 -mfloat-abi=softfp -ffp-contract=off -Isrc -Iengine/src -std=c++11 -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Wno-sign-compare -Wno-reorder -Wno-switch -Wno-missing-field-initializers -Wno-invalid-offsetof

APP_SRCS := \
	src/main.cpp \
	src/openlara_fs.c \
	src/nano_libc.c \
	engine/src/libs/tinf/tinflate.c

include $(NANOOS_DIR)/sdk/app.mk
