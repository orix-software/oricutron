// vim: tabstop=4 expandtab

/*
    Exemple d'une cartouche de ROM
    BASE_ADDR  : $C000
    END_ADDR   : $FFFF
*/

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(__amigaos4__) || defined(__MORPHOS__)
#include <proto/dos.h>
#include <dos/dostags.h>
#include <proto/amigaguide.h>
#endif


#include "../../system.h"
#include "../../6502.h"
#include "../../via.h"
#include "../../8912.h"
#include "../../gui.h"
#include "../../disk.h"
#include "../../monitor.h"
#include "../../6551.h"


#include "../../machine.h"

#include "plugin.h"

#include "config_utils.h"

// #define DEBUG_PLUGIN
#ifdef DEBUG_PLUGIN
    // dbg_printf est une fonction déclarée dans monitor.h mais est spécifique au moniteur
    // #define dbg_printf(x...) { printf(x); }
    #define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
    #define dbg_printf(...)
#endif

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
// extern struct textzone *tz[];
// extern struct osdmenu menus[];
// Si Oricutron est compilé sans l'option -rdynamic alors il faut passer la
// référence des fonctions tzprintfpos et tzpoutc au plugin.
#ifndef RDYNAMIC
void (*my_tzprintfpos)( struct textzone *ptz, int x, int y, char *fmt, ... );
void (*my_tzputc)( struct textzone *ptz, char c );
void (*mon_periphmod)( int x, int y, int w, struct textzone *vtz );
#endif

// *****************************************************************************
//                                  Datas
// *****************************************************************************
//
// -----------------------------------------------------------------------------
// Adresse de base et de fin par défaut d'un périphérique

//Plage d'adresses de la rom: $C000-$FFFF
#define BASE_ADDR 0xC000
#define END_ADDR 0xFFFF

// Une seule cartouche possible
#define INSTANCE_MAX 1

// Cartridge memory
Uint8 rombank[16384];

// Configuration file
#define CONFIG_FILE "plugins/myplugin.cfg"

char bank_file[1024];

// Nombre d'instances total
unsigned int plugin_instances = 0;

// Description du plugin
static char *description = "Plugin example (bank)";

// -----------------------------------------------------------------------------
//                              plugin_init
// -----------------------------------------------------------------------------
// Run once right after the library load
//
SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod)
{
    dbg_printf("---plugin init\n");

    my_tzprintfpos = tzprintfpos;
    my_tzputc = tzputc;
    mon_periphmod = _mon_periphmod;

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//                              plugin_addresse
// -----------------------------------------------------------------------------
// Called to check plugin addresses (if multiples addresses)
// Not used here

// -----------------------------------------------------------------------------
//                              plugin_create
// -----------------------------------------------------------------------------
// Called to create a new instance of the extension
unsigned int cartridge_create(struct machine *oric)
{
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return 0;

    return ++plugin_instances;
}

// -----------------------------------------------------------------------------
//                              plugin_shutdown
// -----------------------------------------------------------------------------
// Called on exit
// Not used

// -----------------------------------------------------------------------------
//                                  plugin_reset
// -----------------------------------------------------------------------------
// Called by init_machine and [F4]
SDL_bool cartridge_reset(struct expansion_bus *oric_bus, unsigned int instance)
{
    dbg_printf("plugin_reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    // Disbale internal ROM
    *oric_bus->romdis = config_load(CONFIG_FILE, rombank, bank_file);

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                          Lecture de la rom
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Read access
Uint8 cartridge_read(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 offset, SDL_bool run)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]
    run = run; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return (Uint8) 0;

    instance--;

    dbg_printf("plugin_read(%d)\n", offset);

    return rombank[offset];
}

// -------------------------------------------------------------------------
//                      Ecriture dasn la ROM
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Write access
// Not used (ROM)

// -------------------------------------------------------------------------
//                              Horloge
// -------------------------------------------------------------------------
// Called before the execution of a 6502 instruction
// Not used (ROM)

// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)
//
void mon_cartridge_update(struct textzone *ptz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    char *filename;

    oldvalid = oldvalid; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    dbg_printf("PLUGIN: mon update\n");

    filename = strrchr(bank_file, '/');

    my_tzprintfpos( ptz, 2, 2,  "Base address : $%04X", base_addr);
    my_tzprintfpos( ptz, 2, 3,  "Bank         : %-12s", (filename == NULL ? "" : ++filename));

    // Trait de séparation en ligne 6
    ptz->px = 0;
    ptz->py = 6;
    my_tzputc( ptz, 6 );

    for (int i=0; i < ptz->w-2; i++)
        my_tzputc( ptz, 12 );

    my_tzputc( ptz, 8 );

}

// -------------------------------------------------------------------------
//                      Sauvegarde de l'état
// -------------------------------------------------------------------------
// Called by monitor
// Not used (ROM)

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "My cartridge",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_BANK,
                NULL,
                cartridge_create,
                NULL,
                cartridge_reset,
                cartridge_read,
                NULL,
                NULL,
                mon_cartridge_update,
                NULL,
                NULL,
    };

