mkdir build/
i686-elf-as src/boot.s -o build/boot.o
i686-elf-gcc -c src/kernel.c -o build/kernel.o -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -T src/linker.ld -o build/pong.bin -ffreestanding -O2 -nostdlib build/boot.o build/kernel.o -lgcc
qemu-system-i386 -kernel build/pong.bin -vga std

