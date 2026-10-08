// vim: tabstop=4 expandtab

// -----------------------------------------------------------------------------
// PLugin DEBUG
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
#define BASE_ADDR 0x331
#define END_ADDR 0x331

#define INSTANCE_MAX 1

Uint8 userdata[INSTANCE_MAX];
Uint8 userdata_old[INSTANCE_MAX];

unsigned int plugin_instances = 0;

static char *description = "Debug";

// -----------------------------------------------------------------------------
//                              plugin_init
// -----------------------------------------------------------------------------
// Run once right after thz library load
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
//                              plugin_create
// -----------------------------------------------------------------------------
// Called to create a new instance of the extension
unsigned int debug_create(struct machine *oric)
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
SDL_bool debug_reset(struct expansion_bus *oric_bus, unsigned int instance)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]

    dbg_printf("stack_reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    userdata[instance] = 0;
    userdata_old[instance] = 0;

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                          Lecture du registre par le 6502
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Read access
unsigned char debug_read(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, unsigned short addr, SDL_bool run)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]
    addr = addr; // gcc [-Wunused-parameter]
    run = run; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return (unsigned char) 0;

    instance--;

    return userdata[instance];
}

// -------------------------------------------------------------------------
//                      Ecriture dans le registre par le 6502
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Write access
SDL_bool debug_write(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, unsigned short addr, unsigned char data)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank;  // gcc [-Wunused-parameter]
    addr = addr; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    userdata[instance] = data;

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                              Horloge
// -------------------------------------------------------------------------
// Called
void debug_ticktock(struct expansion_bus *oric_bus, unsigned int instance, int cycles)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    if ( !cycles )
        return;

    if (userdata[instance])
        fprintf(stderr, "PC=%04X, A=%02X, X=%02X, Y=%02X, P=%c%c-%c%c%c%c%c, SP=1%02X, CALCOP=%02X LPC=%04X, CALCPC=%04X, BADDR=%04X, IRQ=%02X, ROMDIS=%02X\n",
                    oric_bus->cpu->pc,
                    oric_bus->cpu->a,
                    oric_bus->cpu->x,
                    oric_bus->cpu->y,

                    oric_bus->cpu->f_n ? 'N' : '-',
                    oric_bus->cpu->f_v ? 'V' : '-',
                    oric_bus->cpu->f_b ? 'B' : '-',
                    oric_bus->cpu->f_d ? 'D' : '-',
                    oric_bus->cpu->f_i ? 'I' : '-',
                    oric_bus->cpu->f_z ? 'Z' : '-',
                    oric_bus->cpu->f_c ? 'C' : '-',

                    oric_bus->cpu->sp,
                    oric_bus->cpu->calcop,
                    oric_bus->cpu->lastpc,
                    oric_bus->cpu->calcpc,
                    oric_bus->cpu->baddr,
                    *oric_bus->irq,
                    *oric_bus->romdis
                    );

}

// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)
void mon_debug_update(struct textzone *tz, unsigned int instance, unsigned short base_addr, SDL_bool oldvalid)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    my_tzprintfpos( tz, 2, 2,  "Base address : $%04X", base_addr);
    my_tzprintfpos( tz, 2, 3,  "Active flag  : $%02X", userdata[instance]);

    if (oldvalid)
    {
        if (userdata[instance] != userdata_old[instance])
            mon_periphmod( 18, 3, 2, tz );
    }
}

// -------------------------------------------------------------------------
//                      Sauvegarde de l'état
// -------------------------------------------------------------------------
// Called by monitor
void mon_debug_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    userdata_old[instance] = userdata[instance];
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "DEBUG",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                NULL,
                debug_create,
                NULL,
                debug_reset,
                debug_read,
                debug_write,
                debug_ticktock,
                mon_debug_update,
                mon_debug_store,
                NULL,
    };

