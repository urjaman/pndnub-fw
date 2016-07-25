/*
 * This file is part of the pndnub-fw project.
 *
 * Copyright (C) 2010,2011,2015 Urja Rannikko <urjaman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <avr/io.h>
#ifndef RAMSTART
#define RAMSTART 0x60
#warning "RAMSTART not defined, set to 0x60"
#endif

Xlo = 26
Xhi = 27
Ylo = 28
Yhi = 29
Zlo = 30
Zhi = 31

/* Commands: */
/* 0xB0 : PAGESIZE : Query Pagesize / Bootloader Check */
/* ADDRH ADDRL 0xB1 : 0xB1 : Set address */
/* n*2 * data 0xB2 : 0xB2 : Write Flash */
/* LEN 0xB3 : 0xB3 LEN*data : Read Flash */
/* 0xXX * n 0xB4-or-other : -- : Exit Bootloader */

zeroreg = Yhi
acki2c = 21
i2cdat = 22
adl = 24
adh = 25
dlen = 23
bootflags = 2
cmd = 20

.section .bootloader, "ax"
	.global boot
boot:
	/* Ports */
	ldi r16, 0x7F
	out _SFR_IO_ADDR(PORTD), r16
	/* Pullup on PD6 used to determine address started now */
	ldi r17, 0x3E
	out _SFR_IO_ADDR(PORTC), r17
	ldi r18, 0xFF
	out _SFR_IO_ADDR(PORTB), r18
	/* We must setup _a stack_ before first rcall ;) */
	out _SFR_IO_ADDR(SPL), r18
	/* Start configuring TWI */
	clr zeroreg
	out _SFR_IO_ADDR(TWCR), zeroreg /* Clear TWCR before setting address (TWEN=0) */

	in bootflags, _SFR_IO_ADDR(MCUCSR)
	out _SFR_IO_ADDR(MCUCSR), zeroreg

	ldi r19, 0xcc /* 0x66 (=address base) shifted */
	sbis _SFR_IO_ADDR(PIND), 6 /* Value of PD6 is tested now & here. clocks = 8 => 1us => should be enough */
	ori r19, 0x02 /* ADDR=2, as it says in the schema ;p */
	out _SFR_IO_ADDR(TWAR), r19

	ldi acki2c, 0xC4 /* TWEN + TWEA (+ useless INT clear), we're live. */
	out _SFR_IO_ADDR(TWCR), acki2c

	ldi r17, _BV(WDE) | _BV(WDP2) | _BV(WDP1) | _BV(WDP0)
	rcall wdt_cfg

	sbrc bootflags, WDRF
	rjmp out

clrstat:
    clr i2cdat
	ldi Ylo, RAMSTART
	std Y+0, zeroreg
	std Y+1, zeroreg
	std Y+2, zeroreg
bootlp:
	rcall i2c_act
	brts clrstat // a read
	wdr
	mov dlen, Ylo
	subi dlen, RAMSTART
	lsr dlen
	ldi Ylo, RAMSTART
	mov i2cdat, cmd // Ack is the command by default ;)
	movw Zlo, adl /* Prepare the address for read or write. */

	cpi cmd, 0xB0 // Query pagesize
	brne 1f
		ldi i2cdat, SPM_PAGESIZE
		rjmp bootlp
	1:cpi cmd, 0xB1 // Set addr
	brne 1f
		ldd adh, Y+0
		ldd adl, Y+1
		rjmp bootlp
	1: cpi cmd, 0xB3 
	brne 1f
		ldd dlen, Y+0
		rcall init_lpm
		0: lpm r16, Z+
		st Y+, r16
		dec dlen
		brne 0b
		rjmp bootlp
	1: cpi cmd, 0xB2
	brne out
	
	ldi r16, 0x03 // erase
	rcall spm_op
	ldi r16, 0x01
0:
	ld r0, Y+
	ld r1, Y+
	out _SFR_IO_ADDR(SPMCR), r16
	spm
	adiw Zlo, 2
	dec dlen
	brne 0b
	movw Zlo, adl // Restore Z from 25:24
	ldi r16, 0x05
	rcall spm_op
	rjmp bootlp
out:
	ldi r17, 0
	rcall wdt_cfg
	rcall init_lpm
	rjmp entry
.set entry,0

init_lpm:
	ldi r16, 0x11

spm_op:
	out _SFR_IO_ADDR(SPMCR), r16
	spm

wait_spm_ready:
	in r16, _SFR_IO_ADDR(SPMCR)
	sbrc r16, SPMEN
	rjmp wait_spm_ready
	ret


i2c_act:
    rcall i2c_wait
i2c_other: /* Come here to handle not-expected states. */
	set
	ldi Ylo, RAMSTART /* Prepare for data transfer. */
	cpi r17, 0x60
    breq i2c_rx
    cpi r17, 0xA8
    breq i2c_tx
i2c_buserr:
    /* Bus error or unhandled sequence, just reset slave and wait for more stuff */
    ldi r16, 0xD4 // TWINT+TWEA+TWSTO+TWEN
    out _SFR_IO_ADDR(TWCR), r16
    rjmp i2c_act

i2c_tx:
0:	out _SFR_IO_ADDR(TWDR), i2cdat
    out _SFR_IO_ADDR(TWCR), acki2c
	ld i2cdat, Y+
	cpi Ylo, RAMSTART+128
	breq i2c_buserr
    rcall i2c_wait
    cpi r17, 0xB8
    breq 0b /* Just send one more byte */
    cpi r17, 0xC0
    breq 2f
    rjmp i2c_other

i2c_rx:
0:	out _SFR_IO_ADDR(TWCR), acki2c
	rcall i2c_wait
	cpi r17, 0x80
	brne 1f
	clt	/* These are placed in this way to make a zero-len write seem like a zero-len read, and ignored. */
	in cmd, _SFR_IO_ADDR(TWDR)
	st Y+, cmd
	cpi Ylo, RAMSTART+128 /* Overflow check*/
	breq i2c_buserr
	rjmp 0b

1:  cpi r17, 0xA0
	brne i2c_other
2:	out _SFR_IO_ADDR(TWCR), acki2c
	ret

i2c_wait:
    /* Wait for TWINT */
    in r17, _SFR_IO_ADDR(TWCR)
    sbrs r17, TWINT
    rjmp i2c_wait
    /* Read TWSR because thats useful always */
    in r17, _SFR_IO_ADDR(TWSR)
    andi r17, 0xF8
    ret

wdt_cfg:
	in r16, _SFR_IO_ADDR(WDTCR)
	ori r16, _BV(WDCE) | _BV(WDE)
	out _SFR_IO_ADDR(WDTCR), r16
	out _SFR_IO_ADDR(WDTCR), r17
	ret