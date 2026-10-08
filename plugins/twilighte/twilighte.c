// vim: tabstop=4 expandtab

/*
    Exemple de périphérique qui simule une pile hardware de 16 octets
    BASE_ADDR  : adresse de la pile
    BASE_ADDR+1: ppointeur de la pile
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

// Pour les fonction de lecture du fichier de configuration
#include "../../main.h"

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

#define BASE_ADDR 0x0320
#define END_ADDR 0x0343

// On ne peut instancier qu'un seul périphérique
#define INSTANCE_MAX 1

// Taille de la pile
#define DATA_SIZE 16

// Structure de données pour une instance du périphérique
struct BOARD {
    int firmware_version;
    SDL_bool microdisc;                         // Inutilisé pour le moment

    unsigned int bank;

    // 0x0320 - 0x032f
    unsigned char DDRA;
    unsigned char IORAh;

    unsigned char DDRB;
    unsigned char IORB;

    unsigned char T1CL;
    unsigned char TiCH;
    unsigned char T1LL;
    unsigned char TILH;
    unsigned char T2CL;
    unsigned char T2CH;
    unsigned char SR;
    unsigned char ACR;
    unsigned char PCR;
    unsigned char IFR;
    unsigned char IER;
    unsigned char IORA;

    // 0x0342 - 0x0343
    unsigned char t_register;                   // ?xRx xVVV
    unsigned char t_banking_register;

};

// Tableau: instances
struct BOARD userdata[INSTANCE_MAX];

// Copie de userdata[] pour comparaison entre deux appels au moniteur
// (pour pouvoir afficher les différences)
struct BOARD userdata_old[INSTANCE_MAX];

// Banques de RAM
Uint8 rambank[32+1][16384];

// Banques de ROM
Uint8 rombank[32+1][16384];

// Conversion banques RAM
static Uint8 romoffset[] = {0, 8, 12, 16, 4, 20, 24, 28};

// Conversion banques ROM
static Uint8 ramoffset[] = {0, 4, 8, 12, 16, 20, 24, 28};

// Nombre d'instances total
unsigned int plugin_instances = 0;

// Description du plugin
static char *description = "Twilighte board";

#define CONFIG_FILE "plugins/twilighte.cfg"

// -----------------------------------------------------------------------------
//              Déclarations des fonctions utilitaires
// -----------------------------------------------------------------------------
unsigned int cpld(struct BOARD *twilighte);
unsigned int logical_bank(struct BOARD *twilighte);
static SDL_bool bank_load(char* fname, int size, unsigned char where[]);
SDL_bool config_load(struct BOARD *twilighte);

void error_printf( char *fmt, ... );

// -----------------------------------------------------------------------------
//                              plugin_init
// -----------------------------------------------------------------------------
// Run once right after thz library load
//
SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod)
{
    dbg_printf("---TWILIGHTE init\n");

    my_tzprintfpos = tzprintfpos;
    my_tzputc = tzputc;
    mon_periphmod = _mon_periphmod;

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//                              twilighte_create
// -----------------------------------------------------------------------------
// Called to check plugin addresses (if multiples addresses)
SDL_bool twilighte_addresses(unsigned int instance,  Uint16 offset)
{
    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    dbg_printf("---TWILIGHTE addresses(%04x)\n", offset);

    instance--;

    // [base_addr, base_addr+3]
    if (offset <= 0x0f)
        return SDL_TRUE;

    // [base_addr+0x0c, base_addr+0x0f]
    if ((offset >= 0x22) && (offset <= 0x23))
        return SDL_TRUE;

    return SDL_FALSE;
}

// -----------------------------------------------------------------------------
//                              twilighte_create
// -----------------------------------------------------------------------------
// Called to create a new instance of the extension
unsigned int twilighte_create(struct machine *oric)
{
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return 0;

//    *oric->romdis = SDL_FALSE;

    return ++plugin_instances;
}

// -----------------------------------------------------------------------------
//                              twilighte_shutdown
// -----------------------------------------------------------------------------
// Called on exit
// Not used
/*
SDL_bool twilighte_shutdown(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]
    instance = instance; // gcc [-Wunused-parameter]

    return SDL_TRUE;
}
*/

// -----------------------------------------------------------------------------
//                                  twilighte_reset
// -----------------------------------------------------------------------------
// Called by init_machine and [F4]
SDL_bool twilighte_reset(struct expansion_bus *oric_bus, unsigned int instance)
{
    dbg_printf("TWILIGHTE_reset(%d)\n", instance);
    error_printf(": oric->romdis = %02x\n", *oric_bus->romdis);
    error_printf(": oric->type  = %02x\n", oric_bus->type);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

//    if (oric_bus->type != MACH_ATMOS)
//    {
//        error_printf("TWILIGHTE: NOT AN ATMOS");
//        return SDL_FALSE;
//    }

    userdata[instance].firmware_version = 2;
    userdata[instance].t_register = (userdata[instance].firmware_version & 0x03) | 0x80;
    userdata[instance].t_banking_register = 0;
    userdata[instance].DDRA = 0xa7;         // 0b10100111;
    userdata[instance].IORAh = 0x07;
    userdata[instance].DDRB = 0xc0;         // 0b11000000;
    userdata[instance].IORB = 0;


    // À voir si on initialise avec des données aléatoires au lieu de 0x00
    config_load(&userdata[instance]);

    // Désactive la rom interne
    *oric_bus->romdis = SDL_TRUE;
    // Défaut par setromon()
    // oric_bus->romon = ! oric_bus->romdis;

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                          Lecture de la pile (POP)
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Read access
Uint8 twilighte_read(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 offset, SDL_bool run)
{
    Uint8 data = 0;
    unsigned int bank;

    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    run = run; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return data;

    instance--;
/*
    if (!fBank)
        dbg_printf("TWILIGHTE READ: port @$%04x, romdis %d\n", offset, *oric->romdis);
    else
        dbg_printf("TWILIGHTE READ: bank @$%04x, romdis %d\n", offset+0xc000, *oric->romdis);
*/
    if (!fBank)
        // Lecture d'un registre
        switch (offset)
        {
//            case 0x314-0x314:
//                // 0x314: Microdisc mirror
//                break;

            // VIA2
            case 0x320-BASE_ADDR:
                // 0x320: IORB
                // /!\ ATTENTION joytsticks non pris en compte pour le moment
                data = userdata[instance].IORB;
                break;

            case 0x321-BASE_ADDR:
                // 0x321: IORAh
                // /!\ ATTENTION joytsticks non pris en compte pour le moment
                data = userdata[instance].IORAh;
                break;

            case 0x322-BASE_ADDR:
                // 0x322: DDRB
                data = userdata[instance].DDRB;
                break;

            case 0x323-BASE_ADDR:
                // 0x323: DDRA
                data = userdata[instance].DDRA;
                break;

            // Autres registres VIA non émulés correctement
            case 0x324-BASE_ADDR:
                data = userdata[instance].T1CL;
                break;

            case 0x325-BASE_ADDR:
                data = userdata[instance].TiCH;
                break;

            case 0x326-BASE_ADDR:
                data = userdata[instance].T1LL;
                break;

            case 0x327-BASE_ADDR:
                data = userdata[instance].TILH;
                break;

            case 0x328-BASE_ADDR:
                data = userdata[instance].T2CL;
                break;

            case 0x329-BASE_ADDR:
                data = userdata[instance].T2CH;
                break;

            case 0x32a-BASE_ADDR:
                data = userdata[instance].SR;
                break;

            case 0x32b-BASE_ADDR:
                data = userdata[instance].ACR;
                break;

            case 0x32c-BASE_ADDR:
                data = userdata[instance].PCR;
                break;

            case 0x32d-BASE_ADDR:
                data = userdata[instance].IFR;
                break;

            case 0x32e - BASE_ADDR:
                data = userdata[instance].IER;
                break;

            case 0x32f-BASE_ADDR:
                data = userdata[instance].IORA;
                break;

            // Twilighte registers
            case 0x342-BASE_ADDR:
                // 0x342: Extension register
                data = userdata[instance].t_register;
                break;

            case 0x343-BASE_ADDR:
                // 0x343: Banking register
                data = userdata[instance].t_banking_register;
                break;

            default:
                dbg_printf("TWILIGHTE READ: bad address $%04x\n", offset);
                return (Uint8) 0;
        }
    else
    // Lecture d'une banque
    {
        bank = logical_bank(&userdata[instance]);

        if (bank == 0)
            data = rambank[0][offset];

        else if (bank > 32)
            data = rambank[bank-32][offset];

        else
            data = rombank[bank][offset];
    }

    return data;
}

// -------------------------------------------------------------------------
//                      Ecriture dans les registres de la carte
// -------------------------------------------------------------------------
// run: FALSE -> exécution depuis le moniteur
// Write access
SDL_bool twilighte_write(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 offset, Uint8 data)
{
    unsigned int bank;

    oric_bus = oric_bus; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

#ifdef DEBUG_PLUGIN
    if (!fBank)
        dbg_printf("TWILIGHTE WRITE: port $%04x <- $%02x\n", offset, data);
    else
        dbg_printf("TWILIGHTE WRITE: bank $%04x <- $%02x\n", offset, data);
#endif

    if (!fBank)
        // Écriture dans un registre
        switch (offset)
        {
//            case 0x314-0x314:
//                // 0x314: Microdisc mirror
//                break;

            // VIA2
            case 0x320-BASE_ADDR:
                // 0x320: IORB
                // /!\ ATTENTION joytsticks non pris en compte pour le moment (aucun bouton appuyé, pull-up des entrées)
                userdata[instance].IORB = (data & userdata[instance].DDRB) | (~userdata[instance].DDRB);
                break;

            case 0x321-BASE_ADDR:
                // 0x321: IORAh
                // /!\ ATTENTION joytsticks non pris en compte pour le moment (on force aucun bouton appuyé, pull-up des entrées)
                userdata[instance].IORAh = (data & userdata[instance].DDRA) | (~userdata[instance].DDRA);
                userdata[instance].IORA = userdata[instance].IORAh;
                break;

            case 0x322-BASE_ADDR:
                // 0x322: DDRB
                userdata[instance].DDRB = data;
                break;

            case 0x323-BASE_ADDR:
                // 0x323: DDRA
                userdata[instance].DDRA = data;
                break;

            // Autres registres VIA non émulés correctement
            case 0x324-BASE_ADDR:
                userdata[instance].T1CL = data;
                break;

            case 0x325-BASE_ADDR:
                userdata[instance].TiCH = data;
                break;

            case 0x326-BASE_ADDR:
                userdata[instance].T1LL = data;
                break;

            case 0x327-BASE_ADDR:
                userdata[instance].TILH = data;
                break;

            case 0x328-BASE_ADDR:
                userdata[instance].T2CL = data;
                break;

            case 0x329-BASE_ADDR:
                userdata[instance].T2CH = data;
                break;

            case 0x32a-BASE_ADDR:
                userdata[instance].SR = data;
                break;

            case 0x32b-BASE_ADDR:
                userdata[instance].ACR = data;
                break;

            case 0x32c-BASE_ADDR:
                userdata[instance].PCR = data;
                break;

            case 0x32d-BASE_ADDR:
                userdata[instance].IFR = data;
                break;

            case 0x32e - BASE_ADDR:
                userdata[instance].IER = data;
                break;

            case 0x32f-BASE_ADDR:
                userdata[instance].IORA = (data & userdata[instance].DDRA) | (~userdata[instance].DDRA);
                userdata[instance].IORAh = userdata[instance].IORA;
                break;

            // Twilighte registers
            case 0x342-BASE_ADDR:
                // 0x342: Extension register
                // b5: 0->ROM, 1->RAM
                userdata[instance].t_register = data;
                break;

            case 0x343-BASE_ADDR:
                // 0x343: Banking register
                userdata[instance].t_banking_register = data;
                break;

            default:
                dbg_printf("TWILIGHTE WRITE: bad address $%04x\n", offset);
                return SDL_FALSE;
        }
    else
    // Écriture dans une banque
    {
        bank = logical_bank(&userdata[instance]);

        if ( bank == 0)
            rambank[0][offset] = data;

        else if ( bank > 32)
            rambank[bank-32][offset] = data;

#ifdef DEBUG_PLUGIN
        else
            dbg_printf("TWILIGHTE WRITE: ROM (hw=%d, sw=%d", cpld(&userdata[instance]), bank);
#endif
    }

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)
void mon_twilighte_update(struct textzone *ptz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    base_addr = base_addr; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    int bank;
    struct BOARD *twilighte = &userdata[instance];

    dbg_printf("TWILIGHTE: mon update (instance=%d)\n", instance);

    //
    // Board version = $xx
    //
    // Bank set      = $xx
    // Bank hardware number = $xx
    // Bank software number = $xx
    // Bank type     = ssss
    // ---------------------------
    // Twil register = $xx
    // Bank register = $xx
    //
    // IORB          = $xx
    // IORAh         = $xx
    // DDRB          = $xx
    // DDRA          = $xx
    //
    //
    //
    //
    //123456789.123456789.12345678

    bank = logical_bank(twilighte);

    my_tzprintfpos( ptz, 2, 2,  "Board version :  %02d", twilighte->t_register & 0x07 );
    my_tzprintfpos( ptz, 2, 4,  "Bank set      : $%02X", twilighte->t_banking_register );
    my_tzprintfpos( ptz, 2, 5,  "Bank hardware :  %02d", cpld(twilighte) );
    my_tzprintfpos( ptz, 2, 6,  "Bank software :  %02d", (bank > 32 ? bank - 32 : bank) );
    my_tzprintfpos( ptz, 2, 7,  "Bank type     : %s"  , ((bank == 0) ? "Overlay" : ((bank > 32) ? "SRAM" : "EEPROM")) );

    // Trait de séparation en ligne 8
    ptz->px = 0;
    ptz->py = 8;
    my_tzputc( ptz, 6 );

    for (int i=0; i < ptz->w-2; i++)
//        my_tzputc( ptz, 2 );
        my_tzputc( ptz, 12 );

    my_tzputc( ptz, 8 );

    my_tzprintfpos( ptz, 2, 9 , "Twil register : $%02X", twilighte->t_register );
    my_tzprintfpos( ptz, 2, 10, "Bank register : $%02X", twilighte->t_banking_register );

    my_tzprintfpos( ptz, 2, 12, "IORB          : $%02X", twilighte->IORB );
    my_tzprintfpos( ptz, 2, 13, "IORAh         : $%02X", twilighte->IORAh );
    my_tzprintfpos( ptz, 2, 14, "DDRB          : $%02X", twilighte->DDRB );
    my_tzprintfpos( ptz, 2, 15, "DDRA          : $%02X", twilighte->DDRA );


    // Affiche sur fond rouge les valeurs différentes par rapport au précédent appel au moniteur.
    if (oldvalid)
    {
        struct BOARD *twilighte_old = &userdata_old[instance];
        int bank_old = logical_bank(twilighte_old);

        // Board version
        if ( (twilighte->t_register & 0x07) != (twilighte_old->t_register & 0x07) )
            mon_periphmod( 19, 2, 2, ptz );


        // Bank set
        if (twilighte->t_banking_register != twilighte_old->t_banking_register)
            mon_periphmod( 19, 4, 2, ptz );

        // Bank hardware
        if (cpld(twilighte) != cpld(twilighte_old))
            mon_periphmod( 19, 5, 2, ptz );

        // Bank software
        if (bank != bank_old)
            mon_periphmod( 19, 6, 2, ptz );

        // Bank type
        if ( ((bank == 0) ? 0 : ((bank > 32) ? 1 : 2)) != ((bank_old == 0) ? 0 : ((bank_old > 32) ? 1 : 2)) )
            mon_periphmod( 18, 7, 7, ptz );


        // Twil register
        if (twilighte->t_register != twilighte_old->t_register)
            mon_periphmod( 19, 9,  2, ptz );

        // Bank register
        if (twilighte->t_banking_register != twilighte_old->t_banking_register)
            mon_periphmod( 19, 10, 2, ptz );


        // IORB
        if (twilighte->IORB != twilighte_old->IORB)
            mon_periphmod( 19, 12, 2, ptz );

        // IORah
        if (twilighte->IORAh != twilighte_old->IORAh)
            mon_periphmod( 19, 13, 2, ptz );

        // DDRB
        if (twilighte->DDRB != twilighte_old->DDRB)
            mon_periphmod( 19, 14, 2, ptz );

        // DDRA
        if (twilighte->DDRA != twilighte_old->DDRA)
            mon_periphmod( 19, 15, 2, ptz );
    }
    dbg_printf("TWILIGHTE: mon update]\n");
}

// -------------------------------------------------------------------------
//                      Sauvegarde de l'état
// -------------------------------------------------------------------------
// Called by monitor
void mon_twilighte_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    // Copy data+ptr
    memcpy(&userdata_old[instance], &userdata[instance], sizeof(struct BOARD));
}

// -------------------------------------------------------------------------
//                      Simulation du CPLD
// -------------------------------------------------------------------------
unsigned int cpld(struct BOARD *twilighte)
{
    // twilighte->current_bank = twilighte->IORAh & 7
    unsigned int current_bank = twilighte->IORAh & 0x07;
    unsigned int bank;

    // RAM Overlay?
    if (current_bank == 0)
        bank = 7;

    // ROM
    else if ( (current_bank >=5) && (current_bank <=7) )
        bank = current_bank-1;

    // ROM/RAM fonction du bit5 de twilighte->t_register
    else if (twilighte->t_banking_register < 4)
        bank = (current_bank-1+twilighte->t_banking_register*8);

    // ROM/RAM fonction du bit5 de twilighte->t_register
    else if (twilighte->t_banking_register >= 4)
        bank = (current_bank+3+(twilighte->t_banking_register & 3)*8);

    // Valeurs invalides
    else
        error_printf("Banking error: bank=%d, set=%d", current_bank, twilighte->t_banking_register);
        // dbg_printf("Banking error: bank=%d, set=%d", current_bank, twilighte->t_banking_register);

    return bank;
}

// -------------------------------------------------------------------------
//                   Conversion banque CPLD -> banque logique
// -------------------------------------------------------------------------
unsigned int logical_bank(struct BOARD *twilighte)
{
    unsigned int bank = twilighte->IORAh & 0x07;

    // RAM Overlay?
    if (bank == 0)
        return bank;

    // ROM
    if (bank >= 5)
        return bank;

    // ROM/RAM fonction du bit5 de twilighte->t_register
    if (twilighte->t_register & 0x20)
        // RAM
        return (bank + ramoffset[twilighte->t_banking_register & 0x07] + 32);
    else
        // ROM
        return (bank + romoffset[twilighte->t_banking_register & 0x07]);
}

// -------------------------------------------------------------------------
//                              Load config & banks
// -------------------------------------------------------------------------
SDL_bool config_load(struct BOARD *twilighte)
{
    FILE* f;

    char* result;
    char line[1024];

    char tbtmprom[32];
    char tbtmpram[32];
    char bankfile[1024];

    unsigned int i;

    f = fopen(CONFIG_FILE, "r");
    if (!f)
    {
        error_printf(CONFIG_FILE " not found\n");
        // dbg_printf(CONFIG_FILE " not found\n");
        return SDL_FALSE;
    }

//    for (j = 0; j < 32; j++)
//    {
//        twilighte->twilrombankfiles[j][0] = 0;
//        twilighte->twilrambankfiles[j][0] = 0;
//    }

    while (!feof(f))
    {
        result = fgets(line, 1024, f);
        if( result )
        {
          // FIXME: do something to silence the compiler warning ...
        }

        if (read_config_int(line, "firmware", &twilighte->firmware_version, 1, 3))
        {
             // because we initialize board version to 1. If firmware_version is set, then we have a look to firmware_version.
             // If it's equal to 1, then do not overlap the value
            if (twilighte->firmware_version!=1) {
                twilighte->t_register=twilighte->t_register & 0xfe; // Remove bit 0 (firmware 1 which is the default when the .cfg does not contain firmware version)
                twilighte->t_register = twilighte->t_register | (twilighte->firmware_version);
            }
            continue;
        }

        if( read_config_bool(   line, "microdisc",     &twilighte->microdisc ) ) continue;

        for (i = 1; i <= 32; i++)
        {
            sprintf(tbtmprom, "twilbankrom%02d", i);
            sprintf(tbtmpram, "twilbankram%02d", i);

            if (read_config_string(line, tbtmprom, bankfile, 1024))
            {
                // Chargement de la banque
                dbg_printf("Loading bank %s in %s...", bankfile, tbtmprom);

                if (!bank_load(bankfile, -16384, rombank[i]))
                {
                    error_printf("Cannot load %s", bankfile);
                    dbg_printf(" failed.\n");

                    return SDL_FALSE;
                }
                dbg_printf(" done.\n");
            }

            if (read_config_string(line, tbtmpram, bankfile, 1024))
            {
                // Chargement de la banque
                dbg_printf("Loading bank %s in %s...", bankfile, tbtmpram);

                if (!bank_load(bankfile, -16384, rambank[i]))
                {
                    error_printf("Cannot load %s", bankfile);
                    dbg_printf(" failed.\n");

                    return SDL_FALSE;
                }
                dbg_printf(" done.\n");
            }

        }
    }

    fclose(f);

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//                                  Load banks
// -----------------------------------------------------------------------------
static SDL_bool bank_load(char* fname, int size, Uint8 where[])
{
    SDL_RWops* f;
    char* tmpname;

    // MinGW doesn't have asprintf :-(
    tmpname = malloc(strlen(fname) + 10);
    if (!tmpname) {

        return SDL_FALSE;
    }

    sprintf(tmpname, "%s.rom", fname);
    f = SDL_RWFromFile(tmpname, "rb");
    if (!f)
    {
        error_printf("Unable to open '%s'\n", tmpname);
        free(tmpname);
        return SDL_FALSE;
    }

    if (size < 0)
    {
        int filesize;
        SDL_RWseek(f, 0, SEEK_END);
        filesize = SDL_RWtell(f);
        SDL_RWseek(f, 0, SEEK_SET);

        if (filesize > -size)
        {
            error_printf("ROM '%s' exceeds %d bytes.\n", fname, -size);
            SDL_RWclose(f);
            free(tmpname);
            return SDL_FALSE;
        }

        where += (-size) - filesize;
        size = filesize;
    }

    if (SDL_RWread(f, where, size, 1) != 1)
    {
        error_printf("Unable to read '%s'\n", tmpname);
        SDL_RWclose(f);
        free(tmpname);
        return SDL_FALSE;
    }

    SDL_RWclose(f);
    free(tmpname);
    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "Twilghte",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE | PLG_MULTI | PLG_BANK,
                twilighte_addresses,
                twilighte_create,
                NULL,
                twilighte_reset,
                twilighte_read,
                twilighte_write,
                NULL,
                mon_twilighte_update,
                mon_twilighte_store,
                NULL,
    };

