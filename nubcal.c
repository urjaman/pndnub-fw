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
#include <util/crc16.h>

#define DP(x) i2ch_dprint_P(PSTR(x))
#define DPS(x) i2ch_dprint(x)

static void dp_uint(uint8_t c, uint16_t w) {
	uint8_t buf[10];
	buf[0] = c;
	uchar2xstr(buf+1, w>>8);
	uchar2xstr(buf+3, w&0xFF);
	buf[5] = ' ';
	buf[6] = 0;
	DPS((char*)buf);
}

#define CAL_MAGIC 0xCA11

#define MIN_DEFL (16*16)
#define MAX_DEFL (96*16)

#define DOWN_COUNT 600

struct nub_cal_ee {
	uint16_t magic;
	uint16_t xp;
	uint16_t xn;
	uint16_t yp;
	uint16_t yn;
	uint16_t crc;
};

#define AXIS_COUNT 32

static uint8_t cal_locked = 0;
static struct nub_cal_ee cur_cal = { 0, MIN_DEFL, MIN_DEFL, MIN_DEFL, MIN_DEFL, 0 };
static uint16_t cal_down_counter[4] = {};
	
static void process_axis(uint8_t axis, uint16_t mag, uint16_t *cal, uint8_t maj_axis) {
	if (mag < MIN_DEFL) return;
	if (mag > MAX_DEFL) return;
	uint16_t c = *cal;
	if (mag > c) {
		cal_down_counter[axis] = 0;
		uint16_t diff = mag - c;
		if (diff < (c/AXIS_COUNT)) {
			return;
		}
		diff -= c/AXIS_COUNT;
		diff = diff / 4;
		if (!diff) diff = 1;
		*cal = c+diff;
		return;
	} else {
		if (maj_axis) {
			cal_down_counter[axis]++;
			if (cal_down_counter[axis] >= DOWN_COUNT) {
				*cal = c - c/AXIS_COUNT;
				cal_down_counter[axis] = 0;
			}
		}
	}
}

static uint16_t cal_math(uint16_t mag, uint16_t cal) {
	uint16_t v =((mag*(uint32_t)AXIS_COUNT)+(cal/2)) / cal;
	if (v > AXIS_COUNT) v = AXIS_COUNT;
	return v;
}

int prev_x=0, prev_y=0;

void nubcal_input(int x, int y)
{
	int xa = abs(x);
	int ya = abs(y);

	const uint8_t max_delta = 32;
	uint16_t magdelta = abs(x - prev_x) + abs(y - prev_y);
	// the deltas are so we prevent dynamic states from affecting the calibration
	if ((magdelta < max_delta)&&(!cal_locked)) {
		if (x > 0) process_axis(0, xa, &cur_cal.xp, xa>ya);
		if (x < 0) process_axis(1, xa, &cur_cal.xn, xa>ya);
		if (y > 0) process_axis(2, ya, &cur_cal.yp, ya>xa);
		if (y < 0) process_axis(3, ya, &cur_cal.yn, ya>xa);
	}
	prev_x = x;
	prev_y = y;
	
	int xc = 0;
	int yc = 0;
	if (x > 0) xc = cal_math(xa, cur_cal.xp);
	if (x < 0) xc = -cal_math(xa, cur_cal.xn);
	if (y > 0) yc = cal_math(ya, cur_cal.yp);
	if (y < 0) yc = -cal_math(ya, cur_cal.yn);
	
	dp_uint('X', x);
	dp_uint('Y', y);
	if (x > 0) dp_uint('C', cur_cal.xp);
	if (x < 0) dp_uint('C', cur_cal.xn);
	if (y > 0) dp_uint('U', cur_cal.yp);
	if (y < 0) dp_uint('U', cur_cal.yn);
	if (magdelta >= max_delta) dp_uint('D', magdelta);
	
	signed char result[4] = { xc, yc, xc, yc };
	i2ch_set_result(result, 0);
}

void nubcal_init(void)
{
	cal_locked = 0;
	struct nub_cal_ee eecal;
	eeprom_read_block(&eecal, (void*)0, sizeof(struct nub_cal_ee));
	if (eecal.magic != CAL_MAGIC) return;
	uint16_t eec_crc = eecal.crc;
	eecal.crc = 0;
	uint16_t crc = 0xFFFF;
	uint8_t *d = (uint8_t*)&eecal.magic;
	for (uint8_t i=0;i<sizeof(struct nub_cal_ee);i++) crc = _crc_ccitt_update(crc, d[i]);
	if (crc != eec_crc) return;
	cur_cal = eecal;
	cal_locked = 1;
}

void nubcal_store_cal(void)
{
	cur_cal.magic = CAL_MAGIC;
	cur_cal.crc = 0;
	uint16_t crc = 0xFFFF;
	uint8_t *d = (uint8_t*)&cur_cal.magic;
	for (uint8_t i=0;i<sizeof(struct nub_cal_ee);i++) crc = _crc_ccitt_update(crc, d[i]);
	cur_cal.crc = crc;
	eeprom_update_block(d, (void*)0, sizeof(struct nub_cal_ee));
	cal_locked = 1;
}

void nubcal_clear_stored_cal(void)
{
	eeprom_update_word((void*)0, 0xFFFF);
}

void nubcal_reset_cal(void)
{
	struct nub_cal_ee rst_cal = { 0, MIN_DEFL, MIN_DEFL, MIN_DEFL, MIN_DEFL, 0 };
	cal_locked = 0;
	cur_cal = rst_cal;
	for (uint8_t i=0;i<4;i++) cal_down_counter[i] = 0;
}

void nubcal_set_mode(int autocal)
{
	cal_locked = !autocal;
}

uint8_t nubcal_is_locked(void)
{
	return cal_locked;
}
