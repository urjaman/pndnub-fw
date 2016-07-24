void i2ch_init(void);
void i2ch_run(void);
void i2ch_set_result(signed char *result, uint8_t state);
void i2ch_cmd_reply(uint8_t reply);
void i2ch_dprint_P(PGM_P dbg);
void i2ch_dprint(const char* dbg);
