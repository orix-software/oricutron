/*
#define PERIPH_NAME_LEN 10

struct PLUGIN {
    char name[PERIPH_NAME_LEN+1];
    Uint16 default_addr;
    Uint16 size;
    Uint16 type;
    SDL_bool (*addresses)(unsigned int instance,  Uint16 offset):
    unsigned int (*create)(struct machine *oric);
    SDL_bool (*shutdown)(struct machine *oric, unsigned int instance);

    SDL_bool (*reset)(struct machine *oric, unsigned int instance);
    Uint8 (*read)(struct machine *oric, SDL_bool fbank, unsigned int instance, Uint16 addr, SDL_bool fexec);
    SDL_bool (*write)(struct machine *oric, SDL_bool fbank, unsigned int instance, Uint16 addr, Uint8 data);

    void (*mon_update)(struct textzone *tz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid);
    void (*mon_store_state)(struct machine *oric, unsigned int instance);
};
*/
#include "../../plugin.h"

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool periph_add(struct machine *oric, struct PLUGIN *plugin, char *name, Uint16 addr_start, SDL_bool enable);

SDL_bool periph_del(char *name);

SDL_bool periph_enable(char *name);

SDL_bool periph_disable(char *name);

SDL_bool periph_init_by_name(struct machine *oric, char *name);

SDL_bool periph_init_all(struct machine *oric);

Uint8 device_read(struct machine *oric, Uint16 addr); /* */

SDL_bool device_write(struct machine *oric, Uint16 addr, Uint8 data); /* */

SDL_bool device_printer(Uint8 data, SDL_bool rw);

int periph_find_by_name(char *name);

int periph_find_by_addr(struct machine *oric, Uint16 addr);

SDL_bool device_present(struct machine *oric, Uint16 addr); /* */
SDL_bool device_enabled_by_id(int id); /* */
SDL_bool mon_device_enabled_by_id(int id);

void device_list(); /* */

void periph_display(int i);


Uint8 device_mon_read(struct machine *oric, Uint16 addr); /* */

void mon_update_periph( struct machine *oric, int id );
int mon_device_count(); /* */
void toggleperiph( struct machine *oric, struct osdmenuitem *mitem, int id );
void shut_periph(struct machine *oric);
SDL_bool periph_shut_by_id(struct machine *oric, int id);
SDL_bool device_reset_all(struct machine *oric); /* */
SDL_bool device_ticktock_all(struct machine *oric, int cycles); /* */
void mon_store_state_periph(struct machine *oric, SDL_bool oldvalid);
void mon_device_oldvalid(SDL_bool oldvalid); /* */
void mon_periphmod( int x, int y, int w, struct textzone *vtz );
void device_sdl_event(SDL_Event *event); /* */

SDL_bool device_test(struct machine *oric); /* */
SDL_bool load_devices_config(struct machine *oric);

