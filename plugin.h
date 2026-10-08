#define PERIPH_NAME_LEN 20

#define PLG_DEVICE 1
#define PLG_BANK 2
#define PLG_MULTI 4
#define PLG_PRINTER 8

struct expansion_bus {
    struct m6502 *cpu;
    Uint16 address;
    SDL_bool *romdis;
    Uint8 *irq;
    SDL_bool io;
    SDL_bool reset;
    SDL_bool nmi;

    // Utilitaires
    Uint8 type;
    int drivetype;
};

struct printer_port {
	Uint8 data;
	Uint8 strobe;
	Uint8 acknowledge;
};

struct PLUGIN {
    char name[PERIPH_NAME_LEN+1];
    Uint16 default_addr;
    Uint16 size;
    Uint16 type;
    SDL_bool (*addresses)(unsigned int instance, Uint16 offset);

    unsigned int (*create)(struct machine *oric);
    SDL_bool (*shutdown)(struct machine *oric, unsigned int instance);

    SDL_bool (*reset)(struct expansion_bus *oric_bus, unsigned int instance);
    Uint8 (*read)(struct expansion_bus *oric, SDL_bool fbank, unsigned int instance, Uint16 addr, SDL_bool fexec);
    SDL_bool (*write)(struct expansion_bus *oric_bus, SDL_bool fbank, unsigned int instance, Uint16 addr, Uint8 data);

    void (*ticktock)(struct expansion_bus *oric_bus, unsigned int instance, int cycles);

    void (*mon_update)(struct textzone *tz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid);
    void (*mon_store_state)(struct machine *oric, unsigned int instance);

    SDL_bool (*sdl_event)(SDL_Event* event);
};
/*
struct PLUGINS {
    Uint16 addr_start;
    Uint16 addr_end;
    unsigned int instance;
    SDL_bool enable;
    struct PLUGIN *periph;
};
*/
