// vim: tabstop=4 expandtab

/*
    Exemple de périphérique qui simule une pile hardware de 16 octets
    BASE_ADDR  : adresse de la pile
    BASE_ADDR+1: pointeur de la pile
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

#define DEBUG_PLUGIN
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

#define BASE_ADDR 0xC256
#define END_ADDR 0xC256

// On ne peut instancier qu'un seul périphérique
#define INSTANCE_MAX 1

// Taille de la pile
#define DATA_SIZE 16

// Structure de données pour une instance du périphérique
struct DATA {
  Uint8 data[DATA_SIZE];
  Uint8 ptr;
};

// Tableau: pile hardware de chaque instance
struct DATA userdata[INSTANCE_MAX];

// Copie de userdata[] pour comparaison entre deux appels au moniteur
// (pour pouvoir afficher les différences)
struct DATA userdata_old[INSTANCE_MAX];

// Nombre d'instances total
unsigned int plugin_instances = 0;

// Description du plugin
static char *description = "Plugin example";

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
SDL_bool plugin_addresses(unsigned int instance,  Uint16 offset)
{
    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    dbg_printf("---plugin addresses(%04x)\n", offset);

    instance--;

    // [base_addr, base_addr+5]
    if (offset <= 0x05)
        return SDL_TRUE;

    // [base_addr+0x10, base_addr+0x15]
    if ((offset >= 0x10) && (offset <= 0x15))
        return SDL_TRUE;

    return SDL_FALSE;
}

// -----------------------------------------------------------------------------
//                              plugin_create
// -----------------------------------------------------------------------------
// Called to create a new instance of the extension
unsigned int plugin_create(struct machine *oric)
{
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return 0;

//    oric->romdis = SDL_FALSE;

    return ++plugin_instances;
}

// -----------------------------------------------------------------------------
//                              plugin_shutdown
// -----------------------------------------------------------------------------
// Called on exit
SDL_bool plugin_shutdown(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]
    instance = instance; // gcc [-Wunused-parameter]

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//                                  plugin_reset
// -----------------------------------------------------------------------------
// Called by init_machine and [F4]
SDL_bool plugin_reset(struct expansion_bus *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    dbg_printf("plugin_reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    userdata[instance].ptr = 0;

    // À voir si on initialise avec des données aléatoires au lieu de 0x00
    memset(userdata[instance].data, 0x00, DATA_SIZE);

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                          Lecture de la pile (POP)
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Read access
Uint8 plugin_read(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 offset, SDL_bool run)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return (Uint8) 0;

    instance--;

    dbg_printf("plugin_read(%d)\n", offset);

    switch (offset)
    {
        case 0:
            if (run)
            {
                // return userdata[instance].data[--userdata[instance].ptr];
                userdata[instance].ptr = (Uint8)(userdata[instance].ptr -1) % DATA_SIZE;
                return userdata[instance].data[userdata[instance].ptr];
            }
            else
                return userdata[instance].data[userdata[instance].ptr];
        case 1:
            return userdata[instance].ptr;

        case 0x10:
            if (run)
            {
                // return userdata[instance].data[--userdata[instance].ptr];
                userdata[instance].ptr = (Uint8)(userdata[instance].ptr -1) % DATA_SIZE;
                return userdata[instance].data[userdata[instance].ptr];
            }
            else
                return userdata[instance].data[userdata[instance].ptr];
        case 0x11:
            return userdata[instance].ptr;

        default:
            dbg_printf("PLUGIN READ: bad address $%04x\n", offset);
            return (Uint8) 0;
    }
}

// -------------------------------------------------------------------------
//                      Ecriture dasn la pile (PUSH)
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Write access
SDL_bool plugin_write(struct expansion_bus *oric, SDL_bool fBank, unsigned int instance, Uint16 offset, Uint8 data)
{
    oric = oric; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    switch (offset)
    {
        case 0:
            // userdata[instance].data[userdata[instance].ptr++] = data;
            userdata[instance].data[userdata[instance].ptr] = data;
            userdata[instance].ptr = (userdata[instance].ptr +1) % DATA_SIZE;
            break;

        case 1:
            userdata[instance].ptr = data % 16;
            break;

        case 0x10:
            // userdata[instance].data[userdata[instance].ptr++] = data;
            userdata[instance].data[userdata[instance].ptr] = data;
            userdata[instance].ptr = (userdata[instance].ptr +1) % DATA_SIZE;
            break;

        case 0x11:
            userdata[instance].ptr = data % 16;
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
void plugin_ticktock(struct expansion_bus *oric_bus, unsigned int instance, int cycles)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    if ( !cycles )
        return;

    // Do something...
    // Exemple: Mise à jour de compteurs, états,... en fonction du nombre
    // de cycles
}

// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)
void mon_plugin_update(struct textzone *ptz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    int i;

    dbg_printf("PLUGIN: mon update\n");

    my_tzprintfpos( ptz, 2, 2,  "Base address : %04X", base_addr);
    my_tzprintfpos( ptz, 2, 3,  "Stack pointer:   %02X", userdata[instance].ptr);
    my_tzprintfpos( ptz, 2, 4,  "Stack size   :   %02X", DATA_SIZE);

    // Trait de séparation en ligne 6
    ptz->px = 0;
    ptz->py = 6;
    my_tzputc( ptz, 6 );

    for (int i=0; i < ptz->w-2; i++)
//        my_tzputc( ptz, 2 );
        my_tzputc( ptz, 12 );

    my_tzputc( ptz, 8 );

    for (i=0; i<8; i++)
    {
        my_tzprintfpos(ptz, 4, i+7, "%c %02X: %02X", (i==userdata[instance].ptr ? '>' : ' '), i, userdata[instance].data[i]);
        my_tzprintfpos(ptz, 4+12, i+7, "%c %02X: %02X", (i+8==userdata[instance].ptr ? '>' : ' '), i+8, userdata[instance].data[i+8]);
    }


    // Affiche sur fond rouge les valeurs différentes par rapport au précédent appel au moniteur.
    if (oldvalid)
    {
        if (userdata[instance].ptr != userdata_old[instance].ptr)
            mon_periphmod( 19, 3, 2, ptz );

        for (i=0; i<8; i++)
        {
            if (userdata[instance].data[i] != userdata_old[instance].data[i])
                mon_periphmod( 10, i+7, 2, ptz );

            if (userdata[instance].data[i+8] != userdata_old[instance].data[i+8])
                mon_periphmod( 10+12, i+7, 2, ptz );
        }
    }
}

// -------------------------------------------------------------------------
//                      Sauvegarde de l'état
// -------------------------------------------------------------------------
// Called by monitor
void mon_plugin_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    // Copy data+ptr
    memcpy(&userdata_old[instance], &userdata[instance], sizeof(struct DATA));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "PLUGIN",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE | PLG_MULTI,     // PLG_BANK,
                plugin_addresses,
                plugin_create,
                plugin_shutdown,
                plugin_reset,
                plugin_read,
                plugin_write,
                plugin_ticktock,
                mon_plugin_update,
                mon_plugin_store,
                NULL,
    };

