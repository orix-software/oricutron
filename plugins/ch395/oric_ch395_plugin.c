#if SDL_MAJOR_VERSION == 1
# ifdef __SPECIFY_SDL_DIR__
# include <SDL/SDL.h>
# else
# include <SDL.h>
# endif
#else /* SDL_MAJOR_VERSION == 1 */
# ifdef __SPECIFY_SDL_DIR__
# include <SDL2/SDL.h>
# else
# include <SDL.h>
# endif
#endif

#include "ch395.h"
#include "stdio.h"

void dbg_printf(char *fmt, ...);

#define CH395_ORIC_EXTENSION_DATA_PORT		0x360
#define CH395_ORIC_EXTENSION_COMMAND_PORT	0x361


struct ch395 * ch395_oric_init()
{
  return  ch395_create(NULL);
}

void ch395_oric_reset(struct ch395 *ch395)
{
  // ch395_reset(ch395);
}


void ch395_oric_destroy(struct ch395 *ch395)
{
  // ch395_destroy(ch395);
}

void ch395_oric_config(struct ch395 *ch395)
{

}

void	ch395_oric_write(struct ch395 *ch395, Uint16 addr, Uint8 data)
{

  if (addr == ch395_ORIC_EXTENSION_DATA_PORT)
  {
    ch395_write_data_port(ch395, data);
  }

  if (addr == ch395_ORIC_EXTENSION_COMMAND_PORT)
  {
  	ch395_write_command_port(ch395, data);
  }
}

unsigned char 	ch395_oric_read(struct ch395 *ch395, Uint16 addr)
{

  if (addr == ch395_ORIC_EXTENSION_DATA_PORT)
  {
	  return ch395_read_data_port(ch395);
  }

  if (addr == ch395_ORIC_EXTENSION_COMMAND_PORT)
  {
    return ch395_read_command_port(ch395);
  }
  return 0;
}
