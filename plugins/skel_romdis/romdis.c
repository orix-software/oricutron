// vim: tabstop=4 expandtab

/*
    Exemple de périphérique qui permet d'activer la ram overlay
    BASE_ADDR  : commande: 0->ROM, 1->RAM
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
// #define BASE_ADDR 0x360
// #define END_ADDR 0x361

#define BASE_ADDR 0x0330
#define END_ADDR 0x0330

// On ne peut instancier qu'un seul périphérique
#define INSTANCE_MAX 1

// Tableau: registre de chaque instance
Uint8 userdata[INSTANCE_MAX];

// Copie de userdata[] pour comparaison entre deux appels au moniteur
// (pour pouvoir afficher les différences)
Uint8 userdata_old[INSTANCE_MAX];

// Nombre d'instances total
unsigned int plugin_instances = 0;

// Description du plugin
static char *description = "Plugin example (romdis)";

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
//                              plugin_addresses
// -----------------------------------------------------------------------------
// Called to check plugin addresses (if multiples addresses)
// Not used here

// -----------------------------------------------------------------------------
//                              plugin_create
// -----------------------------------------------------------------------------
// Called to create a new instance of the extension
unsigned int romdis_create(struct machine *oric)
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
SDL_bool romdis_reset(struct expansion_bus *oric_bus, unsigned int instance)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]

    dbg_printf("plugin_reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    userdata[instance] = 0;

    *oric_bus->romdis = SDL_FALSE;

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                          Lecture du registre par le 6502
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Read access
Uint8 romdis_read(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 offset, SDL_bool run)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]
    run = run; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return (Uint8) 0;

    instance--;

    dbg_printf("plugin_read(%d)\n", offset);

    switch (offset)
    {
        case 0:
            return userdata[instance];

        default:
            dbg_printf("PLUGIN ROMDIS READ: bad address $%04x\n", offset);
            return (Uint8) 0;
    }
}

// -------------------------------------------------------------------------
//                      Ecriture du registre par le 6502
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Write access
SDL_bool romdis_write(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 offset, Uint8 data)
{
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    switch (offset)
    {
        case 0:
            userdata[instance] = data;
            *oric_bus->romdis = (userdata[instance] != 0);
            break;

        default:
            dbg_printf("PLUGIN WRITE: bad address $%04x\n", offset);
            return (Uint8) 0;
    }
    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                              Horloge
// -------------------------------------------------------------------------
// Called before the execution of a 6502 instruction
// Nor used

// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)
void mon_romdis_update(struct textzone *ptz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    dbg_printf("PLUGIN: mon update\n");

    my_tzprintfpos( ptz, 2, 2,  "Base address : $%04X", base_addr );
    my_tzprintfpos( ptz, 2, 3,  "Memory       : %s", (userdata[instance] ? "Overlay" : "ROM") );

    // Trait de séparation en ligne 6
    ptz->px = 0;
    ptz->py = 6;
    my_tzputc( ptz, 6 );

    for (int i=0; i < ptz->w-2; i++)
        my_tzputc( ptz, 12 );

    my_tzputc( ptz, 8 );

    my_tzprintfpos(ptz, 4, 8, "Flag : $%02X", userdata[instance]);

    // Affiche sur fond rouge les valeurs différentes par rapport au précédent appel au moniteur.
    if (oldvalid)
    {
        if (userdata[instance] != userdata_old[instance])
        {
            // Memory:
            mon_periphmod( 17, 3, 8, ptz );

            // Flag:
            mon_periphmod( 11, 8, 3, ptz );
        }
    }
}

// -------------------------------------------------------------------------
//                      Sauvegarde de l'état
// -------------------------------------------------------------------------
// Called by monitor
void mon_romdis_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    // Copy data
    memcpy(&userdata_old[instance], &userdata[instance], sizeof(Uint8));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "ROMDIS",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                NULL,
                romdis_create,
                NULL,
                romdis_reset,
                romdis_read,
                romdis_write,
                NULL,
                mon_romdis_update,
                mon_romdis_store,
                NULL,
    };

