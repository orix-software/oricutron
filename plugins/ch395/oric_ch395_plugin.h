
struct ch395 * ch395_oric_init(void);
void ch395_oric_reset(struct ch395 *ch395);
void	ch395_oric_write(struct ch395 *ch395, Uint16 addr, Uint8 data);

unsigned char 	ch395_oric_read(struct  ch395 *ch395, Uint16 addr);

void ch395_oric_config(struct ch395 *ch395);
void ch395_oric_destroy(struct ch395 *ch395);
void ch395_oric_reset(struct ch395 *ch395);
unsigned char ch395_read_command_port(struct ch395 *ch395);
void ch395_write_command_port(struct ch395 *ch395, uint8_t command);
