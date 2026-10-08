// -----------------------------------------------------------------------------
// vim: ts=4 et
// -----------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

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

#define DS1501_SECONDS_REGISTER 0x360
#define DS1501_MINUTES_REGISTER 0x361
#define DS1501_HOUR_REGISTER    0x362
#define DS1501_DAY_REGISTER     0x363
#define DS1501_DATE_REGISTER    0x364
#define DS1501_MONTH_REGISTER   0x365
#define DS1501_YEAR_REGISTER    0x366
#define DS1501_CENTURY_REGISTER 0x367

#define DS1501_ADDRESS_INTERNAL_RAM_REGISTER 0x370
#define DS1501_DATA_INTERNAL_RAM_REGISTER 0x373

#define DS1501_CTRLA_REGISTER   0x36E
#define DS1501_CTRLB_REGISTER   0x36F

#define IRQB_DS1501 4
#define IRQF_DS1501 (1 << IRQB_DS1501)

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
#define BASE_ADDR 0x360
#define END_ADDR 0x37F

#define INSTANCE_MAX 1

// Control A flags
#define BLF1_mask    0x80
#define BLF2_mask    0x40
#define PRS_mask     0x20
#define PAB_mask     0x10
#define TDF_mask     0x08
#define KSF_mask     0x04
#define WDF_mask     0x02
#define IRQF_mask    0x01

// Control B flags
#define TE_mask      0x80
#define CS_mask      0x40
#define BME_mask     0x20
#define TPE_mask     0x10
#define TIE_mask     0x08
#define KIE_mask     0x04
#define WDE_mask     0x02
#define WDS_mask     0x01

// Month register control bits
#define EOSC_mask   0x80
#define E32K_mask   0x40
#define BB32_maks   0x20

// Alarm control bits
#define AM1_mask    0x80
#define AM2_mask    0x80
#define AM3_mask    0x80
#define AM4_mask    0x80
#define DYDT_mask   0x40

// Utilitaire
#define bcd2bin(data)   (( (data) & 0x0f) + ( (data) >> 4)*10)

struct DS1501_REGISTERS
{
    Uint8 seconds;
    Uint8 minutes;
    Uint8 hours;
    Uint8 day;
    Uint8 date;
    Uint8 month;
    Uint8 year;
    Uint8 century;
    Uint8 alarm_seconds;
    Uint8 alarm_minutes;
    Uint8 alarm_hours;
    Uint8 alarm_day_date;
    Uint8 watchdog_ms;
    Uint8 watchdog_s;
};

struct DS1501
{
    struct DS1501_REGISTERS internal;
    struct DS1501_REGISTERS external;

    Uint8 control_a;
    Uint8 control_b;
    Uint8 ram_address;
    Uint8 ram[256];

    int clock_us;
    int clock_ms;
    Uint8 internal_watchdog_ms;
    Uint8 internal_watchdog_s;

    SDL_bool pending_transfert;
};

struct DS1501 *userdata[INSTANCE_MAX];
struct DS1501 *userdata_old[INSTANCE_MAX];

unsigned int plugin_instances = 0;

static char *description = "DS1501";

Uint8 bin2bcd(Uint8 value);

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod)
{
    dbg_printf("---plugin init\n");

    my_tzprintfpos = tzprintfpos;
    my_tzputc = tzputc;
    mon_periphmod = _mon_periphmod;

    return SDL_TRUE;
}

    // -----------------------------------------------------------------------------
    //
    // -----------------------------------------------------------------------------
unsigned int ds1501_create(struct machine *oric)
{
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return 0;

    userdata[plugin_instances] = malloc(sizeof(struct DS1501));
    userdata_old[plugin_instances] = malloc(sizeof(struct DS1501));

    if (userdata[plugin_instances])
    {
        if (userdata_old[plugin_instances] == NULL)
        {
            free(userdata[plugin_instances]);
            return 0;
        }

        srandom(time(0));

        // Initilisation des flags
        // EOSC = E32K = 0
        userdata[plugin_instances]->internal.month &= ~(EOSC_mask | E32K_mask);


        // Internal

        // On force les bits non utilisés à 0
        userdata[plugin_instances]->internal.seconds = random() % 60;
        userdata[plugin_instances]->internal.minutes = random() % 60;
        userdata[plugin_instances]->internal.hours = random() % 24;
        userdata[plugin_instances]->internal.day = random() % 7 +1;
        userdata[plugin_instances]->internal.date = random() % 31 +1;

        userdata[plugin_instances]->internal.month = random() % 12 +1;
        userdata[plugin_instances]->internal.year = random() % 100;
        userdata[plugin_instances]->internal.century = random() % 40;

        userdata[plugin_instances]->internal.alarm_seconds = (userdata[plugin_instances]->internal.alarm_seconds & 0x80) + random() % 60;
        userdata[plugin_instances]->internal.alarm_minutes = (userdata[plugin_instances]->internal.alarm_seconds & 0x80) + random() % 60;
        userdata[plugin_instances]->internal.alarm_hours = (userdata[plugin_instances]->internal.alarm_seconds & 0x80) + random() % 23;
        userdata[plugin_instances]->internal.alarm_day_date = (userdata[plugin_instances]->internal.alarm_day_date & 0xC0) + random() % 31 +1;

        // Initilisation des flags
        // EOSC = E32K = 0
        userdata[plugin_instances]->internal.month &= ~(EOSC_mask | E32K_mask);

        // TIE = KIE = WDE = WDS = 0
        userdata[plugin_instances]->control_b &= ~(TIE_mask | KIE_mask |  WDE_mask | WDS_mask);

        // Indique Vbat (BLF1) et Vbaux (BLF2) Ok
        userdata[plugin_instances]->control_a &= ~(BLF1_mask | BLF2_mask);

        // Watchdog
        userdata[plugin_instances]->internal.watchdog_ms = random() % 100;
        userdata[plugin_instances]->internal.watchdog_s = random() % 100;

        userdata[plugin_instances]->internal_watchdog_ms = userdata[plugin_instances]->internal.watchdog_ms;
        userdata[plugin_instances]->internal_watchdog_s = userdata[plugin_instances]->internal.watchdog_s;

        // External

        // Transfert internal -> external
        userdata[plugin_instances]->external.seconds = bin2bcd(userdata[plugin_instances]->internal.seconds);
        userdata[plugin_instances]->external.minutes = bin2bcd(userdata[plugin_instances]->internal.minutes);
        userdata[plugin_instances]->external.hours   = bin2bcd(userdata[plugin_instances]->internal.hours);
        userdata[plugin_instances]->external.day     = bin2bcd(userdata[plugin_instances]->internal.day);
        userdata[plugin_instances]->external.date    = bin2bcd(userdata[plugin_instances]->internal.date);

        userdata[plugin_instances]->external.month   = bin2bcd(userdata[plugin_instances]->internal.month & 0x1f) | (userdata[plugin_instances]->internal.month & 0xe0);
        userdata[plugin_instances]->external.year   = bin2bcd(userdata[plugin_instances]->internal.year);
        userdata[plugin_instances]->external.century   = bin2bcd(userdata[plugin_instances]->internal.century);

        userdata[plugin_instances]->external.alarm_hours = bin2bcd(userdata[plugin_instances]->internal.alarm_hours & 0x7f) | (userdata[plugin_instances]->internal.alarm_hours & 0x80);
        userdata[plugin_instances]->external.alarm_minutes = bin2bcd(userdata[plugin_instances]->internal.alarm_minutes & 0x7f) | (userdata[plugin_instances]->internal.alarm_minutes & 0x80);
        userdata[plugin_instances]->external.alarm_seconds = bin2bcd(userdata[plugin_instances]->internal.alarm_seconds & 0x7f) | (userdata[plugin_instances]->internal.alarm_seconds & 0x80);
        userdata[plugin_instances]->external.alarm_day_date = bin2bcd(userdata[plugin_instances]->internal.alarm_day_date & 0x3f) | (userdata[plugin_instances]->internal.alarm_day_date & 0xC0);


        // Watchdog
        userdata[plugin_instances]->external.watchdog_ms = bin2bcd(userdata[plugin_instances]->internal.watchdog_ms);
        userdata[plugin_instances]->external.watchdog_s  = bin2bcd(userdata[plugin_instances]->internal.watchdog_s);


        // Initialisation des compteurs
        userdata[plugin_instances]->clock_us = 10000;
        userdata[plugin_instances]->clock_ms = 100;

        // Pas de transfert external -> internal en attente
        userdata[plugin_instances]->pending_transfert = SDL_FALSE;

        // Temporaire pour tests
        // userdata[plugin_instances]->internal.hours = 0;
        // userdata[plugin_instances]->internal.minutes = 0;
        // userdata[plugin_instances]->internal.seconds = 0;

      return ++plugin_instances;
    }

    return 0;
}

    // -----------------------------------------------------------------------------
    //
    // -----------------------------------------------------------------------------
SDL_bool ds1501_shutdown(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return SDL_FALSE;

    free(userdata[instance]);
    free(userdata_old[instance]);

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool ds1501_reset(struct expansion_bus *oric_bus, unsigned int instance)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]

    dbg_printf("stack_reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                          Lecture du DS1501
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
Uint8 ds1501_read(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 addr, SDL_bool run)
{
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return (Uint8) 0;

    instance--;

    switch (addr)
    {
       // Seconds
        case 0x00:
            return userdata[instance]->external.seconds;
            break;

        // Minutes
        case 0x01:
            return userdata[instance]->external.minutes;
            break;

        // Hours
        case 0x02:
            return userdata[instance]->external.hours;
            break;

        // Day
        case 0x03:
            return userdata[instance]->external.day;
            break;

        // Date
        case 0x04:
            return userdata[instance]->external.date;
            break;

        // Month
        case 0x05:
            return userdata[instance]->external.month;
            break;

        // Year
        case 0x06:
            return userdata[instance]->external.year;
            break;

        // Century
        case 0x07:
            return userdata[instance]->external.century;
            break;

        // Alarm Seconds
        case 0x08:
            return userdata[instance]->external.alarm_seconds;
            break;

        // Alarm Minutes
        case 0x09:
            return userdata[instance]->external.alarm_minutes;
            break;

        // Alarm Hours
        case 0x0a:
            return userdata[instance]->external.alarm_hours;
            break;

        // Alarm Day/Date
        case 0x0b:
            return userdata[instance]->external.alarm_day_date;
            break;

        // Watchdog
        case 0x0c:
            return userdata[instance]->external.watchdog_ms;
            break;

        // Watchdog
        case 0x0d:
            return userdata[instance]->external.watchdog_s;
            break;

        // Control A
        // BLF1 | BLF2 | PRS | PAB | TDF | KSF | WDF | IRQF
        case 0x0e:
        {
            Uint8 data = userdata[instance]->control_a;
            if (run)
            {
                userdata[instance]->control_a &= ~(TDF_mask|KSF_mask|WDF_mask|IRQF_mask);
                *oric_bus->irq &= ~IRQF_DS1501;
            }
            return data;
        }

        // Contorl B
        // TE | CS | BME | TPE | TIE | KIE | WDE | WDS
        case 0x0f:
            return userdata[instance]->control_b;
            break;

        // RAM Addres
        case 0x10:
            return userdata[instance]->ram_address;
            break;

        // Reserved
        case 0x11:
        case 0x12:
            return (Uint8) 0;
            break;

        // RAM Data
        case 0x13:
            if ( (userdata[instance]->control_b & BME_mask) && run )
                return userdata[instance]->ram[userdata[instance]->ram_address++];

            return userdata[instance]->ram[userdata[instance]->ram_address];
            break;

        // Reserved
        // case 0x14 ... 0x1f:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1a:
        case 0x1b:
        case 0x1c:
        case 0x1d:
        case 0x1e:
        case 0x1f:
            return (Uint8) 0;
            break;

        default:
            dbg_printf("DS1501 READ: bad address $%04x\n", addr);
            return (Uint8) 0;
    }
}

    // -------------------------------------------------------------------------
    //                      Ecriture dans le DS1501
    // -------------------------------------------------------------------------
    // Actuellement, seuls les registres Date/Heure sont pris en compte pour TE
    //
    // TODO TE:
    //      - Vérifier si les autres doivent l'être également
    //      - Vérifier si tous les registres doivent être transférer à chaque fois
    //        ou si il ne faut considérer que ceux qui ont été modifiés entre TE=0
    //        et TE=1

SDL_bool ds1501_write(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 addr, Uint8 data)
{
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    switch (addr)
    {
       // Seconds
        case 0x00:
            // 00-59: Vérifier ce qu'il se passe si on est hors limites
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.seconds = data & 0x7f;

            if (userdata[instance]->control_b & TE_mask)
                userdata[instance]->internal.seconds = bcd2bin(userdata[instance]->external.seconds);
            else
                userdata[instance]->pending_transfert = SDL_TRUE;

            break;

        // Minutes
        case 0x01:
            // 00-59: Vérifier ce qu'il se passe si on est hors limites
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.minutes = data & 0x7f;

            if (userdata[instance]->control_b & TE_mask)
                userdata[instance]->internal.minutes = bcd2bin(userdata[instance]->external.minutes);
            else
                userdata[instance]->pending_transfert = SDL_TRUE;

            break;

        // Hours
        case 0x02:
            // 00-23: Vérifier ce qu'il se passe si on est hors limites
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.hours = data & 0x3f;

            if (userdata[instance]->control_b & TE_mask)
                userdata[instance]->internal.hours = bcd2bin(userdata[instance]->external.hours);
            else
                userdata[instance]->pending_transfert = SDL_TRUE;

            break;

        // Day
        case 0x03:
            // 01-07: Vérifier ce qu'il se passe si on est hors limites
            userdata[instance]->external.day = data & 0x07;

            if (userdata[instance]->control_b & TE_mask)
                userdata[instance]->internal.day = bcd2bin(userdata[instance]->external.day);
            else
                userdata[instance]->pending_transfert = SDL_TRUE;

            break;

        // Date
        case 0x04:
            // 01-31: Vérifier ce qu'il se passe si on est hors limites
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.date = data & 0x3f;

            if (userdata[instance]->control_b & TE_mask)
                userdata[instance]->internal.date = bcd2bin(userdata[instance]->external.date);
            else
                userdata[instance]->pending_transfert = SDL_TRUE;

            break;

        // Month
        case 0x05:
            // 01-12: Vérifier ce qu'il se passe si on est hors limites
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.month = data;

            if (userdata[instance]->control_b & TE_mask)
            {
                userdata[instance]->internal.month = (userdata[instance]->external.month & 0xe0) | bcd2bin(userdata[instance]->external.month & 0x1f);
                dbg_printf("month_internal = %02x, external = %02x\n", userdata[instance]->internal.month, userdata[instance]->external.month);
                dbg_printf("external & 0xe0 = %02X, bcd2bin(%02X) = %02X\n", userdata[instance]->external.month & 0xe0, userdata[instance]->external.month & 0x1f, bcd2bin(userdata[instance]->external.month & 0x1f));
                dbg_printf("OSC = %02x\n", userdata[instance]->internal.month & EOSC_mask);
            }
            else
                userdata[instance]->pending_transfert = SDL_TRUE;

            break;

        // Year
        case 0x06:
            // 00-99: Vérifier ce qu'il se passe si on est hors limites
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.year = data;

            if (userdata[instance]->control_b & TE_mask)
                userdata[instance]->internal.year = bcd2bin(userdata[instance]->external.year);
            else
                userdata[instance]->pending_transfert = SDL_TRUE;

            break;

        // Century
        case 0x07:
            // 00-39: Vérifier ce qu'il se passe si on est hors limites
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.century = data;

            if (userdata[instance]->control_b & TE_mask)
                userdata[instance]->internal.century = bcd2bin(userdata[instance]->external.century);
            else
                userdata[instance]->pending_transfert = SDL_TRUE;

            break;


        // Alarm Seconds
        case 0x08:
            // 00-59: Vérifier ce qu'il se passe si on est hors limites
            userdata[instance]->external.alarm_seconds = data;
            userdata[instance]->internal.alarm_seconds = data;
            break;

        // Alarm Minutes
        case 0x09:
            // 00-59: Vérifier ce qu'il se passe si on est hors limites
            userdata[instance]->external.alarm_minutes = data;
            userdata[instance]->internal.alarm_minutes = data;
            break;

        // Alarm Hours
        case 0x0a:
            // 00-23: Vérifier ce qu'il se passe si on est hors limites
            userdata[instance]->external.alarm_hours = data & 0xbf;
            userdata[instance]->internal.alarm_hours = data & 0xbf;
            break;

        // Alarm Day/Date
        case 0x0b:
            // 01-07 / 01-31: Vérifier ce qu'il se passe si on est hors limites
           userdata[instance]->external.alarm_day_date = data;
           userdata[instance]->internal.alarm_day_date = data;
            break;

        // Watchdog
        case 0x0c:
            // 00-99: Vérifier ce qu'il se passe si on est hors limites
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.watchdog_ms = data;
            userdata[instance]->internal.watchdog_ms = bcd2bin(data);
            userdata[instance]->internal_watchdog_ms = bcd2bin(data);
            break;

        // Watchdog
        case 0x0d:
            // 00-99: Vérifier ce qu'il se passe si on est hors limites
            // data = (data & 0x0f) + (data >> 4)*10;
            userdata[instance]->external.watchdog_s = data;
            userdata[instance]->internal.watchdog_s = bcd2bin(data);
            userdata[instance]->internal_watchdog_s = bcd2bin(data);
            break;

        // Control A
        // BLF1 | BLF2 | PRS | PAB | TDF | KSF | WDF | IRQF
        case 0x0e:
            // Les bits BLF1 et BLF2 sont read-only
            // Les bits PRS, PAB, KSF sont rw
            // Les bits TDF et WDF peuvent être mis à 0 uniquement
            // Le bit IRQF dépend de TDF, KSF et WDF
            // Permettre la modification de ces flags via le moniteur?
            userdata[instance]->control_a = (data & ~(BLF1_mask | BLF2_mask)) | (userdata[instance]->control_a & (BLF1_mask | BLF2_mask));

            if (userdata[instance]->control_a & KSF_mask)
            {
                    userdata[instance]->control_a &= ~PAB_mask;

                    // TODO: Vérifier si le flag IRQF et l'IRQ sont levés même si TE=0
                    if ( userdata[instance]->control_b & KIE_mask)
                    {
                        userdata[instance]->control_a |= IRQF_mask;
                        *oric_bus->irq |= IRQF_DS1501;
                    }
            }
            break;

        // Contorl B
        // TE | CS | BME | TPE | TIE | KIE | WDE | WDS
        case 0x0f:
            userdata[instance]->control_b = data;
            break;

        // RAM Addres
        case 0x10:
            userdata[instance]->ram_address = data;
            break;

        // Reserved
        case 0x11:
        case 0x12:
            break;

        // RAM Data
        case 0x13:
            if (userdata[instance]->control_b & BME_mask)
                userdata[instance]->ram[userdata[instance]->ram_address++] = data;

            else
                userdata[instance]->ram[userdata[instance]->ram_address] = data;

            break;

        // Reserved
        // case 0x14 ... 0x1f:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1a:
        case 0x1b:
        case 0x1c:
        case 0x1d:
        case 0x1e:
        case 0x1f:
            break;

        default:
            dbg_printf("DS1501 WRITE: bad address $%04x\n", addr);
            return SDL_FALSE;
    }

    // Watchdog reload
    // userdata[instance]->internal.watchdog_s = userdata[instance]->internal_watchdog_s;
    // userdata[instance]->internal.watchdog_ms = userdata[instance]->internal_watchdog_ms;

    dbg_printf("DS1501: internal.century=%d, external.century=%d, TE_pending=%d\n", userdata[instance]->internal.century, userdata[instance]->external.century, (int)userdata[instance]->pending_transfert );
    dbg_printf("DS1501: internal.year=%d, external.year=%d\n", userdata[instance]->internal.year, userdata[instance]->external.year );
    dbg_printf("DS1501: TE=%d\n", userdata[instance]->control_b & TE_mask);

    // Transfert vers external
    if( (userdata[instance]->control_b & TE_mask) && userdata[instance]->pending_transfert )
    {
        dbg_printf("DS1501: transfert\n");
    	// Heure
        userdata[instance]->internal.seconds = bcd2bin(userdata[instance]->external.seconds);
        userdata[instance]->internal.minutes = bcd2bin(userdata[instance]->external.minutes);
        userdata[instance]->internal.hours = bcd2bin(userdata[instance]->external.hours);

	    // Date
        userdata[instance]->internal.day = bcd2bin(userdata[instance]->external.day);

        userdata[instance]->internal.date = bcd2bin(userdata[instance]->external.date);
        userdata[instance]->internal.month = (userdata[instance]->external.month & 0xe0) | bcd2bin(userdata[instance]->external.month & 0x1f);
        userdata[instance]->internal.year = bcd2bin(userdata[instance]->external.year);
        userdata[instance]->internal.century = bcd2bin(userdata[instance]->external.century);

        userdata[instance]->pending_transfert = SDL_FALSE;
    }

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                              Horloge
    // -------------------------------------------------------------------------
void ds1501_ticktock(struct expansion_bus *oric_bus, unsigned int instance, int cycles)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    if ( !cycles || (userdata[instance]->internal.month & EOSC_mask) )
        return;

    userdata[instance]->clock_us -= cycles;

    // 10ms
    if (userdata[instance]->clock_us <= 0)
    {
        // 10 ms
        userdata[instance]->clock_us += 10000;

        userdata[instance]->clock_ms--;

        // Décrémenter le Watchdog si il n'est pas désactivé
        if (userdata[instance]->internal.watchdog_s || userdata[instance]->internal.watchdog_ms)
        {
            if (!userdata[instance]->internal.watchdog_ms)
            {
                userdata[instance]->internal.watchdog_ms = 99;

                // Si watchdog_s est à 0, on repart à 100 -1
                if (!userdata[instance]->internal.watchdog_s )
                    userdata[instance]->internal.watchdog_s = 100;

                userdata[instance]->internal.watchdog_s--;
            }
            else
                userdata[instance]->internal.watchdog_ms--;

            // Time out? (mise à jour des flags uniquement si TE=1
            if (( userdata[instance]->internal.watchdog_ms == 0) && (userdata[instance]->internal.watchdog_s == 0) )
            {
                if (userdata[instance]->control_b & TE_mask)
                {
                    userdata[instance]->control_a |= WDF_mask;

                    if (userdata[instance]->control_b & WDE_mask)
                    {
                        if (userdata[instance]->control_b & WDS_mask)
                        {
                            dbg_printf("DS1501: Watchdog reset system\n");

                            // oric_bus->reset = SDL_TRUE;

                            // WDE est mis à 0 après après l'impulsion RST
                            userdata[instance]->control_b &= ~WDE_mask;
                        }
                        else
                        {
                            dbg_printf("DS1501: Watchdog fire IRQ\n");

                            // userdata[instance]->internal.watchdog_ms = userdata[instance]->internal_watchdog_ms;
                            // userdata[instance]->internal.watchdog_s = userdata[instance]->internal_watchdog_s;

                            // IRQ levée en fin de fonction
                            // Tester un changement d'état de IRQF di on veut lever l'IRQ
                            //uniquement sur un changement d'état de IRQ 0->1
                            *oric_bus->irq |= IRQF_DS1501;
                        }

                        userdata[instance]->control_a |= IRQF_mask;
                    }
                }
                // Reload Watchdog
                // TODO: Vérifier si il faut le recharger au passage suivant plutôt que maintenant
                // (sinon le si le CPU lit les registres Watchdogs à la reception de l'IRQ il ne verra pas 00.00)
                userdata[instance]->internal.watchdog_ms = userdata[instance]->internal_watchdog_ms;
                userdata[instance]->internal.watchdog_s = userdata[instance]->internal_watchdog_s;
            }
        }

        // 1s
        if (userdata[instance]->clock_ms <= 0)
        {
            // 1s
            userdata[instance]->clock_ms += 100;

            // Mise à jour de l'horloge
            userdata[instance]->internal.seconds++;

            if (userdata[instance]->internal.seconds == 60)
            {
                userdata[instance]->internal.seconds = 0;
                userdata[instance]->internal.minutes++;

                if (userdata[instance]->internal.minutes == 60)
                {
                    userdata[instance]->internal.minutes = 0;
                    userdata[instance]->internal.hours++;

                    if (userdata[instance]->internal.hours == 24)
                    {
                        userdata[instance]->internal.hours = 0;

                        // Day of Week
                        userdata[instance]->internal.day = ((userdata[instance]->internal.day+1) % 7)+1;

                        // Date
                        userdata[instance]->internal.date += 1;

                        Uint8 month_flags = userdata[instance]->internal.month & 0xe0;
                        userdata[instance]->internal.month = userdata[instance]->internal.month & 0x1f;

                        switch  (userdata[instance]->internal.date)
                        {
                            case 29:
                                if (userdata[instance]->internal.month == 2)
                                    if ( (userdata[instance]->internal.century * 100 + userdata[instance]->internal.year) % 4 )
                                    {
                                        userdata[instance]->internal.date = 1;
                                        userdata[instance]->internal.month++;
                                    }
                                break;

                            case 30:
                                if (userdata[instance]->internal.month == 2)
                                {
                                    userdata[instance]->internal.date = 1;
                                    userdata[instance]->internal.month++;
                                }
                                break;

                            case 31:
                                switch(userdata[instance]->internal.month)
                                {
                                    case 2:
                                    case 4:
                                    case 6:
                                    case 9:
                                    case 11:
                                        userdata[instance]->internal.date = 1;
                                        userdata[instance]->internal.month++;
                                }
                                break;

                            case 32:
                                userdata[instance]->internal.date = 1;
                                userdata[instance]->internal.month++;
                                break;
                        }

                        if (userdata[instance]->internal.month == 13)
                        {
                            userdata[instance]->internal.month = 1;
                            userdata[instance]->internal.year++;

                            if (userdata[instance]->internal.year == 100)
                            {
                                userdata[instance]->internal.year = 0;
                                userdata[instance]->internal.century = (++userdata[instance]->internal.century) % 40;
                            }
                        }
                        userdata[instance]->internal.month |= month_flags;
                    }
                }
            }

            // Transfert vers external
            if (userdata[instance]->control_b & TE_mask)
            {
                userdata[instance]->external.seconds = bin2bcd(userdata[instance]->internal.seconds);
                userdata[instance]->external.minutes = bin2bcd(userdata[instance]->internal.minutes);
                userdata[instance]->external.hours = bin2bcd(userdata[instance]->internal.hours);
                userdata[instance]->external.day = bin2bcd(userdata[instance]->internal.day);
                userdata[instance]->external.date = bin2bcd(userdata[instance]->internal.date);

                userdata[instance]->external.month = (userdata[instance]->internal.month & 0xe0) | bin2bcd(userdata[instance]->internal.month & 0x1f);
                userdata[instance]->external.year = bin2bcd(userdata[instance]->internal.year);
                userdata[instance]->external.century = bin2bcd(userdata[instance]->internal.century);

            }
            // Gestion de l'alarme
            Uint8 AMx = ((userdata[instance]->internal.alarm_day_date & AM4_mask) >> 4) | ((userdata[instance]->internal.alarm_hours & AM3_mask) >> 5) | ((userdata[instance]->internal.alarm_minutes & AM2_mask) >> 6) | ((userdata[instance]->internal.alarm_seconds & AM1_mask) >> 7);

            SDL_bool alarm = SDL_TRUE;

            switch (AMx)
            {
                case 0x00:
                    // When day, hours, minutes and seconds match
                    if (userdata[instance]->internal.alarm_day_date & DYDT_mask)
                        alarm &= (userdata[instance]->internal.day == bcd2bin(userdata[instance]->internal.alarm_day_date & 0x3f));

                    // When date, hours, minutes and seconds match
                    else
                        alarm &= (userdata[instance]->internal.date == bcd2bin(userdata[instance]->internal.alarm_day_date & 0x3f));

                // When hours, minutes and seconds match
                case 0x08:
                    alarm &= (userdata[instance]->internal.hours == bcd2bin(userdata[instance]->internal.alarm_hours));

                // When minutes and seconds match
                case 0x0c:
                    alarm &= (userdata[instance]->internal.minutes == bcd2bin(userdata[instance]->internal.alarm_minutes));

                // When seconds match
                case 0x0e:
                    alarm &= (userdata[instance]->internal.seconds == bcd2bin(userdata[instance]->internal.alarm_seconds));
                    break;

                 // Once per second
                case 0x0f:
                    break;

               default:
                    dbg_printf("DS1501: AMx invalid %02X\n", AMx);
            }

            // Mise à jour des flags uniquement si TE=1
            if (alarm && (userdata[instance]->control_b & TE_mask) )
            {
                userdata[instance]->control_a |= TDF_mask;

                if (userdata[instance]->control_b & TIE_mask)
                {
                    dbg_printf("DS1501: Alarm IRQ\n");
                    userdata[instance]->control_a |= IRQF_mask;

                    // IRQ levée en fin de fonction?
                    // Tester un changement d'état de IRQF di on veut lever l'IRQ
                    //uniquement sur un changement d'état de IRQ 0->1
                    // oric_bus->cpu->irq |= IRQF_DS1501;
                    *oric_bus->irq |= IRQF_DS1501;
                }

                if (userdata[instance]->control_b & TPE_mask)
                {
                    userdata[instance]->control_a &= ~PAB_mask;
                    dbg_printf("DS1501: TPE -> -PWR\n");
                }
            }
        }
    }

    // On lève une IRQ tant que IRQF= 1
    // TODO: vérifier si une IRQ est levée uniquement si changement d'état de IRQF: 0->1
    //       ou tant que IRQF =1
    // if ( userdata[instance]->control_a & IRQF_mask)
    //     *oric_bus->irq |= IRQF_DS1501;
}

    // -------------------------------------------------------------------------
    //                  Mise à jour de la page du moniteur
    // -------------------------------------------------------------------------
    //    ....+....|....+....|....+...
    //    Base address : $0360
    //    OSC          : Enable
    //    Clock        : 00ms 1234us
    //    ----------------------------
    //    CtrA: $xx %10101010
    //    CtrB: $xx %01010101
    //    Date: 01/02/2024  02/02/2022
    //    Hour:   00:00:00    01:01:01
    //
    //    Alrm: 00:00:00      11:11:11
    //    DtDy:       28            28
    //    AMod:    %1010         %1010
    //
    //    Wddg:      99.99       99.99
    //    RAM : $00
    //         00 11 22 33 44 55 66 77
    //         88 99 aa bb cc dd ee ff
    //         00 11 22 33 44 55 66 77

void mon_ds1501_update(struct textzone *tz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    Uint8 AMx = ((userdata[instance]->external.alarm_day_date & AM4_mask) >> 4) | ((userdata[instance]->external.alarm_hours & AM3_mask) >> 5) | ((userdata[instance]->external.alarm_minutes & AM2_mask) >> 6) | ((userdata[instance]->external.alarm_seconds & AM1_mask) >> 7);
    Uint8 AMx_old;

    int i;

    dbg_printf("DS1501: mon update\n");

    my_tzprintfpos( tz, 2, 1, "Base address: %04X", base_addr);
    my_tzprintfpos( tz, 2, 2, "OSC         : %s", (userdata[instance]->internal.month & EOSC_mask ? "Disable" : "Enable") );
    my_tzprintfpos( tz, 2, 3, "Clock       : %3dms %5dus", userdata[instance]->clock_ms, userdata[instance]->clock_us);

    // Trait de séparation en ligne 4
    tz->px = 0;
    tz->py = 4;
    my_tzputc( tz, 6 );

    for (i=0; i < tz->w-2; i++)
//        my_tzputc( tz, 2 );
        my_tzputc( tz, 12 );

    my_tzputc( tz, 8 );

    // Control registers
    my_tzprintfpos( tz, 2, 6, "CtrA: %02X %%", userdata[instance]->control_a);
    for( i=128; i; i>>=1 )
      my_tzputc(tz, (userdata[instance]->control_a & i)?'1':'0');

    my_tzprintfpos( tz, 2, 7, "CtrB: %02X %%", userdata[instance]->control_b);
    for( i=128; i; i>>=1 )
      my_tzputc(tz, (userdata[instance]->control_b & i)?'1':'0');


    // External

    // Date / Hour
    my_tzprintfpos( tz, 2, 9, "Date: %02X/%02X/%02X%02X", userdata[instance]->external.date, userdata[instance]->external.month & 0x1f, userdata[instance]->external.century, userdata[instance]->external.year);
    my_tzprintfpos( tz, 2, 10, "Hour: %02X:%02X:%02X", userdata[instance]->external.hours, userdata[instance]->external.minutes, userdata[instance]->external.seconds);

    // Alarm
    my_tzprintfpos( tz, 2, 12, "Alrm: %02X:%02X:%02X", userdata[instance]->external.alarm_hours & 0x3f, userdata[instance]->external.alarm_minutes & 0x7f, userdata[instance]->external.alarm_seconds & 0x7f);
    my_tzprintfpos( tz, 2, 13, "DyDt: %02X (%s)", userdata[instance]->external.alarm_day_date & 0x3f, userdata[instance]->external.alarm_day_date & 0x40 ? "Day " : "Date");

    my_tzprintfpos( tz, 2, 14, "AMod: %02X %%", AMx);
    for( i=8; i; i>>=1 )
        my_tzputc(tz, (AMx & i)?'1':'0');

    // Watchdog
    my_tzprintfpos( tz, 2, 16, "Wdg : %02X.%02X", userdata[instance]->external.watchdog_s, userdata[instance]->external.watchdog_ms);

    // RAM
    my_tzprintfpos( tz, 2, 17, "RAM : %02X", userdata[instance]->ram_address);
    for (i=0; i<8; i++)
        my_tzprintfpos( tz, 4+i*3, 18, "%02X", userdata[instance]->ram[i]);

    for (i=0; i<8; i++)
        my_tzprintfpos( tz, 4+i*3, 19, "%02X", userdata[instance]->ram[i+8]);


    // Internals

    // Date / Hour
    my_tzprintfpos( tz, 19, 9, "%02d/%02d/%04d", userdata[instance]->internal.date, userdata[instance]->internal.month & 0x1f, userdata[instance]->internal.century*100 + userdata[instance]->internal.year);
    my_tzprintfpos( tz, 19, 10, "%02d:%02d:%02d", userdata[instance]->internal.hours, userdata[instance]->internal.minutes, userdata[instance]->internal.seconds);

    // Alarm
    my_tzprintfpos( tz, 19, 12, "%02d:%02d:%02d", userdata[instance]->internal.alarm_hours & 0x3f, userdata[instance]->internal.alarm_minutes & 0x7f, userdata[instance]->internal.alarm_seconds & 0x7f);
    my_tzprintfpos( tz, 19, 13, "%02X (%s)", userdata[instance]->external.alarm_day_date & 0x3f, userdata[instance]->external.alarm_day_date & 0x40 ? "Day " : "Date");

    my_tzprintfpos( tz, 19, 14, "%02X %%", AMx);
    AMx = ((userdata[instance]->internal.alarm_day_date & AM4_mask) >> 4) | ((userdata[instance]->internal.alarm_hours & AM3_mask) >> 5) | ((userdata[instance]->internal.alarm_minutes & AM2_mask) >> 6) | ((userdata[instance]->internal.alarm_seconds & AM1_mask) >> 7);
    for( i=8; i; i>>=1 )
        my_tzputc(tz, (AMx & i)?'1':'0');

    // Watchdog
    my_tzprintfpos( tz, 19, 16, "%02d.%02d", userdata[instance]->internal.watchdog_s, userdata[instance]->internal.watchdog_ms);


    if (oldvalid)
    {
        // Control
        if (userdata[instance]->control_a != userdata_old[instance]->control_a)
        {
            int j=12;

            mon_periphmod( 8, 6, 2, tz );

            for( i=128; i; i>>=1, j++ )
                if ((userdata[instance]->control_a & i) != (userdata_old[instance]->control_a & i))
                    mon_periphmod(j, 6, 1, tz);
        }

        if (userdata[instance]->control_b != userdata_old[instance]->control_b)
        {
            int j=12;

            mon_periphmod( 8, 7, 2, tz );

            for( i=128; i; i>>=1, j++ )
                if ((userdata[instance]->control_b & i) != (userdata_old[instance]->control_b & i))
                    mon_periphmod(j, 7, 1, tz);
        }


        // External

        // Date
        if (userdata[instance]->external.date != userdata_old[instance]->external.date)
            mon_periphmod( 8, 9, 2, tz );

        if (userdata[instance]->external.month != userdata_old[instance]->external.month)
            mon_periphmod( 11, 9, 2, tz );

        if (userdata[instance]->external.century != userdata_old[instance]->external.century)
            mon_periphmod( 14, 9, 2, tz );

        if (userdata[instance]->external.year != userdata_old[instance]->external.year)
            mon_periphmod( 16, 9, 2, tz );

        // Hour
        if (userdata[instance]->external.hours != userdata_old[instance]->external.hours)
            mon_periphmod( 8, 10, 2, tz );

        if (userdata[instance]->external.minutes != userdata_old[instance]->external.minutes)
            mon_periphmod( 11, 10, 2, tz );

        if (userdata[instance]->external.seconds != userdata_old[instance]->external.seconds)
            mon_periphmod( 14, 10, 2, tz );

        // Alarm
        if (userdata[instance]->external.alarm_hours != userdata_old[instance]->external.alarm_hours)
            mon_periphmod( 8, 12, 2, tz );

        if (userdata[instance]->external.alarm_minutes != userdata_old[instance]->external.alarm_minutes)
            mon_periphmod( 11, 12, 2, tz );

        if (userdata[instance]->external.alarm_seconds != userdata_old[instance]->external.alarm_seconds)
            mon_periphmod( 14, 12, 2, tz );

        if (userdata[instance]->external.alarm_day_date != userdata_old[instance]->external.alarm_day_date)
        {
            mon_periphmod( 8, 13, 2, tz );
            mon_periphmod( 12, 13, 4, tz );
        }

        AMx = ((userdata[instance]->external.alarm_day_date & AM4_mask) >> 4) | ((userdata[instance]->external.alarm_hours & AM3_mask) >> 5) | ((userdata[instance]->external.alarm_minutes & AM2_mask) >> 6) | ((userdata[instance]->external.alarm_seconds & AM1_mask) >> 7);
        AMx_old = ((userdata_old[instance]->external.alarm_day_date & AM4_mask) >> 4) | ((userdata_old[instance]->external.alarm_hours & AM3_mask) >> 5) | ((userdata_old[instance]->external.alarm_minutes & AM2_mask) >> 6) | ((userdata_old[instance]->external.alarm_seconds & AM1_mask) >> 7);

        if (AMx != AMx_old)
        {
            mon_periphmod( 8, 14, 2, tz );

            int j = 12;
            for( i=8; i; i>>=1, j++ )
                if ((AMx & i) != (AMx_old & i))
                    mon_periphmod(j, 14, 1, tz);
        }

        // WatchDog
        if (userdata[instance]->external.watchdog_s != userdata_old[instance]->external.watchdog_s)
            mon_periphmod( 8, 16, 2, tz );

        if (userdata[instance]->external.watchdog_ms != userdata_old[instance]->external.watchdog_ms)
            mon_periphmod( 11, 16, 2, tz );


        // RAM
        for (i=0; i<8; i++)
           if (userdata[instance]->ram[i] != userdata_old[instance]->ram[i])
                mon_periphmod( 4+i*3, 18, 2, tz );

        for (i=0; i<8; i++)
           if (userdata[instance]->ram[i+8] != userdata_old[instance]->ram[i+8])
                mon_periphmod( 4+i*3, 19, 2, tz );

        // Internal

        // Date
        if (userdata[instance]->internal.date != userdata_old[instance]->internal.date)
            mon_periphmod( 19, 9, 2, tz );

        if (userdata[instance]->internal.month != userdata_old[instance]->internal.month)
            mon_periphmod( 22, 9, 2, tz );

        if (userdata[instance]->internal.century != userdata_old[instance]->internal.century)
            mon_periphmod( 25, 9, 2, tz );

        if (userdata[instance]->internal.year != userdata_old[instance]->internal.year)
            mon_periphmod( 27, 9, 2, tz );

        // Hour
        if (userdata[instance]->internal.hours != userdata_old[instance]->internal.hours)
            mon_periphmod( 19, 10, 2, tz );

        if (userdata[instance]->internal.minutes != userdata_old[instance]->internal.minutes)
            mon_periphmod( 22, 10, 2, tz );

        if (userdata[instance]->internal.seconds != userdata_old[instance]->internal.seconds)
            mon_periphmod( 25, 10, 2, tz );

        // Alarm
        if (userdata[instance]->internal.alarm_hours != userdata_old[instance]->internal.alarm_hours)
            mon_periphmod( 19, 12, 2, tz );

        if (userdata[instance]->internal.alarm_minutes != userdata_old[instance]->internal.alarm_minutes)
            mon_periphmod( 22, 12, 2, tz );

        if (userdata[instance]->internal.alarm_seconds != userdata_old[instance]->internal.alarm_seconds)
            mon_periphmod( 25, 12, 2, tz );

        if (userdata[instance]->external.alarm_day_date != userdata_old[instance]->external.alarm_day_date)
        {
            mon_periphmod( 19, 13, 2, tz );
            mon_periphmod( 23, 13, 4, tz );
        }

        AMx = ((userdata[instance]->internal.alarm_day_date & AM4_mask) >> 4) | ((userdata[instance]->internal.alarm_hours & AM3_mask) >> 5) | ((userdata[instance]->internal.alarm_minutes & AM2_mask) >> 6) | ((userdata[instance]->internal.alarm_seconds & AM1_mask) >> 7);
        AMx_old = ((userdata_old[instance]->internal.alarm_day_date & AM4_mask) >> 4) | ((userdata_old[instance]->internal.alarm_hours & AM3_mask) >> 5) | ((userdata_old[instance]->internal.alarm_minutes & AM2_mask) >> 6) | ((userdata_old[instance]->internal.alarm_seconds & AM1_mask) >> 7);

        if (AMx != AMx_old)
        {
            mon_periphmod( 19, 14, 2, tz );

            int j = 23;
            for( i=8; i; i>>=1, j++ )
                if ((AMx & i) != (AMx_old & i))
                    mon_periphmod(j, 14, 1, tz);
        }

        // WatchDog
        if (userdata[instance]->internal.watchdog_s != userdata_old[instance]->internal.watchdog_s)
            mon_periphmod( 19, 16, 2, tz );

        if (userdata[instance]->internal.watchdog_ms != userdata_old[instance]->internal.watchdog_ms)
            mon_periphmod( 22, 16, 2, tz );
    }

}

    // -------------------------------------------------------------------------
    //                      Sauvegarde de l'état
    // -------------------------------------------------------------------------
void mon_ds1501_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    // Copy data+ptr
    memcpy(userdata_old[instance], userdata[instance], sizeof(struct DS1501));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
Uint8 bin2bcd(Uint8 value)
{
    if (value >= 200)
        value -= 200;

    if (value >= 100)
        value -= 100;

    return ( ((value / 10) << 4) + (value % 10));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "DS1501",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                NULL,
                ds1501_create,
                ds1501_shutdown,
                ds1501_reset,
                ds1501_read,
                ds1501_write,
                ds1501_ticktock,
                mon_ds1501_update,
                mon_ds1501_store,
                NULL,
    };

