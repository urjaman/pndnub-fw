void     i2c_init(void);
uint8_t  i2c_set_txbuf(uint8_t *buf, uint8_t len);
uint8_t  i2c_getclear_txlen(void);
uint8_t *i2c_get_command(uint8_t *len);
