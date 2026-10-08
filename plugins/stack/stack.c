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
//                      Extension Stack hardware
// *****************************************************************************
// PIle hardware 16 niveaux
// 0000: pile
// 0001: pointeur de pile
// -----------------------------------------------------------------------------
#define BASE_ADDR 0x356
#define END_ADDR 0x357

#define STACK_SIZE 16
#define INSTANCE_MAX 1

struct STACK {
  Uint8 data[STACK_SIZE];
  Uint8 ptr;
  Uint8 old_data[STACK_SIZE];
  Uint8 old_ptr;
};

struct STACK userdata[INSTANCE_MAX];

unsigned int stack_instances = 0;

static char *description = "Hardware stack";

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
unsigned int stack_create(struct machine *oric)
{
    oric = oric; // gcc [-Wunused-parameter]

    if (stack_instances >= INSTANCE_MAX)
        return 0;

    return ++stack_instances;
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
SDL_bool stack_reset(struct expansion_bus *oric_bus, unsigned int instance)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]

    dbg_printf("stack_reset(%d)\n", instance);

    if ( (!instance) || (instance > stack_instances) )
        return SDL_FALSE;

    instance--;

    userdata[instance].ptr = 0;

    // À voir si on initialise avec des données aléatoires au lieu de 0x00
    memset(userdata[instance].data, 0x00, STACK_SIZE);

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                          Lecture de la pile (POP)
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Read access
Uint8 stack_read(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 addr, SDL_bool run)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > stack_instances) )
        return (Uint8) 0;

    instance--;

    switch (addr)
    {
        case 0:
            if (run)
            {
                // return userdata[instance].data[--userdata[instance].ptr];
                userdata[instance].ptr = (Uint8)(userdata[instance].ptr -1) % STACK_SIZE;
                return userdata[instance].data[userdata[instance].ptr];
            }
            else
                return userdata[instance].data[userdata[instance].ptr];
        case 1:
            return userdata[instance].ptr;

        default:
            dbg_printf("STACK READ: bad address $%04x\n", addr);
            return (Uint8) 0;
    }
}

// -------------------------------------------------------------------------
//                      Ecriture dasn la pile (PUSH)
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Write access
SDL_bool stack_write(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 addr, Uint8 data)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > stack_instances) )
        return SDL_FALSE;

    instance--;

    switch (addr)
    {
        case 0:
            // userdata[instance].data[userdata[instance].ptr++] = data;
            userdata[instance].data[userdata[instance].ptr] = data;
            userdata[instance].ptr = (userdata[instance].ptr +1) % STACK_SIZE;
            break;

        case 1:
            userdata[instance].ptr = data % 16;
            break;

        default:
            dbg_printf("STACK WRITE: bad address $%04x\n", addr);
            return (Uint8) 0;
    }
    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)
void mon_stack_update(struct textzone *tz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    if ( (!instance) || (instance > stack_instances) )
        return;

    instance--;

    int i;

    dbg_printf("STACK: mon update\n");

    my_tzprintfpos( tz, 2, 2,  "Base address : %04X", base_addr);
    my_tzprintfpos( tz, 2, 3,  "Stack pointer:   %02X", userdata[instance].ptr);
    my_tzprintfpos( tz, 2, 4,  "Stack size   :   %02X", STACK_SIZE);

    // Trait de séparation en ligne 6
    tz->px = 0;
    tz->py = 6;
    my_tzputc( tz, 6 );

    for (int i=0; i < tz->w-2; i++)
//        my_tzputc( tz, 2 );
        my_tzputc( tz, 12 );

    my_tzputc( tz, 8 );

    for (i=0; i<8; i++)
    {
        my_tzprintfpos(tz, 4, i+7, "%c %02X: %02X", (i==userdata[instance].ptr ? '>' : ' '), i, userdata[instance].data[i]);
        my_tzprintfpos(tz, 4+12, i+7, "%c %02X: %02X", (i+8==userdata[instance].ptr ? '>' : ' '), i+8, userdata[instance].data[i+8]);
    }


    if (oldvalid)
    {
        if (userdata[instance].ptr != userdata[instance].old_ptr)
            mon_periphmod( 19, 3, 2, tz );

        for (i=0; i<8; i++)
        {
            if (userdata[instance].data[i] != userdata[instance].old_data[i])
                mon_periphmod( 10, i+7, 2, tz );

            if (userdata[instance].data[i+8] != userdata[instance].old_data[i+8])
                mon_periphmod( 10+12, i+7, 2, tz );
        }
    }

//    tzstrpos(tz, 1, 15, "....+....|....+....|....+...");
}

// -------------------------------------------------------------------------
//                      Sauvegarde de l'état
// -------------------------------------------------------------------------
// Called by monitor
void mon_stack_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > stack_instances) )
        return;

    instance--;

    // Copy data+ptr
    memcpy(&userdata[instance].old_data, &userdata[instance].data, STACK_SIZE+1);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "STACK",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                NULL,
                stack_create,
                NULL,
                stack_reset,
                stack_read,
                stack_write,
		NULL,
                mon_stack_update,
                mon_stack_store,
                NULL,
    };

