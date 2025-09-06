KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

obj-m := drivers/spi/ssd1306-spi.o

DTS_SRC := dt-overlays/ssd1306-overlay.dts
DTB_OUT := dt-overlays/ssd1306-overlay.dtbo

all: module overlay

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

overlay:
	dtc -@ -I dts -O dtb -o $(DTB_OUT) $(DTS_SRC)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(DTB_OUT)
