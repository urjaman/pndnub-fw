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

void uint2str(uint8_t *buf, uint16_t val);
void uchar2str(uint8_t *buf, uint8_t val);
void uchar2xstr(uint8_t *buf, uint8_t val);
uint8_t str2uchar(uint8_t *buf);
uint8_t xstr2uchar(uint8_t *buf);
uint32_t astr2luint(uint8_t *buf);
void uint2xstr(uint8_t *buf, uint16_t val);
uint32_t astr2luint(uint8_t *buf);
void luint2str(uint8_t *buf, uint32_t val);
void luint2xstr(uint8_t*buf, uint32_t val);
