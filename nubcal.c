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
#include "nubcal.h"

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

#define A_STEP (16)

#define DOWN_COUNT 600

struct nub_cal_ee {
	uint16_t magic;
	uint16_t xp;
	uint16_t xn;
	uint16_t yp;
	uint16_t yn;
	uint16_t crc;
};


struct nub_cal_ee stored_cal = { 0, MIN_DEFL, MIN_DEFL, MIN_DEFL, MIN_DEFL, 0 };

struct nub_cal_ee cur_cal = { 0, MIN_DEFL, MIN_DEFL, MIN_DEFL, MIN_DEFL, 0 };

static uint16_t cal_down_counter[4] = {};
static uint16_t axis_counter[4] =  {};
	
static void process_axis(uint8_t axis, uint16_t mag, uint16_t *cal, uint8_t maj_axis) {
	if (mag < MIN_DEFL) return;
	if (mag > MAX_DEFL) return;
	uint16_t c = *cal;
	axis_counter[axis]++;
	if (mag > c) {
		cal_down_counter[axis] = 0;
		uint16_t diff = mag - c;
		if (diff < (c/32)) {
			return;
		}
		diff -= c/32;
		diff = diff / 5;
		if (!diff) diff = 1;
		*cal = c+diff;
		return;
	} else {
		if (maj_axis) {
			cal_down_counter[axis]++;
			if (cal_down_counter[axis] >= DOWN_COUNT) {
				*cal = c - c/32;
				cal_down_counter[axis] = 0;
			}
		}
	}
}

static uint16_t cal_math(uint16_t mag, uint16_t cal) {
	uint16_t v =((mag*32UL)+(cal/2)) / cal;
	if (v > 32) v = 32;
	return v;
}

int prev_x=0, prev_y=0;

void nubcal_input(int x, int y)
{
	int xa = abs(x);
	int ya = abs(y);

	uint16_t magdelta = abs(x - prev_x) + abs(y - prev_y);
	// the deltas are so we prevent dynamic states from affecting the calibration
	if (magdelta < 32) {
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
	if (magdelta > 32) dp_uint('D', magdelta);
	
	signed char result[4] = { xc, yc, xc, yc };
	i2ch_set_result(result, 0);
}

void nubcal_store_cal(void)
{
	
}


void nubcal_set_mode(int autocal)
{
	
}