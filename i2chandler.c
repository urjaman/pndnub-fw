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

#define DBGOUT_MAX 160

struct i2c_output {
    signed char result[4]; // retro stuff
    uint8_t out_id; // output number
    uint8_t state; // what kind is this result?
    uint8_t cmd_reply; // ack/nack/a few bits of reply from commands that dont change state
    uint8_t debug_cnt; // how much dbgout there is?
    char dbgout[DBGOUT_MAX]; // atleast debug_cnt of stuff
} i2c_buffers[2];

static uint8_t out_bufid = 0;


void i2ch_init(void)
{
    i2c_set_txbuf((void*)&(i2c_buffers[0].result[0]), 8);
}

static void i2c_flip(void) {
    /* Hey, i made nb different from cb */
    struct i2c_output *nb = &(i2c_buffers[(out_bufid+1)&1]);
    if (nb->out_id == out_bufid) {
        nb->out_id = out_bufid+1;
    }
}

static void i2c_trypush(void) {
    struct i2c_output *nb = &(i2c_buffers[(out_bufid+1)&1]);
    struct i2c_output *cb = &(i2c_buffers[out_bufid&1]);
    if (nb->out_id != out_bufid) {
        uint8_t len = nb->debug_cnt + 8;
        if (!i2c_set_txbuf((void*)&(nb->result[0]),len)) {
            out_bufid = nb->out_id;
            memcpy(cb,nb,len);
        }
    }
}

void i2ch_run(void) {
    struct i2c_output *nb = &(i2c_buffers[(out_bufid+1)&1]);
    uint8_t l = i2c_getclear_txlen();
    uint8_t flip = 0;
    if (l>=7) {
        if (nb->cmd_reply) {
            nb->cmd_reply = 0;
            flip = 1;
        }
    }
    if (l>8) {
        uint8_t dbg_read = l-8;
        uint8_t dbg_cnt = nb->debug_cnt;
        if (dbg_read>=dbg_cnt) {
            nb->debug_cnt = 0;
        } else {
            uint8_t dbg_left = dbg_cnt - dbg_read;
            for (uint8_t i=0;i<dbg_left;i++) nb->dbgout[i] = nb->dbgout[i+dbg_read];
            nb->debug_cnt = dbg_left;
        }
        flip = 1;
    }
    if (flip) i2c_flip();
    i2c_trypush();
}

void i2ch_set_result(signed char *result, uint8_t state)
{
    struct i2c_output *nb = &(i2c_buffers[(out_bufid+1)&1]);
    memcpy(nb->result,result,4);
    nb->state = state;
    i2c_flip();
    i2c_trypush();
}

void i2ch_cmd_reply(uint8_t reply)
{
    struct i2c_output *nb = &(i2c_buffers[(out_bufid+1)&1]);
    nb->cmd_reply = reply;
    i2c_flip();
    i2c_trypush();
}

void i2ch_dprint_P(PGM_P dbg)
{
    struct i2c_output *nb = &(i2c_buffers[(out_bufid+1)&1]);
    uint16_t l = strlen_P(dbg);
    if ((l+nb->debug_cnt)>DBGOUT_MAX) l = DBGOUT_MAX - nb->debug_cnt;
    if (l) {
        memcpy_P(&(nb->dbgout[nb->debug_cnt]),dbg,l);
        nb->debug_cnt += l;
        i2c_flip();
        i2c_trypush();
    }
}

void i2ch_dprint(const char* dbg)
{
    struct i2c_output *nb = &(i2c_buffers[(out_bufid+1)&1]);
    uint16_t l = strlen(dbg);
    if ((l+nb->debug_cnt)>DBGOUT_MAX) l = DBGOUT_MAX - nb->debug_cnt;
    if (l) {
        memcpy(&(nb->dbgout[nb->debug_cnt]),dbg,l);
        nb->debug_cnt += l;
        i2c_flip();
        i2c_trypush();
    }
}
