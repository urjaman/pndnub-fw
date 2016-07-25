
#include "main.h"
#include "i2c.h"
#include <util/twi.h>

/* I heard this is from the kernel, but seemed useful here. */
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

#define I2C_MAX_CMD 4
#define I2C_CMDBUFDEP 2

static uint8_t i2c_rx_buf[I2C_CMDBUFDEP][I2C_MAX_CMD];
static uint8_t i2c_rx_len[I2C_CMDBUFDEP];

static uint8_t i2c_rx_bpw = 0;
static uint8_t i2c_rx_bpr = 0;
static uint8_t i2c_rx_bufoff = 0;

static uint8_t *i2c_tx_buf=0;
static uint8_t i2c_tx_buflen = 0;
static uint8_t i2c_tx_maxout = 0;
static uint8_t i2c_tx_outoff = 0;


#if I2C_CMDBUFDEP > 2
#error "Not implemented"
#endif

void i2c_init(void)
{
    /* Assume: bootloader already set address and stuff. */
    /* We just enable interrupts for using our vector. */
    TWCR = _BV(TWEA) | _BV(TWEN) | _BV(TWIE);
}

static volatile uint8_t i2c_busy = 0;

ISR(TWI_vect)
{
    uint8_t twst = TWSR & 0xF8;
    uint8_t tmp;

    switch (twst) {
        default: /* Default handling of unknown/unhandled states is to just forget about it. */
            TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWSTO) | _BV(TWIE);
			i2c_busy = 0;
            break;
        case TW_ST_SLA_ACK:
        case TW_ST_ARB_LOST_SLA_ACK:
            if (i2c_tx_buf) {
                TWDR = i2c_tx_buf[0];
            } else {
                TWDR = 0;
            }
            TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);
            i2c_tx_outoff = 1;
			i2c_busy = 1;
            break;

        case TW_ST_DATA_ACK:
            if (i2c_tx_buf && (i2c_tx_outoff < i2c_tx_buflen)) {
                TWDR = i2c_tx_buf[i2c_tx_outoff];
            } else {
                TWDR = 0;
            }
            TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);
            i2c_tx_outoff++;
            break;

        case TW_ST_DATA_NACK:
            TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);
            if (i2c_tx_maxout < i2c_tx_outoff)
                    i2c_tx_maxout = i2c_tx_outoff;
            i2c_tx_outoff = 0;
			i2c_busy = 0;
            break;

        case TW_SR_SLA_ACK:
        case TW_SR_ARB_LOST_SLA_ACK:
            TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);
			i2c_busy = 1;
            i2c_rx_bufoff = 0;
            break;

        case TW_SR_DATA_ACK:
            tmp = TWDR;
            TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);
            i2c_rx_buf[i2c_rx_bpw][i2c_rx_bufoff++] = tmp;
            break;

        case TW_SR_STOP:
            TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);
            i2c_rx_len[i2c_rx_bpw] = i2c_rx_bufoff;
            i2c_rx_bpw ^= 1;
			i2c_busy = 0;
            break;
    }
}
 

uint8_t i2c_set_txbuf(uint8_t *buf, uint8_t len)
{
    uint8_t sr = SREG;
    cli();
    if (ACCESS_ONCE(i2c_tx_outoff)) { 
        /* We do not allow change of buffer under tx */
        SREG = sr;
        return 1;
    }
    i2c_tx_buf = buf;
    i2c_tx_buflen = len;
    i2c_tx_maxout = 0; // Just to keep it linked to the set buf.
    SREG = sr;
    return 0;
}

uint8_t i2c_getclear_txlen(void)
{
    uint8_t sr = SREG;
    cli();
    uint8_t r = ACCESS_ONCE(i2c_tx_maxout);
    i2c_tx_maxout = 0;
    SREG = sr;
    return r;
}

uint8_t *i2c_get_command(uint8_t *len) {
    uint8_t bpr = i2c_rx_bpr;
    if (bpr != ACCESS_ONCE(i2c_rx_bpw)) {
        *len = i2c_rx_len[bpr];
        i2c_rx_bpr = bpr^1;
        return i2c_rx_buf[bpr];
    }
    return NULL;
}


void sleepy_mode(uint8_t deepok) {
	cli();
	if (i2c_busy) deepok = 0;
	set_sleep_mode(deepok?SLEEP_MODE_PWR_DOWN:SLEEP_MODE_IDLE);
	sleep_enable();
	sei();
	sleep_cpu();
	sleep_disable();
}
