all:
	${MAKE} -C kernel all
	${LD} -T kernel/vmlinux.lds kernel/*.o ../../init/*.o ../../lib/*.o ../../user/uapp.o -o ../../vmlinux 
	$(shell test -d boot || mkdir -p boot)
	${OBJCOPY} -O binary ../../vmlinux ./boot/Image
	${OBJDUMP} -S ../../vmlinux > ../../vmlinux.asm
	nm ../../vmlinux >  ../../System.map

clean:
	${MAKE} -C kernel clean
	$(shell test -d boot && rm -rf boot)
