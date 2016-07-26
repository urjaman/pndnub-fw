/*
 * This file is part of the pndnub-fw project.
 *
 * Copyright (C) 2015,2016 Urja Rannikko <urjaman@gmail.com>
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

static void dp_hb(uint8_t byte) {
    uint8_t buf[3];
    uchar2xstr(buf,byte);
    DPS((char*)buf);
}

void main(void) __attribute__((noreturn,OS_main));
void main(void) {
	if (!(PIND & _BV(6))) PORTD &= ~_BV(6); // turn off the pullup on the address determination if its grounded.
	i2c_init();
	i2ch_init();
	nub_init();
	nubcal_init();
	sei();
	DP("Boot! ");
	uint8_t len = 0;
	for(;;) {
	    uint8_t *cmd = i2c_get_command(&len);
	    if (cmd) {
			if ((len==2)&&(cmd[0]==0x01)) { // These are ASCII key commands, simple, but should work :P
				switch (cmd[1]) {
					default:
						DP("Unimplemented CMD:");
						dp_hb(cmd[1]);
						DP(". ");
						break;
					case 'a':
					case 'A': // switch to autocal
						nubcal_set_mode(1);
						DP("Calibration: automatic. ");
						i2ch_cmd_reply(0x80); // thats our "yup."
						break;
					case 'm': // lock calibration / "manual mode"
					case 'M':
					case 'l':
					case 'L':
						nubcal_set_mode(0);
						DP("Calibration locked. ");
						i2ch_cmd_reply(0x80);
						break;
					case 'R':
					case 'r':
						nubcal_reset_cal();
						DP("Calibration reset. ");
						i2ch_cmd_reply(0x80);
					break;
					case 'C':
					case 'c':
						nubcal_clear_stored_cal();
						DP("EEPROM calibration cleared. ");
						i2ch_cmd_reply(0x80);
						break;
					case 'S':
					case 's':
						nubcal_store_cal();
						DP("Calibration locked and stored to EEPROM. ");
						i2ch_cmd_reply(0x80);
						break;
					case '?': {
						uint8_t lock = nubcal_is_locked();
						if (lock) DP("Calibration is locked. ");
						else DP("Calibration is not locked. ");
						i2ch_cmd_reply(0x80 | lock);
						}
						break;
				}
			} else {
				DP("Cmd: l=");
				dp_hb(len);
				DP(" d=");
				for (uint8_t i=0;i<len;i++) {
					dp_hb(cmd[i]);
					DP(" ");
				}
				DP(". ");
			}
	    }
	    i2ch_run();
		nub_run(); /* This sleeps */
	}
}

