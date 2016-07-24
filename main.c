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

#define DP(x) i2ch_dprint_P(PSTR(x))
#define DPS(x) i2ch_dprint(x)

void dp_hb(uint8_t byte) {
    uint8_t buf[3];
    uchar2xstr(buf,byte);
    DPS((char*)buf);
}

void jump_bootloader(void) __attribute__((noreturn));
void jump_bootloader(void)
{
    /* Take over from i2ch_run here. */
    uint8_t txbuf[1];
    txbuf[0] = 0x55;
    i2c_set_txbuf(txbuf,1);
    while (!i2c_getclear_txlen()) {
        /* ... wait for a read ... we no sleep yet... */
    }
    /* jump to boooot loader */
	void (*const btloader)(void) __attribute__((noreturn)) = (void*)(BTLOADERADDR>>1); // Make PM
	cli();
	btloader();
}

void main(void) __attribute__((noreturn,OS_main));
void main(void) {
	i2c_init();
	i2ch_init();
	sei();
	DP("Boot!\n");
	uint8_t len = 0;
	for(;;) {
	    uint8_t *cmd = i2c_get_command(&len);
	    i2ch_run();
	    if (cmd) {
	        DP("Cmd: l=");
	        dp_hb(len);
	        DP(" d=");
	        for (uint8_t i=0;i<len;i++) {
	            dp_hb(cmd[i]);
	            DP(" ");
	        }
	        DP("\n");
	        if (len>=1) {
	            switch (cmd[0]) {
	                case 0xAA:
	                    jump_bootloader();
	            }
	        }
	    }
	    /* no sleep, yet .. debug fw. */
	}
}

