

dl:
	$(MAKE) -f Makefile.dl

clean:
	$(MAKE) -C irx/usbserv clean
	rm -f usbserv.s usbhdfsd.s
	rm -f *.irx *.o ../*.o *.bin *.elf
