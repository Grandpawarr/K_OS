dd if=../code/out/mbr.bin of=./60mb.img bs=512 count=1 conv=notrunc
dd if=../code/out/loader.bin of=./60mb.img bs=512 count=4 seek=1 conv=notrunc
dd if=../code/out/kernel.bin of=./60mb.img bs=512 count=200 seek=5 conv=notrunc

