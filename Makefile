
KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# Kernel module
obj-m := ssd1306-spi.o

DTS_SRC := dt-overlays/ssd1306-overlay.dts
DTB_OUT := dt-overlays/ssd1306-overlay.dtbo

all: module overlay

module:
	$(MAKE) -C $(KDIR) M=$(PWD)/drivers/spi modules

overlay:
	dtc -@ -I dts -O dtb -o $(DTB_OUT) $(DTS_SRC)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD)/drivers/spi clean
	rm -f $(DTB_OUT)
