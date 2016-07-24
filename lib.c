/*
 * Copyright (C) 2013 Urja Rannikko <urjaman@gmail.com>
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
#include "lib.h"


void luint2str(uint8_t *buf, uint32_t val)
{
	ultoa(val,(char*)buf,10);
}


void uint2str(uint8_t *buf, uint16_t val)
{
	luint2str(buf,(uint32_t)val);
}


void uchar2str(uint8_t *buf, uint8_t val)
{
	uint2str(buf,(uint16_t)val);
}


static uint8_t hextab_func(uint8_t offset) {
	offset |= 0x30;
	if (offset > 0x39) offset += 7;
	return offset;
}


void uchar2xstr(uint8_t *buf,uint8_t val)
{
	uint8_t offset;
	offset = ((val>>4)&0x0F);
	buf[0] = hextab_func(offset);
	offset = (val&0x0F);
	buf[1] = hextab_func(offset);
	buf[2] = 0;
}


uint8_t str2uchar(uint8_t *buf)
{
	uint8_t rv;
	for (rv=0;*buf;buf++) {
		if ((*buf >= '0')||(*buf <= '9')) {
			rv *= 10;
			rv = rv + (*buf &0x0F);
		}
	}
	return rv;
}


static uint8_t reverse_hextab(uint8_t hexchar) {
	if (hexchar > 0x39) hexchar = hexchar - 7;
	hexchar &= 0x0F;
	return hexchar;
}


static uint8_t isvalid(uint8_t c, uint8_t base)
{
	if (base == 16) {
		if ((c > 'F') || (c < '0')) return 0;
		if ((c > '9') && (c < 'A')) return 0;
	} else {
		if ((c > '9') || (c < '0')) return 0;
	}
	return 1;
}


uint32_t astr2luint(uint8_t *buf)
{
	uint8_t i;
	uint8_t len;
	uint8_t base=10;
	uint32_t rv;
	strupr((char*)buf);
	len = strlen((char*)buf);
	if (buf[len-1] == 'H') base=16;

	rv = 0;
	for(i=0;i<len;i++) {
		if (!(isvalid(buf[i],base))) continue;
		rv = rv * base;
		rv += reverse_hextab(buf[i]); // RV HEXTAB works also for base 10
	}
	if (buf[0] == '~') rv = ~rv;
	return rv;
}


uint8_t xstr2uchar(uint8_t *buf)
{
	uint8_t rv;
	rv = (reverse_hextab(*buf)<<4);
	buf++;
	rv |= reverse_hextab(*buf);
	return rv;
}


void luint2xstr(uint8_t *buf, uint32_t val)
{
	ultoa(val,(char*)buf,16);
	strupr((char*)buf); // i dont like "aaaah"...
}


void uint2xstr(uint8_t *buf,uint16_t val)
{
	luint2xstr(buf,(uint32_t)val);
}


