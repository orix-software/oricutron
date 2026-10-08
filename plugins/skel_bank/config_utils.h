// Functions
SDL_bool config_load(char *config_file, Uint8 *bank, char *bank_file);
SDL_bool bank_load(char* fname, int size, Uint8 where[]);
SDL_bool read_config_string( char *buf, char *token, char *dest, Sint32 maxlen );

