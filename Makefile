##
## This file is part of the pndnub-fw project.
##
## Copyright (C) 2015 Urja Rannikko <urjaman@gmail.com>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

PROJECT=pndnub-fw
DEPS=main.h i2c.h i2chandler.h Makefile lib.h
SOURCES=main.c i2c.c i2chandler.c lib.c nubber.c
CC=avr-gcc
LD=avr-ld
OBJCOPY=avr-objcopy
MMCU=atmega8
BTLOADERADDR=0x1F00
SERIAL_DEV=/dev/ttyUSB0

#AVRBINDIR=/usr/avr/bin/
#AVRDUDECMD=avrdude -p m644p -c dt006 -E noreset
# If using avr-gcc < 4.6.0, replace -flto with -combine
CFLAGS=-mmcu=$(MMCU) -Os -Wl,--relax -fno-inline-small-functions -frename-registers -g -Wall -W -pipe -flto -fwhole-program -std=gnu99 -Wno-main

.PHONY: asm clean bootloader blj-program objdump objdumpS size

all: $(PROJECT).out


$(PROJECT).hex: $(PROJECT).out
	$(AVRBINDIR)$(OBJCOPY) -j .text -j .data -O ihex $(PROJECT).out $(PROJECT).hex

$(PROJECT).bin: $(PROJECT).out
	$(AVRBINDIR)$(OBJCOPY) -j .text -j .data -O binary $(PROJECT).out $(PROJECT).bin
 
$(PROJECT).out: $(SOURCES) $(DEPS)
	$(AVRBINDIR)$(CC) -DBTLOADERADDR=$(BTLOADERADDR) $(CFLAGS) -I./ -o $(PROJECT).out $(SOURCES)
	$(AVRBINDIR)avr-size $(PROJECT).out
	
asm: $(SOURCES) $(DEPS)
	$(AVRBINDIR)$(CC) $(CFLAGS) -S  -I./ -o $(PROJECT).s $(SOURCES)
	

#program: $(PROJECT).hex
#	$(AVRBINDIR)$(AVRDUDECMD) -U flash:w:$(PROJECT).hex


clean:
	rm -f $(PROJECT).bin
	rm -f $(PROJECT).out
	rm -f $(PROJECT).hex
	rm -f $(PROJECT).s
	rm -f *.o
	rm -f boot.out boot.hex
	rm -f serialprogrammer

brpgm: $(PROJECT).hex
	avrdude -c arduino -b 115200 -p m8 -P /dev/ttyUSB2 -U flash:w:$(PROJECT).hex


blpgm: boot.hex
	avrdude -p m8 -c avrispmkII -P usb -U flash:w:boot.hex

bootloader: boot.hex

boot.hex: boot.out
	$(AVRBINDIR)$(OBJCOPY) -j .bootloader -O ihex boot.out boot.hex

boot.out:  boot.S
	$(AVRBINDIR)$(CC) -mmcu=$(MMCU) -c -o boot.o boot.S
	$(AVRBINDIR)$(LD) --section-start=.bootloader=$(BTLOADERADDR) -o boot.out boot.o


blj-program: $(PROJECT).bin serialprogrammer
	./serialprogrammer --bljump=1500000 $(PROJECT).bin $(SERIAL_DEV)

serialprogrammer: serialprogrammer.c
	gcc -W -Wall -Os -o serialprogrammer serialprogrammer.c

objdumpS: $(PROJECT).out
	$(AVRBINDIR)avr-objdump -xdCS $(PROJECT).out | less

objdump: $(PROJECT).out
	$(AVRBINDIR)avr-objdump -xdC $(PROJECT).out | less


