#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

##############################################################################
# For reproducible builds
#
SOURCE_DATE_EPOCH = $(shell (git status -uno --porcelain | grep -q .) && date +%s || git log -n 1 --format="%ct")
#SOURCE_DATE_EPOCH = $(shell git log -n 1 --format="%ct")
ARFLAGS += D
CFLAGS += -DSOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH)
MAKEFLAGS += -j1
.NOTPARALLEL:
#
##############################################################################

PROJECT_NAME := hello-world
COMPONENT_EMBED_FILES :=  $(sort $(wildcard ${PROJECT_PATH}/static_data/*))

CFLAGS += -Wno-pointer-sign

NVS_FILENAME := nvs.csv
NVS_OFFSET := 0x102000
NVS_SIZE := 0x3000
NVS_BIN := build/nvs.bin

$(NVS_BIN):						$(NVS_FILENAME)
	$(IDF_PATH)/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py --input $^ --output $@ --size $(NVS_SIZE)

ESPTOOL_ALL_FLASH_ARGS += $(NVS_OFFSET) $(NVS_BIN)

all_binaries: $(NVS_BIN)

##############################################################################
# Single-location image (ota_0 only) start
#
build/$(PROJECT_NAME).desc:		build/$(PROJECT_NAME).bin
	./misc/gen_desc.py -i $< -o $@

upload_binaries:	build/$(PROJECT_NAME).desc build/$(PROJECT_NAME).bin
	#scp -C $^ pi@pi:/var/www/html/ota.wodeewa.com/out/

app:		upload_binaries
app-flash:	upload_binaries
#
##############################################################################


include $(IDF_PATH)/make/project.mk

##############################################################################
# Multi-location images (ota_N) start
#
ota_partition_indices = $(shell sed -rn "s/\#.*//; s/[ \t]//g; s/.*,ota_([0-9]*),.*/\1/p" "$(CONFIG_PARTITION_TABLE_FILENAME)")
get_partition_address = $(shell sed -rn "s/\#.*//; s/[ \t]//g; s/.*,$(1),([^,]*),.*/\1/p" "$(CONFIG_PARTITION_TABLE_FILENAME)")
get_partition_size = $(shell sed -rn "s/\#.*//; s/[ \t]//g; s/.*,$(1),[^,]*,([^,]*)/\1/p" "$(CONFIG_PARTITION_TABLE_FILENAME)")

build/$(PROJECT_NAME).ota_%.bin:
		rm -f ./build/esp8266/esp8266_out.ld
		export CFLAGS= && export CXXFLAGS= && make $(APP_BIN) APP_OFFSET=$(call get_partition_address,ota_$*) APP_SIZE=$(call get_partition_size,ota_$*)
		mv $(APP_BIN) $@
		rm -f ./build/esp8266/esp8266_out.ld

build/$(PROJECT_NAME).ota_%.desc:	build/$(PROJECT_NAME).ota_%.bin
		./misc/gen_desc.py -i $< -o $@

.PHONY:		multi-images
multi-images:	$(foreach i,$(ota_partition_indices),$(foreach suffix,bin desc,build/$(PROJECT_NAME).ota_$(i).$(suffix)))
		scp -C $^ pi@pi:/var/www/html/ota.wodeewa.com/out/
		cp build/$(PROJECT_NAME).ota_0.bin $(APP_BIN)
#
##############################################################################

