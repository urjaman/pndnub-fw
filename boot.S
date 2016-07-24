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
#warning "RAMSTART not defined, overridden to 0x60"
#endif
#if RAMSTART != 0x60
#error "This code was written for ATmega8 with ramstart 0x60 specifically"
#endif
#if SPM_PAGESIZE > 256
#error Too big SPM pagesize for the protocol
#endif
Xlo = 26
Xhi = 27
Ylo = 28
Yhi = 29
Zlo = 30
Zhi = 31

#define TAG_CHAR 0x55
#define INIT_CMD 0xAA
#define EXIT_CMD 0xFF

.section .bootloader, "ax"
	.global boot
boot:
	cli
	/* Ports */
	clr r1
	out _SFR_IO_ADDR(DDRD), r1
	ldi r16, 0x7F
	out _SFR_IO_ADDR(PORTD), r16
	/* Pullup on PD6 used to determine address started now */
	out _SFR_IO_ADDR(DDRC), r1
	ldi r17, 0x3E
	out _SFR_IO_ADDR(PORTC), r17
	out _SFR_IO_ADDR(DDRB), r1
	ldi r18, 0xFF
	out _SFR_IO_ADDR(PORTB), r18
	/* We must setup stack before first rcall */
	ldi r21, hi8(RAMEND)
	ldi r20, lo8(RAMEND)
	out _SFR_IO_ADDR(SPH), r21
	out _SFR_IO_ADDR(SPL), r20
	/* Start configuring TWI */
	out _SFR_IO_ADDR(TWCR), r1 /* Clear TWCR before setting address (TWEN=0) */
	ldi r19, 0xcc /* 0x66 (=address base) shifted */
	sbis _SFR_IO_ADDR(PIND), 6 /* Value of PD6 is tested now & here. clocks = 8 => 1us => should be enough */
	ori r19, 0x02 /* ADDR=2, as it says in the schema ;p */
	out _SFR_IO_ADDR(TWAR), r19
	ldi r20, 0x44 /* TWEN + TWEA, we're live. */
	out _SFR_IO_ADDR(TWCR), r20
	/* You can jump here from app for entering the bootloader if you setup HW properly */
    .global boot_jmp_entry
boot_jmp_entry:
	/* Drive IRQ to say we're ready to have something done to us. */
	sbi _SFR_IO_ADDR(DDRD), 7
	ldi r16, RAMSTART /* This is done to make loading it to any other X/Y/Z a simple movw rX, r0 */
	mov r0, r16
	/* And thats the size optimization that makes this code RAMSTART < 0x100 specific */
	/* Provide a data of (4) zeros if somebody just reads */
	clr r24
	rcall i2c_act
	cpi r16, 1
	brne out
	cpi r19, INIT_CMD
	brne out
	/* Prepare the page size for them to read next. */
#if SPM_PAGESIZE == 256
	clr r24
#else
	ldi r24, SPM_PAGESIZE
#endif
    rjmp bootlp

clrstat:
    clr r24
bootlp:
	rcall i2c_act
	cpi r16, 0x81
	breq clrstat /* Okay they read the byte we wanted to say, clear what we have to say. */
	cpi r16, SPM_PAGESIZE+1 /* This I2C ABI is the reason why we dont support page size 128 now */
	brne out /* Not correct amount of data => out. */
	cpi r19, EXIT_CMD /* Still check if they give data + addr/cmd == exit */
	breq out

	mov Zlo, r19

#if SPM_PAGESIZE == 64
	mov Zhi, Zlo
	swap Zlo
	lsl Zlo
	lsl Zlo
	andi Zlo, 0xC0
	lsr Zhi
	lsr Zhi
//#elif SPM_PAGESIZE == 128
//	mov Zhi, Zlo
//	clr Zlo
//	lsr Zhi
//	ror Zlo
// #elif SPM_PAGESIZE == 256
//	mov Zhi, Zlo
//	clr Zlo
#else
#error unexpected SPM_PAGESIZE
#endif

	movw Xlo, Zlo // Save Z in X

	rcall wait_spm_ready
	ldi r16, 0x03 // erase
	out _SFR_IO_ADDR(SPMCR), r16
	spm
	rcall wait_spm_ready
	ldi r20, (SPM_PAGESIZE/2)
    movw Ylo, r0
	ldi r16, 0x01
pageload:
	ld r0, Y+
	ld r1, Y+
	out _SFR_IO_ADDR(SPMCR), r16
	spm
	adiw r30, 2
	dec r20
	brne pageload
	movw r30, r26 // Restore Z from X
	ldi r16, 0x05
	out _SFR_IO_ADDR(SPMCR), r16
	spm
	/* Load the TAG that we did something to be sent as status next if read. */
	ldi r24, TAG_CHAR
	rjmp bootlp
out:
    /* Unset IRQ */
	cbi _SFR_IO_ADDR(DDRD), 7
	rcall wait_spm_ready
	ldi r16, 0x11
	out _SFR_IO_ADDR(SPMCR), r16
	spm
	rcall wait_spm_ready
	rjmp entry
.set entry,0

wait_spm_ready:
	in r16, _SFR_IO_ADDR(SPMCR)
	sbrc r16, SPMEN
	rjmp wait_spm_ready
	ret

i2c_act:
    ldi r18, 0xC4 /* This is the value we mostly put into TWCR */
    rcall i2c_wait
i2c_other: /* Come here to handle not-expected states. */
    clr 16 /* Clear R16 in preparation for RX or TX */
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
0:
    out _SFR_IO_ADDR(TWDR), r24
    out _SFR_IO_ADDR(TWCR), r18
    rcall i2c_wait
    inc r16 /* In all returning cases this is valid. */
    cpi r17, 0xB8
    breq 0b /* Just send one more byte */
    cpi r17, 0xC0
    breq 1f
    rjmp i2c_other
1:
    out _SFR_IO_ADDR(TWCR), r18
    ori r16, 0x80 /* Indicate TX and amount */
    ret

i2c_rx:
    movw Ylo, r0
0:  out _SFR_IO_ADDR(TWCR), r18
    rcall i2c_wait
    cpi r17, 0x80
    brne 1f
    /* Got byte */
    inc r16
    sbrc r16, 7 /* Overflow write check, we just disengage from the transaction and ignore it if >=128 bytes rcvd. */
    rjmp i2c_buserr
    in r19, _SFR_IO_ADDR(TWDR)
    st Y+, r19
    rjmp 0b

1:  cpi r17, 0xA0
    brne i2c_other
    out _SFR_IO_ADDR(TWCR), r18
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
