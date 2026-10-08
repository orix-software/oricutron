#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>


#include "../../system.h"
#include "../../6502.h"
#include "../../via.h"
#include "../../8912.h"
#include "../../gui.h"
#include "../../disk.h"
#include "../../monitor.h"
#include "../../6551.h"

//#include "../assinie/periph.h"

#include "plugin.h"
#include "../../machine.h"

#define INSTANCE_MAX 1

unsigned char KERNEL_MAX_NUMBER_OF_MALLOC = 9;
unsigned char KERNEL_MALLOC_FREE_CHUNK_MAX = 5;

// From periph.h
extern int periph_find_by_name(char *name);

extern SDL_bool device_enabled_by_id(int id); /* */

#ifdef DEBUG_PLUGIN
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif


#ifndef RDYNAMIC
void (*my_tzprintfpos)( struct textzone *ptz, int x, int y, char *fmt, ... );
void (*my_tzputc)( struct textzone *ptz, char c );
void (*mon_periphmod)( int x, int y, int w, struct textzone *vtz );
#endif


struct orixdebug
{
    struct expansion_bus *oric;
    struct machine *oric_machine;
    unsigned char *mem;
    int stub;
};

struct orixdebug *userdata[INSTANCE_MAX];
struct orixdebug *userdata_old[INSTANCE_MAX];

#define BASE_ADDR 0x03FE
#define END_ADDR 0x03FF


// Nombre d'instances total
unsigned int plugin_instances = 0;

#include "orixdebug.h"

void mon_orixdebug_status(struct textzone *ptz, unsigned int instance, int pos_state, int y)
{
    int i = 0;
    i = y;

}


unsigned int orixdebug_create(struct machine *oric)
{

    int i;
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return 0;
    printf("plugin_instances %d\n", plugin_instances);
    userdata[plugin_instances] = malloc(sizeof(struct orixdebug));
    userdata_old[plugin_instances] = malloc(sizeof(struct orixdebug));

    if (userdata[plugin_instances])
    {
        if (userdata_old[plugin_instances] == NULL)
        {
            free(userdata[plugin_instances]);
            return 0;
        }


        // pthread_t gtk_thread;
        // pthread_create(&gtk_thread, NULL, run_gtk, NULL);



        return ++plugin_instances;
    }

    return 0;
}

void orixdebug_destroy()
{

}

SDL_bool orixdebug_reset(struct expansion_bus *oric_bus, unsigned int instance )
{
    int i;
    dbg_printf("stack_reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    userdata[instance]->oric = (void *)oric_bus;
    userdata[instance]->mem = oric_bus->mem;

    return SDL_TRUE;
}

void mon_orixdebug_update(struct textzone *ptz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    int i, j = 0;

    // Oorix const
    int KERNEL_MAX_FP_PER_PROCESS = 2;
    int PATH_CURRENT_MAX_LEVEL = 4;
    int MAX_LENGTH_OF_FILES = 8+3+1+1;
    int KERNEL_MAX_PROCESS = 4;
    int KERNEL_USERZP_SAVE_LENGTH = 16;
    int KERNEL_MAX_LENGTH_COMMAND = 8;
    int KERNEL_LENGTH_MAX_CMDLINE = 37;

    int MALLOC_BUSY_SIZE_LOW = 0x570;
    int MALLOC_BUSY_SIZE_HIGH = 0x567;
    int MALLOC_BUSY_BEGIN_HIGH = 0x539;
    int MALLOC_BUSY_END_HIGH = 0x54b;
    int MALLOC_BUSY_BEGIN_LOW = 0x542;
    int MALLOC_BUSY_END_LOW = 0x554;
    int MALLOC_FREE_SIZE_HIGH = 0x2ba;
    int MALLOC_FREE_SIZE_LOW = 0x2bf;
    int MALLOC_FREE_BEGIN_HIGH = 0x52a;
    int MALLOC_FREE_BEGIN_LOW = 0x525;
    int MALLOC_FREE_END_HIGH = 0x534;
    int MALLOC_FREE_END_LOW = 0x52f;

    int kernel_process_kernel_process_struct_kernel_fd_opened = 0x58e;
    int kernel_process_kernel_process_struct_fp_ptr = 0x588;
    int kernel_process = 0x579;
    int max_number_of_fp = 2;
    int ptr_fp = 0;
    int ptr_process = 0;
    int f_path = 3;
    int ppid;



    int KERNEL_MAX_PATH_LENGTH;

    int cwd_str;
    int cmdline;

    int begin_malloc = 0;
    int end_malloc = 0;
    int size_malloc = 0;


    int kernel_one_process_struct_ptr_low;
    int kernel_one_process_struct_ptr_high;

    int found = 0;



    KERNEL_MAX_PATH_LENGTH = 9 * PATH_CURRENT_MAX_LEVEL + MAX_LENGTH_OF_FILES;
    kernel_one_process_struct_ptr_low = kernel_process + KERNEL_MAX_PROCESS + 1;
    kernel_one_process_struct_ptr_high = kernel_one_process_struct_ptr_low + KERNEL_MAX_PROCESS;
    cwd_str = KERNEL_MAX_LENGTH_COMMAND + 1;


    cmdline = cwd_str + KERNEL_MAX_PATH_LENGTH +KERNEL_MAX_FP_PER_PROCESS * 2 + KERNEL_USERZP_SAVE_LENGTH;
    ppid = cmdline + KERNEL_LENGTH_MAX_CMDLINE;

    base_addr = base_addr; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;
    j = 0;
    printf("############################################\n");
    printf("#            Restarting ORIX debug         #\n");
    printf("############################################\n");


    int *process_array = (int *)malloc(KERNEL_MAX_PROCESS * sizeof(int));
    printf("########################## PROCESS informations\n");
    for (i = 0; i < KERNEL_MAX_PROCESS; i++)
    {
        ptr_process = userdata[instance]->mem[kernel_one_process_struct_ptr_low + i ] + (userdata[instance]->mem[kernel_one_process_struct_ptr_high + i] * 256);
        printf("PID %d (offset of allocated struct : 0x%x)", i, ptr_process);
        j = 0;
        printf(" PATH : ");
        while ( userdata[instance]->mem[ptr_process + j + cwd_str ] != '\0')
        {
            printf("%c", userdata[instance]->mem[ptr_process + j  + cwd_str ]);
            j++;
        }

        j = 0;
        printf(" commandline : '");
        while ( userdata[instance]->mem[ptr_process + j + cmdline ] != '\0')
        {
            printf("%c", userdata[instance]->mem[ptr_process + j  + cmdline ]);
            j++;
        }
        printf("'");

        printf("\n");
        process_array[i] = ptr_process;
    }


    int *fp_array = (int *)malloc(max_number_of_fp * sizeof(int));
    printf("########################## Filepointers informations\n");
    printf("Current kernel_fd_opened (ch376) : %d\n", userdata[instance]->mem[kernel_process_kernel_process_struct_kernel_fd_opened]);
    for (i = 0; i < max_number_of_fp; i++)
    {
        ptr_fp = userdata[instance]->mem[kernel_process_kernel_process_struct_fp_ptr + i*2  ] + (userdata[instance]->mem[kernel_process_kernel_process_struct_fp_ptr + 1 + i*2 ] * 256);
        printf("ptr fp %d (offset of allocated struct : 0x%x) ", i, ptr_fp);
        j = 0;
        fp_array[i] = ptr_fp;
        if (ptr_fp != 0)
        {
            printf("f_path: ");
            while ( userdata[instance]->mem[ptr_fp + j + f_path] != '\0')
            {
                printf("%c", userdata[instance]->mem[ptr_fp + j + f_path]);
                j++;
            }

        }
        printf("\n");
    }

    printf("########################## MALLOC (allocated memory)\n");
    for (i = 0; i < KERNEL_MALLOC_FREE_CHUNK_MAX; i++)
        if ( userdata[instance]->mem[MALLOC_FREE_BEGIN_HIGH + i] != 0 )
        {
            printf("FREE #%02X%02X:#%02X%02X\n", userdata[instance]->mem[MALLOC_FREE_BEGIN_HIGH + i], userdata[instance]->mem[MALLOC_FREE_BEGIN_LOW + i] , userdata[instance]->mem[MALLOC_FREE_END_HIGH + i], userdata[instance]->mem[MALLOC_FREE_END_LOW + i]);
            my_tzprintfpos( ptz, 2,  j + 3, "FREE #%02X%02X:#%02X%02X\n", userdata[instance]->mem[MALLOC_FREE_BEGIN_HIGH + i], userdata[instance]->mem[MALLOC_FREE_BEGIN_LOW + i] , userdata[instance]->mem[MALLOC_FREE_END_HIGH + i], userdata[instance]->mem[MALLOC_FREE_END_LOW + i]);
            j++;
        }

    for (i = 0; i < KERNEL_MAX_NUMBER_OF_MALLOC; i++)
        if ( userdata[instance]->mem[MALLOC_BUSY_BEGIN_HIGH + i] != 0 )
        {
            found = 0;
            begin_malloc = userdata[instance]->mem[MALLOC_BUSY_BEGIN_LOW + i] + userdata[instance]->mem[MALLOC_BUSY_BEGIN_HIGH + i] * 256;

            printf("BUSY #%02X%02X:#%02X%02X", userdata[instance]->mem[MALLOC_BUSY_BEGIN_HIGH + i], userdata[instance]->mem[MALLOC_BUSY_BEGIN_LOW + i], userdata[instance]->mem[MALLOC_BUSY_END_HIGH + i], userdata[instance]->mem[MALLOC_BUSY_END_LOW + i]);
            for (j = 0; j < max_number_of_fp; j++)
            {
                if (begin_malloc == fp_array[j])
                {
                    printf(" fp:");
                    int t = 0;
                    while ( userdata[instance]->mem[begin_malloc + t + f_path] != '\0')
                    {
                        printf("%c", userdata[instance]->mem[begin_malloc + t + f_path]);
                        t++;
                    }
                    found = 1;
                }

            }
            // Match pid
            for (j = 0; j < KERNEL_MAX_PROCESS; j++)
            {
                if (begin_malloc == process_array[j])
                {
                    printf(" PID %d struct ", j);
                    found = 1;
                }

            }

            if (found == 0) printf(" Specific malloc process");

            printf("\n");
            my_tzprintfpos( ptz, 2,  j + 3, "BUSY #%02X%02X:#%02X%02X\n", userdata[instance]->mem[MALLOC_BUSY_BEGIN_HIGH + i], userdata[instance]->mem[MALLOC_BUSY_BEGIN_LOW + i], userdata[instance]->mem[MALLOC_BUSY_END_HIGH + i], userdata[instance]->mem[MALLOC_BUSY_END_LOW + i]);
            j++;
        }




}

void mon_orixdebug_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;
}


Uint8 orixdebug_read(struct expansion_bus *oric, SDL_bool fBank, unsigned int instance, Uint16 addr, SDL_bool run)
{
    fBank = fBank; // gcc [-Wunused-parameter]
    oric = oric; // gcc [-Wunused-parameter]

    return 0;

}

SDL_bool orixdebug_write(struct expansion_bus *oric, SDL_bool fBank, unsigned int instance, Uint16 addr, Uint8 data)
{
    fBank = fBank; // gcc [-Wunused-parameter]
    oric = oric;  // gcc [-Wunused-parameter]

    return SDL_TRUE;

}


SDL_bool orixdebug_shutdown(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]
    instance = instance; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return SDL_FALSE;

    free(userdata[instance]);

    return SDL_TRUE;
}


SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod)
{
    dbg_printf("---orixddebug init\n");

    my_tzprintfpos = tzprintfpos;
    my_tzputc = tzputc;
    mon_periphmod = _mon_periphmod;

    // Pour savoir si un plugin est chargé periph_find_by_name() retourne un numéro de plugin si il existe et une valeur supérieure au nombre de plugins chargés dans le cas contraire mais tu ne sais pas si il est actif ou non. 
    // periph_find_by_name("Twilighte");
    // device_enabled_by_id(0); /* */
    // Ce qu'il faut faire ensuite c'est faire un appel à device_enabled_by_id() avec la valeur reçue de periph_find_by_name(), en retour tu as en booléen SDL_TRUE si le plugin est actif et SDL_FALSE dans le cas contraire (ou si l'id est incorrect).
    return SDL_TRUE;
}


struct PLUGIN plugin = { "orixdebug",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                // 0 : aucun device
                // PLG_DEVICE  : 1 => indique un périphérique dont l'adresse est dans la page 3
                // PLG_BANK    : 2 => indique un périphérique dont l'adresse est au delà de $BFFF
                // PLG_MULTI   : 4 => indique que le périphérique possède plusieurs adresses non
                0,
                NULL,
                orixdebug_create,
                NULL,
                orixdebug_reset,
                orixdebug_read,
                NULL,
                NULL,
                mon_orixdebug_update,
                NULL,
		        NULL
    };
