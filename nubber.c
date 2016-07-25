/*
 * This file is part of the pndnub-fw project.
 *
 * Copyright (C) 2015 Urja Rannikko <urjaman@gmail.com>
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

#include "main.h"
#include "i2c.h"
#include "i2chandler.h"
#include "lib.h"
#include "nubber.h"

#define DP(x) i2ch_dprint_P(PSTR(x))
#define DPS(x) i2ch_dprint(x)

static void dp_hb(uint8_t byte) {
    uint8_t buf[3];
    uchar2xstr(buf,byte);
    DPS((char*)buf);
}

static void dp_uint(uint8_t c, uint16_t w) {
	uint8_t buf[10];
	buf[0] = c;
	uchar2xstr(buf+1, w>>8);
	uchar2xstr(buf+3, w&0xFF);
	buf[5] = ' ';
	buf[6] = 0;
	DPS((char*)buf);
}

#define NUB_E _BV(0)
#define NUB_W _BV(1)
#define NUB_C _BV(2)
#define NUB_N _BV(6)
#define NUB_S _BV(7)

/* This is the programming port. Used to set their pullup enabled in the control sequence. */
#define UNUSED _BV(3) | _BV(4) | _BV(5)

static void adc_init(void) {
	ADMUX = _BV(REFS0);
	ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADFR) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}

static uint16_t adc_read(void) {
	while (!(ADCSRA & _BV(ADIF)));
	uint16_t d = ADC;
	ADCSRA = ADCSRA;
	return d;
}

static volatile uint8_t once = 1;

ISR(INT0_vect) {
	DP("IRQ!");
	GICR &= ~_BV(INT0);
	once = 0;
}

static void set_pattern(uint8_t p) {
	switch (p) {
		default: PORTB = NUB_E | NUB_W | NUB_C | NUB_N | NUB_S | UNUSED; break;
		case 1: PORTB = NUB_E | UNUSED;	break;
		case 2: PORTB = NUB_W | UNUSED; break;
		case 3: PORTB = NUB_N | UNUSED; break;
		case 4: PORTB = NUB_S | UNUSED; break;
		case 5: PORTB = NUB_C | UNUSED; break;
	}
}

static int read_pattern(uint8_t p) {
	set_pattern(p);
	_delay_ms(3);
	adc_read();
	return adc_read();
}

void nub_init(void) {
	DDRB = NUB_E | NUB_W | NUB_C | NUB_N | NUB_S;
	GICR |= _BV(INT0);
	adc_init();
}

void nub_run(void) {
	if (!(GICR & _BV(INT0))) {
		if (PIND & _BV(2)) {
			GICR |= _BV(INT0);
			once = 0;
		}
	}
	if (!once) {
		signed char output[4] = {};
		dp_uint('A', adc_read());
		int ew = read_pattern(1) - read_pattern(2);
		int ns = read_pattern(3) - read_pattern(4);
		set_pattern(0);
		dp_uint('X', ew);
		dp_uint('Y', ns);
		output[0] = ew / 4;
		output[1] = ns / 4;
		i2ch_set_result(output, 0);
	} else {
		_delay_ms(1);
	}
	once++;
}
	