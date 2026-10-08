// vim: tabstop=4 expandtab

// -----------------------------------------------------------------------------
// PLugin Clock
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
#define BASE_ADDR 0x332
#define END_ADDR 0x334

// Possibilité d'avoir deux horloges
#define INSTANCE_MAX 2

struct DATA
{
    Uint8 seconds;
    Uint8 minutes;
    Uint8 hour;

    int count_us;
    int count_ms;
};

struct DATA userdata[INSTANCE_MAX];
struct DATA userdata_old[INSTANCE_MAX];

unsigned int plugin_instances = 0;

static char *description = "Clock";

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
unsigned int clock_create(struct machine *oric)
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
SDL_bool clock_reset(struct expansion_bus *oric_bus, unsigned int instance)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]

    dbg_printf("clock_reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    userdata[instance].hour = 0;
    userdata[instance].minutes = 0;
    userdata[instance].seconds = 0;
    userdata[instance].count_us = 10000;
    userdata[instance].count_ms = 100;

    userdata_old[instance].hour = 0;
    userdata_old[instance].minutes = 0;
    userdata_old[instance].seconds = 0;
    userdata_old[instance].count_us = 10000;
    userdata[instance].count_ms = 100;

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                          Lecture des registres
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Read access
unsigned char clock_read(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, unsigned short offset, SDL_bool run)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]
    run = run; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return (unsigned char) 0;

    instance--;

    switch (offset)
    {
        case 0x00:
            return userdata[instance].hour;

        case 0x01:
            return userdata[instance].minutes;

        case 0x02:
            return userdata[instance].seconds;
    }

    return (Uint8) 0;
}

// -------------------------------------------------------------------------
//                      Ecriture dans les registres
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Write access
SDL_bool clock_write(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, unsigned short offset, unsigned char data)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank;  // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    switch (offset)
    {
        case 0x00:
            userdata[instance].hour= data;
            break;

        case 0x01:
            userdata[instance].minutes = data;
            break;

        case 0x02:
            userdata[instance].seconds = data;

            userdata[instance].count_us = 10000;
            userdata[instance].count_ms = 100;
            break;
    }

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                              Horloge
// -------------------------------------------------------------------------
// Called
void clock_ticktock(struct expansion_bus *oric_bus, unsigned int instance, int cycles)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    if ( !cycles )
        return;

    userdata[instance].count_us -= cycles;

    // 10ms?
    if (userdata[instance].count_us <= 0)
    {
        userdata[instance].count_us += 10000;

        userdata[instance].count_ms--;

        // 1s?
        if (userdata[instance].count_ms <= 0)
        {
            userdata[instance].count_ms = 100;

            userdata[instance].seconds++;

            // 60s?
            if (userdata[instance].seconds >= 60)
            {
                userdata[instance].seconds = 0;

                userdata[instance].minutes++;

                // 60 min?
                if (userdata[instance].minutes >= 60)
                {
                    userdata[instance].minutes = 0;

                    userdata[instance].hour++;

                    // 24?
                    if (userdata[instance].hour >= 24)
                        userdata[instance].hour = 0;
                }
            }
        }
    }
}

// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)
void mon_clock_update(struct textzone *tz, unsigned int instance, unsigned short base_addr, SDL_bool oldvalid)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    my_tzprintfpos( tz, 2, 2,  "Address : $%04X", base_addr);
    my_tzprintfpos( tz, 2, 3,  "Count   : %3dms %5dus", userdata[instance].count_ms, userdata[instance].count_us);

    // Trait de séparation en ligne 4
    tz->px = 0;
    tz->py = 4;
    my_tzputc( tz, 6 );

    for (int i=0; i < tz->w-2; i++)
        my_tzputc( tz, 12 );

    my_tzputc( tz, 8 );

    my_tzprintfpos( tz, 2, 6, "Hour    : %02d", userdata[instance].hour);
    my_tzprintfpos( tz, 2, 7, "Minutes : %02d", userdata[instance].minutes);
    my_tzprintfpos( tz, 2, 8, "Seconds : %02d", userdata[instance].seconds);

    if (oldvalid)
    {
        // Count_ms
        if (userdata[instance].count_ms != userdata_old[instance].count_ms)
            mon_periphmod( 12, 3, 3, tz );

        // Count_us
        if (userdata[instance].count_us != userdata_old[instance].count_us)
            mon_periphmod( 18, 3, 5, tz );

        // Hour
        if (userdata[instance].hour != userdata_old[instance].hour)
            mon_periphmod( 12, 6, 2, tz );

        // Minutes
        if (userdata[instance].minutes != userdata_old[instance].minutes)
            mon_periphmod( 12, 7, 2, tz );

        // Seconds
        if (userdata[instance].seconds != userdata_old[instance].seconds)
            mon_periphmod( 12, 8, 2, tz );
    }
}

// -------------------------------------------------------------------------
//                      Sauvegarde de l'état
// -------------------------------------------------------------------------
// Called by monitor
void mon_clock_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    memcpy(&userdata_old[instance], &userdata[instance], sizeof(struct DATA));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "Clock",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                NULL,
                clock_create,
                NULL,
                clock_reset,
                clock_read,
                clock_write,
                clock_ticktock,
                mon_clock_update,
                mon_clock_store,
                NULL,
    };

