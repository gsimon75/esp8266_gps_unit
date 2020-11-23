#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := hello-world

CFLAGS += -Wno-pointer-sign

NVS_FILENAME := nvs.csv
NVS_OFFSET := 0x102000
NVS_SIZE := 0x3000
NVS_BIN := build/nvs.bin

$(NVS_BIN):		$(NVS_FILENAME)
				$(IDF_PATH)/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py --input $^ --output $@ --size $(NVS_SIZE)

ESPTOOL_ALL_FLASH_ARGS += $(NVS_OFFSET) $(NVS_BIN)
all_binaries: $(NVS_BIN)

include $(IDF_PATH)/make/project.mk

