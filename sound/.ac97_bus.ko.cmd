cmd_sound/ac97_bus.ko := ld -r -m elf_x86_64 -z noexecstack --build-id=sha1  -T scripts/module.lds -o sound/ac97_bus.ko sound/ac97_bus.o sound/ac97_bus.mod.o;  true
