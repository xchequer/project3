cmd_drivers/hid/usbhid/usbhid.o := ld -m elf_x86_64 -z noexecstack   -r -o drivers/hid/usbhid/usbhid.o drivers/hid/usbhid/hid-core.o drivers/hid/usbhid/hiddev.o drivers/hid/usbhid/hid-pidff.o
