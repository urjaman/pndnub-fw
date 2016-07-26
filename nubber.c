/*
 * This file is part of the pndnub-fw project.
 *
 * Copyright (C) 2016 Urja Rannikko <urjaman@gmail.com>
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
 */

#include "main.h"
#include "i2c.h"
#include "i2chandler.h"
#include "lib.h"
#include "nubber.h"
#include "nubcal.h"

#define DP(x) i2ch_dprint_P(PSTR(x))
#define DPS(x) i2ch_dprint(x)


#define NUB_E _BV(0)
#define NUB_W _BV(1)
#define NUB_C _BV(2)
#define NUB_N _BV(6)
#define NUB_S _BV(7)

/* This is the programming port. Used to set their pullup enabled in the control sequence. */
#define UNUSED _BV(3) | _BV(4) | _BV(5)


static volatile uint8_t adc_activate;
static void adc_init(void) {
	ADMUX = _BV(REFS0);
	/* Okay, so on a mega8 the F_CPU cant be divided runtime in software, but the lock bits can set
	 * the internal RC to 8, 4, 2 or 1 Mhz, and the ADC divisor is the only bit of this fw that really
	 * cares about F_CPU, we read the fuse bits to determine the appropriate ADC divisor for
	 * CLKadc = 62.5 kHz. */
	uint8_t clksel = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS) & 0xF;
	uint8_t prescaler;
	switch (clksel) {
		default: /* 4 or other, we assume 8Mhz */
			prescaler = _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // We assume worst case = high frequency :P
			break;
		case 1: /* 1Mhz */
			prescaler = _BV(ADPS2);
			break;
		case 2: /* 2Mhz */
			prescaler = _BV(ADPS2) | _BV(ADPS0);
			break;
		case 3: /* 4Mhz */
			prescaler = _BV(ADPS2) | _BV(ADPS1);
			break;
	}
	adc_activate = _BV(ADEN) | _BV(ADSC) | _BV(ADFR) | _BV(ADIF) | _BV(ADIE) | prescaler;
	ADCSRA = 0;
}

inline static void set_pattern(uint8_t p) {
	switch (p) {
		default: PORTB = NUB_E | NUB_W | NUB_C | NUB_N | NUB_S | UNUSED; break;
		case 1: PORTB = NUB_E | UNUSED;	break;
		case 2: PORTB = NUB_W | UNUSED; break;
		case 3: PORTB = NUB_N | UNUSED; break;
		case 4: PORTB = NUB_S | UNUSED; break;
		case 5: PORTB = NUB_C | UNUSED; break;
	}
}

ISR(INT0_vect) {
	GICR &= ~_BV(INT0);
	ADCSRA = adc_activate;
	set_pattern(1);
}

static uint8_t adc_state = 0;
static uint16_t adc_sum = 0;
#define SKIP_SAMPLES 6
#define SAMPLE_CNT 16

static volatile uint16_t adc_res[4];
static volatile uint8_t output = 0;

ISR(ADC_vect) {
	adc_state++;
	uint8_t scnt = adc_state&0x1F;
	if (scnt <= SKIP_SAMPLES)
		return;
	adc_sum += ADC;
	if (scnt >= SKIP_SAMPLES+SAMPLE_CNT) {
		uint8_t chan = adc_state >> 5;
		adc_res[chan++] = adc_sum;
		adc_sum = 0;
		if (chan>=4) {
			chan = 0;
			output = 1;
		}
		adc_state = chan << 5;
		set_pattern(chan+1);
	}
}


void nub_init(void) {
	DDRB = NUB_E | NUB_W | NUB_C | NUB_N | NUB_S;
	GICR |= _BV(INT0);
	adc_init();
}

static uint8_t stop_counter = 0;

void nub_run(void) {
	cli();
	if (!(GICR & _BV(INT0))) {
		if (output) {
			output = 0;
			sei();
			int ew = (adc_res[0]>>2) - (adc_res[1]>>2);
			int ns = (adc_res[2]>>2) - (adc_res[3]>>2);
			signed char result[2] = {};
			result[0] = ew / 16;
			result[1] = ns / 16;

			if ((!result[0])&&(!result[1])) {
				stop_counter++;
			} else {
				stop_counter = 0;
			}

			if (stop_counter >= 40) {
				cli();
				DDRD &= ~_BV(7); // IRQ off
				stop_counter = 0;
				set_pattern(0);
				adc_state = 0;
				adc_sum = 0;
				ADCSRA = 0;
				GICR |= _BV(INT0);
				sleepy_mode(1);
			} else {
				nubcal_input(ew, ns);
				DDRD |= _BV(7); // IRQ on (is on for the entire sequence AFAIK)
			}
		} else {
			sleepy_mode(0); // does sei()
		}
	} else {
		sleepy_mode(1); // does sei();
	}
}
	