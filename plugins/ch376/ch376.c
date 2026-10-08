/********************************************************************
 *                                                                  *
 * Very minimal CH376 emulation (21.12.2016)                        *
 *                                                                  *
 * Code:                                                            *
 *   Jérôme 'Jede' Debrune                                          *
 *   Philippe 'OffseT' Rimauro                                      *
 *   Christian 'Assinie' Lardière                                   *
 *                                                                  *
 ** ch376.c *********************************************************/
/*
 Changes:

 23.11.2025 - Jede: add usb devices management
 10.06.2025 - Jede: Fix bug when a char is not normalized for FAT32 operation (open, create, delete file/dir)
 10.03.2023 - Assinie: Fix bug : . and .. was reading as right entry. Now, it's skipped
 02.04.2022 - Assinie: Added support for CMD_REAF_VAR32 (GET_FILE_SIZE and CURRENT_OFFSET only)
 01.02.2021 - Assinie: Fix time struct (Linux Only)
 07.12.2017 - Assinie: Added support for CMD_GET_FILE_SIZE
 06.12.2017 - Assinie: Added support for CMD_DISK_CAPACITY
 13.11.2017 - Assinie: Improve '*' wildcard support for CMD_FILE_OPEN
 05.10.2017 - Assinie: Added support for CMD_DIR_CREATE and CMD_FILE_ERASE (Linux only)
 21.08.2017 - Jede   : Added support for CMD_DIR_CREATE and CMD_FILE_ERASE (WIN32 only)
 22.07.2017 - OffseT : Added support for CMD_DIR_CREATE and CMD_FILE_ERASE (Added related Amiga system APIs only)

 */
/* /// "Portable includes" */

//#define DEBUG_CH376 1

#if defined(__MORPHOS__) || defined (__AMIGA__) || defined (__AROS__)

#define __NOLIBBASE__

#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

extern struct Library *SysBase;

#elif defined(WIN32)

#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include <sys/stat.h>
//@iss #include <shlwapi.h>

#elif defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#else
#error "FixMe!"
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
#include "ch376.h"

#define USB_MOUSE_CLASS        0x03
#define USB_MASS_STORAGE_CLASS 0x08
#define USB_HUB_CLASS          0x03


#define CH376_USB_SPEED_FULL_12MBPS 0x00
#define CH376_USB_SPEED_FULL_1_5MBPS 0x01
#define CH376_USB_SPEED_LOW_1_5MBPS 0x02

#define CH376_MAX_USB_DEVICES 127 // usb can handle 127 for each controler

/* /// */

/* /// "CH376 interface commands and constants" */

// Chip version
#define CH376_DATA_IC_VER 3

// Commands
#define CH376_CMD_NONE          0x00
#define CH376_CMD_GET_IC_VER    0x01
#define CH376_CMD_ENTER_SLEEP   0x03 // Not emulated
#define CH376_CMD_SET_USB_SPEED 0x04
#define CH376_CMD_RESET_ALL     0x05 // Not emulated
#define CH376_CMD_CHECK_EXIST   0x06
#define CH376_CMD_GET_REGISTER  0x0a // Not emulated
#define CH376_CMD_SET_REGISTER  0x0b // or WRITE_VAR8
#define CH376_CMD_READ_VAR32    0x0c
#define CH376_CMD_WRITE_VAR32   0x0d // Not emulated
#define CH376_CMD_DELAY_100US   0x0f // Not emulated
#define CH376_SET_USB_ADDR      0x13 // Not emulated
#define CH376_CMD_SET_USB_MODE  0x15 // Not emulated
#define CH376_CMD_TEST_CONNECT  0x16 // Not emulated
#define CH376_CMD_ABORT_NAK     0x17 // Not emulated
#define CH376_CMD_SET_EP0_RX    0x18 // Not emulated
#define CH376_CMD_SET_EP0_TX    0x19 // Not emulated
#define CH376_CMD_SET_EP1_RX    0x1a // Not emulated
#define CH376_CMD_SET_EP1_TX    0x1b // Not emulated
#define CH376_CMD_SET_EP2_RX    0x1c // Not emulated
#define CH376_CMD_SET_EP2_TX    0x1d // Not emulated
#define CH376_CMD_GET_STATUS    0x22
#define CH376_CMD_UNLOCK_USB    0x23 // Not emulated
#define CH376_DIRTY_BUFFER      0x25 // Not emulated
#define CH376_CMD_RD_USB_DATA0  0x27
#define CH376_CMD_RD_USB_DATA_UNLOCK     0x28
#define CH376_CMD_WR_EP0        0x29 // DATA3
#define CH376_CMD_WR_EP1        0x2a // DATA5
#define CH376_CMD_WR_EP2        0x2b // DATA7
#define CH376_WR_USB_DATA       0x2c
#define CH376_CMD_WR_REQ_DATA   0x2d
#define CH376_OFS_DATA          0x2e // Not emulated
#define CH376_CMD_SET_FILE_NAME 0x2f
#define CH376_CMD_DISK_CONNECT  0x30 // Not emulated
#define CH376_CMD_DISK_MOUNT    0x31
#define CH376_CMD_FILE_OPEN     0x32
#define CH376_CMD_FILE_ENUM_GO  0x33
#define CH376_CMD_FILE_CREATE   0x34
#define CH376_CMD_FILE_ERASE    0x35
#define CH376_CMD_FILE_CLOSE    0x36
#define CH376_CMD_DIR_INFO_READ 0x37
#define CH376_DIR_INFO_SAVE     0x38
#define CH376_CMD_BYTE_LOCATE   0x39
#define CH376_CMD_BYTE_READ     0x3a
#define CH376_CMD_BYTE_RD_GO    0x3b
#define CH376_CMD_BYTE_WRITE    0x3c
#define CH376_CMD_BYTE_WR_GO    0x3d
#define CH376_CMD_DISK_CAPACITY 0x3e
#define CH376_CMD_DISK_QUERY    0x3f
#define CH376_CMD_DIR_CREATE    0x40
#define CH376_CMD_SET_ADDR      0X45
#define CH376_CMD_GET_DESCR     0x46
#define CH376_CMD_SET_CONFIG    0x49
#define CH376_SEC_READ          0x4b
#define CH376_SEC_WRITE         0x4c
#define CH376_CMD_AUTO_SETUP    0x4d
#define CH376_CMD_ISSUE_TKN_X   0x4e
#define CH376_CMD_DISK_RD_GO    0x55
#define CH376_DISK_WR_GO        0x57
#define CH376_DISK_INQUIRY      0x58
#define CH376_DISK_READY        0x59
#define CH376_DISK_R_SENSE      0x5a
#define CH376_RD_DISK_SEC       0x5b
#define CH376_WR_DISK_SEC       0x5c
#define CH376_DISK_MAX_LUN      0x5d

#define CH376_ARG_SET_USB_MODE_INVALID  0x00
#define CH376_USB_MODE_DEVICE_OUTER_FW  0x01
#define CH376_USB_MODE_DEVICE_INNER_FW  0x02
#define CH376_ARG_SET_USB_MODE_SD_HOST  0x03
#define CH376_ARG_SET_USB_MODE_USB_HOST 0x06
#define CH376_ARG_SET_USB_HOST_RESET_USB_BUS 0x07

// VAR32 offsets
#define CH376_VAR_FILE_SIZE      0x68
#define CH376_VAR_CURRENT_OFFSET 0x6c

// Status & errors
#define CH376_ERR_OPEN_DIR     0x41
#define CH376_ERR_MISS_FILE    0x42
#define CH376_ERR_FOUND_NAME   0x43
#define CH376_ERR_DISK_DISCON  0x82
#define CH376_ERR_LARGE_SECTOR 0x84
#define CH376_ERR_TYPE_ERROR   0x92
#define CH376_ERR_BPB_ERROR    0xa1
#define CH376_ERR_DISK_FULL    0xb1
#define CH376_ERR_FDT_OVER 	   0xb2
#define CH376_ERR_FILE_CLOSE   0xb4

#define CH376_RET_SUCCESS 0x51
#define CH376_RET_ABORT   0x5f

#define CH376_INT_SUCCESS        0x14
#define CH376_USB_INT_CONNECT 	 0x15
#define CH376_USB_INT_DISCONNECT 0x16
#define CH376_USB_INT_BUF_OVER 	 0x17
#define CH376_USB_INT_USB_READY  0x18
#define CH376_INT_DISK_READ      0x1d
#define CH376_INT_DISK_WRITE     0x1e

/* Basic information of the current system */
/* Bit 6 is used to indicate the subclass of the USB storage device SubClass-Code; bit 6 is 0 to indicate that the subclass is 6, and bit 6 is 1 to indicate that the subclass is different from 6 */
/* Bit 5 is used to indicate the USB configuration status in USB device mode and the USB device connection status in USB host mode */
/* In USB device mode, if bit 5 is 1, the USB configuration is complete, and bits 5 and 0 are not configured */
/* In USB host mode, if bit 5 is 1, there is a USB device in the USB port, and if bit 5 is 0, there is no USB device in the USB port */
/* Bit 4 is used to indicate the buffer lock status in USB device mode. Bit 4 being 1 means the USB buffer is locked, and bit 6 being 1 means it has been released */
/* Other bits are reserved; please do not modify */
#define VAR_SYS_BASE_INFO           0x20

/* Number of USB transaction operation attempts */
/* If bit 7 is 0, it will not retry when NAK is received; if bit 7 is 1 and bit 6 is 0, it will retry infinitely upon receiving NAK (you can use the CMD_ABORT_NAK command to abandon the retry), if bit 7 is 1 and bit 6 is 1, it will retry for up to 3 seconds upon receiving NAK */
/* Bit 5 to Bit 0 represents the number of retry attempts after the timeout expires */
#define VAR_RETRY_TIMES             0x25

/* Bit indicator in host file mode */
/* Bit 1 and Bit 0: Indicator of the logical disk's FAT file system, 00-FAT12, 01-FAT16, 10-FAT32, 11-illegal */
/* Bit 2: Indicates whether the FAT table data in the current buffer has been modified, 0-not modified, 1-modified */
/* Bit 3: The file length needs to be modified; the current file is appended with data, 0-no modification is not appended, 1-appended and needs to be modified */
/* Other bits are reserved; please do not modify */
#define VAR_FILE_BIT_FLAG           0x26

/* Status of disk and file in host file mode */
/* Bit indicator of the SD card in host file mode */
#define VAR_SD_BIT_FLAG             0x30

/* Bit 0: SD card version, 0-only supports the first SD version, 1-supports the second SD version */
/* Bit 1: Auto recognition, 0-SD card, 1-MMC card */
/* Bit 2: Auto identification, 0-standard capacity SD card, 1-high capacity (HC-SD) SD card */
/* Bit 4: Timeout for ACMD41 command */
/* Bit 5: Timeout for CMD1 command */
/* Bit 6: Timeout for CMD58 command */
/* Other bits are reserved; please do not modify */
#define VAR_DISK_STATUS             0x2B

/* The synchronization indicator of the BULK-IN / BULK-OUT endpoint of the USB storage device */
/* Bit 7: Bulk endpoint synchronization indicator */
/* Bit 6: Bulk endpoint synchronization indicator */
/* Bit 5 ~ Bit 0: Must be 0 */
#define VAR_UDISK_TOGGLE 0x31

/* The logical unit number of the USB storage device */
/* Bit 7 ~ Bit 4: The current logical unit number of the USB storage device; after CH376 initializes the USB storage device, the default value is to access logical unit #0 */
/* Bit 3 ~ Bit 0: The maximum logical unit number of the USB storage device; plus 1 equals the number of logical units */
#define VAR_UDISK_LUN 0x34

/* The number of sectors per cluster of the logical disk */
#define VAR_SEC_PER_CLUS 0x38
/* The index number of the current file directory information in the sector */
#define VAR_FILE_DIR_INDEX 0x3B

/* The sector offset of the current file pointer in the cluster; 0xFF points to the end of the file, the end of the cluster */
#define VAR_CLUS_SEC_OFS 0x3C

/* 32-bit variable / 4 bytes */
/* For FAT16 disks, this is the number of sectors occupied by the root directory; for FAT32 disks, this is the starting cluster number of the root directory (total length 32 bits, least significant byte first) */
#define VAR_DISK_ROOT 0x44

/* The total number of clusters of the logical disk (total length is 32 bits, least significant byte first) */
#define VAR_DSK_TOTAL_CLUS 0x48

/* The absolute starting sector number of the logical disk LBA (total length 32 bits, least significant byte first) */
#define VAR_DSK_START_LBA 0x4C

/* The starting LBA of the logical disk data area (total length is 32 bits, least significant byte first) */
#define VAR_DSK_DAT_START 0x50

/* LBA corresponding to the data of the current data buffer of the disk (total length 32 bits, least significant byte first) */
#define VAR_LBA_BUFFER 0x54

/* The starting LBA address of the disk currently being read and written (total length is 32 bits, least significant byte first) */
#define VAR_LBA_CURRENT 0x58

/* The LBA address of the sector where the current file directory information is located (total length 32 bits, least significant byte first) */
#define VAR_FAT_DIR_LBA 0x5C

/* The starting cluster number of the current file or directory (folder) (total length 32 bits, least significant byte first) */
#define VAR_START_CLUSTER 0x60

/* The current cluster number of the current file (total length is 32 bits, least significant byte first) */
#define VAR_CURRENT_CLUST 0x64

/* The length of the current file (total length is 32 bits, least significant byte first) */
#define VAR_FILE_SIZE 0x68

/* The current file pointer, the byte offset of the current read and write position (total length 32 bits, least significant byte first) */
#define VAR_CURRENT_OFFSET 0x6C


/* /// */

/* /// "CH376 data structures" */

#define CMD_DATA_REQ_SIZE 0xff

// Attributes
#define DIR_ATTR_READ_ONLY 0x01
#define DIR_ATTR_HIDDEN    0x02
#define DIR_ATTR_SYSTEM    0x04
#define DIR_ATTR_VOLUME_ID 0x08
#define DIR_ATTR_DIRECTORY 0x10
#define DIR_ATTR_ARCHIVE   0x20
// Time = (Hour<<11) + (Minute<<5) + (Second>>1)
#define DIR_MAKE_FILE_TIME(h, m, s) ((h<<11) + (m<<5) + (s>>1))
// Date = ((Year-1980)<<9) + (Month<<5) + Day
#define DIR_MAKE_FILE_DATE(y, m, d) (((y-1980)<<9) + (m<<5) + d)

#pragma pack(1)

// Important: All values in all structures are stored in little-endian (either in 16 or 32 bit)
struct FatDirInfo
{
    char     DIR_Name[11];
    CH376_U8 DIR_Attr;
    CH376_U8 DIR_NTRes;
    CH376_U8 DIR_CrtTimeTenth;
    CH376_U8 DIR_CrtTime[2];
    CH376_U8 DIR_CrtDate[2];
    CH376_U8 DIR_LstAccDate[2];
    CH376_U8 DIR_FstClusHI[2];
    CH376_U8 DIR_WrtTime[2];
    CH376_U8 DIR_WrtDate[2];
    CH376_U8 DIR_FstClusLO[2];
    CH376_U8 DIR_FileSize[4];
};

struct MountInfo
{
    CH376_U8 MOUNT_DeviceType;
    CH376_U8 MOUNT_RemovableMedia;
    CH376_U8 MOUNT_Versions;
    CH376_U8 MOUNT_DataFormatAndEtc;
    CH376_U8 MOUNT_AdditionalLength;
    CH376_U8 MOUNT_Reserved1;
    CH376_U8 MOUNT_Reserved2;
    CH376_U8 MOUNT_MiscFlag;
    char     MOUNT_VendorIdStr[8];
    char     MOUNT_ProductIdStr[16];
    char     MOUNT_ProductRevStr[4];
};

#define USBDEVICE_IS_CONNECTED     1
#define USBDEVICE_IS_NOT_CONNECTED 0

#define ISSUE_TKN_IS_SET     1
#define ISSUE_TKN_IS_NOT_SET 0

struct UsbDevice
{
    CH376_U8 USBDEVICE_Address;
    CH376_U8 USBDEVICE_Config;
    CH376_U8 USBDEVICE_Is_Connected;
};

struct DiskQuery
{
    CH376_U8 DISK_TotalSector[4];
    CH376_U8 DISK_FreeSector[4];
    CH376_U8 DISK_DiskFat;
};

union CommandData
{
    char              CMD_FileName[14];     // [W/O] CH376_CMD_SET_FILE_NAME
    struct MountInfo  CMD_MountInfo;        // [R/O] CH376_CMD_DISK_MOUNT
    struct FatDirInfo CMD_FatDirInfo;       // [R/O] CH376_CMD_FILE_OPEN, CH376_CMD_FILE_ENUM_GO
    struct DiskQuery  CMD_DiskQuery;        // [R/O] CH376_CMD_DISK_QUERY, CH376_CMD_DISK_CAPACITY
    CH376_U8          CMD_FileSeek[4];      // [R/W] CH376_CMD_BYTE_LOCATE
    CH376_U8          CMD_FileReadWrite[2]; // [W/O] CH376_CMD_BYTE_READ, CH376_CMD_BYTE_WRITE
    CH376_U8          CMD_IOBuffer[255];    // [R/W] CH376_CMD_BYTE_READ, CH376_CMD_BYTE_RD_GO, CH376_CMD_BYTE_WRITE, CH376_CMD_BYTE_WR_GO
    CH376_U8          CMD_CheckByte;        // [R/W] CH376_CMD_CHECK_EXIST
    CH376_U8          CMD_VAR32[4];         // [R/W] CH376_CMD_GET_FILE_SIZE, CH376_CMD_SET_FILE_SIZE
};

#pragma pack()

/* /// */

/* /// "CH376 runtime structure" */

struct ch376
{
    CH376_CONTEXT context;

    CH376_U8 command;
    CH376_U8 command_status;

    CH376_U8 interface_status;
    CH376_U8 usb_mode;

    union CommandData cmd_data;
    CH376_U8 nb_bytes_in_cmd_data;
    CH376_U8 pos_rw_in_cmd_data;
    CH376_U8 buffer_read_count;

    CH376_U16 bytes_to_read_write;

    CH376_LOCK root_dir_lock;
    CH376_LOCK current_dir_lock;
    CH376_FILE current_file;
    CH376_DIR current_directory_browsing;

    CH376_BOOL current_file_is_directory;
    char dir_pattern[14];

    char *sdcard_drive_path;
    char *usb_drive_path;

    CH376_S32 current_pos;

    // USB management
    CH376_U8 usb_speed; // Usb speed
    CH376_U8 chip_registers[0x6c]; //Registers

    CH376_U8 current_register_write;
    struct UsbDevice usbdevices[CH376_MAX_USB_DEVICES];
    // Each devices connected has 0 address, when we set usb address, we don't know which one is available when we asked to controler
    // That is why, we need to set the first devices
    // The device is enumerated on the CH376 bus with adress 0
    // current_usb_device_to_set_adress is used to enumerate the next device which has usb_adress_0
    CH376_U8 current_usb_device_to_set_adress;
    CH376_U8 issue_tkn;
    CH376_U8 issue_tkn_is_set;
    CH376_U8 operation_descriptor;
    CH376_U8 device_connected_to_usb_port;
    CH376_U8 usb_data[100];
    CH376_U8 pos_in_usb_data;
    CH376_U8 current_device_address;

    CH376_U8 hid_mouse_deltax;
    CH376_U8 hid_mouse_deltay;

    CH376_U32 hid_mouse_posx;
    CH376_U32 hid_mouse_posy;

};

/* /// */

/* /// "CH376 private prototypes" */

static char *clone_string(const char *string);
static void clear_structure(struct ch376 *ch376);
static void cancel_all_io(struct ch376 *ch376);
static int normalize_char(char c);
static const char * normalize_file_name(const char *file_name, char *normalized_file_name);
static const char * trim_file_name(const char *file_name, char *trimmed_file_name);
static void file_read_chunk(struct ch376 *ch376);
static void file_write_chunk(struct ch376 *ch376);
static CH376_BOOL pattern_match(const char *pattern, const char *str);
static const char * normalize_pattern(const char *pattern, char *normalized_pattern);

/* /// */

/* /// "Portable operating system-dependent prototypes" */

// Allocate "size" byte of memory
/* static */ void * system_alloc_mem(int size);

// Free memory previously allocated with system_alloc_mem
// It is safe to call it with NULL
/* static */ void system_free_mem(void *ptr);

// Initialize the operating system dependent context
// User data may be used if external data is required for such initialization
static CH376_BOOL system_init_context(CH376_CONTEXT *context, UNUSED void *user_data);

// Release the operating system dependent context previously initialized by system_init_context
static void system_clean_context(CH376_CONTEXT *context);

// Returns true if the dir_lock points to the root_dir
static CH376_BOOL system_is_root_dir(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_LOCK root_dir);

// Fill the disk_info structure with information of the disk from which the lock was obtained
static CH376_BOOL system_get_disk_info(CH376_CONTEXT *context, CH376_LOCK root_lock, struct DiskQuery *disk_info);

// Try to lock the directory from the provided lock location and return an associated lock
// Return 0 if a directory could not be locked (not a directory or not existing)
static CH376_LOCK system_obtain_directory_lock(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock);

// Release a lock previouly obtained from system_obtain_directory_lock
// It is safe to call it with a 0
static void system_release_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock);

// Clone a directory lock
static CH376_LOCK system_clone_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock);

// Create a new directory
static CH376_LOCK system_create_directory(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock);

// Return a file handle corresponding to the given file located in the provited directory lock
// Return 0 if the file could not be found
static CH376_FILE system_file_open_existing(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock);

// Create a file in the provited directory lock and return its file handle
// Return 0 if the file could not be created
static CH376_FILE system_file_open_new(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock);

// Close a file opened with system_file_open_existing or system_file_open_new
static void system_file_close(CH376_CONTEXT *context, CH376_FILE file);

// Seek into a file (absolute position from beginning of the file)
static CH376_S32 system_file_seek(CH376_CONTEXT *context, CH376_FILE file, int pos);

// Read a part of a file
static CH376_S32 system_file_read(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size);

// Write some data into a file
static CH376_S32 system_file_write(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size);

// Delete a file
static CH376_BOOL system_file_delete(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock);

// Delete a directory
static CH376_BOOL system_directory_delete(CH376_CONTEXT *context, CH376_LOCK root_lock);

// Start examining a directory
// Return information about the directory itself
static CH376_DIR system_start_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock);

// Get the next entry from a directory on which a examine session was started using system_start_examine_directory
static CH376_BOOL system_go_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_DIR fib, struct FatDirInfo *dir_info, char *pattern);

// Finish a directory examine session (release all related resources)
static void system_finish_examine_directory(CH376_CONTEXT *context, CH376_DIR fib);

// Get the size of the current file
static CH376_S32 system_get_file_size(CH376_CONTEXT *context, CH376_FILE file);

// Get the offset position of the current file
static CH376_S32 system_get_file_offset(CH376_CONTEXT *context, CH376_FILE file);

/* /// */

#if defined(__MORPHOS__) || defined (__AMIGA__) || defined (__AROS__)

/* /// "Amiga system functions" */

#define DEBUG_CH376 1

#ifdef DEBUG_CH376
#include <clib/debug_protos.h>
#define dbg_printf kprintf
#else
#define dbg_printf(...)
#endif

static void * system_alloc_mem(int size)
{
    return AllocVec(size, MEMF_ANY);
}

static void system_free_mem(void *ptr)
{
    FreeVec(ptr);
}

static CH376_BOOL system_init_context(CH376_CONTEXT *context, UNUSED void *user_data)
{
  context->DOSBase = OpenLibrary(DOSNAME, 0L);
  return (context->DOSBase != NULL);
}

static void system_clean_context(CH376_CONTEXT *context)
{
  if(context->DOSBase != NULL)
    CloseLibrary(context->DOSBase);
}

static CH376_BOOL system_is_root_dir(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_LOCK root_dir)
{
    struct Library *DOSBase = context->DOSBase;

    return SameLock(dir_lock, root_dir) == LOCK_SAME;
}

static CH376_BOOL system_get_disk_info(CH376_CONTEXT *context, CH376_LOCK root_lock, struct DiskQuery *disk_info)
{
    struct Library *DOSBase = context->DOSBase;
    BOOL got_info = CH376_FALSE;

    if(disk_info)
    {
        struct InfoData *info = system_alloc_mem(sizeof(struct InfoData));

        if(info)
        {
            if(Info(root_lock, info) == DOSTRUE)
            {
                QUAD total_sector = (info->id_NumBlocks * info->id_BytesPerBlock) / 512;
                QUAD free_sector = ((info->id_NumBlocks - info->id_NumBlocksUsed) * info->id_BytesPerBlock) / 512;

                disk_info->DISK_TotalSector[0] = (total_sector & 0x000000ff) >>  0;
                disk_info->DISK_TotalSector[1] = (total_sector & 0x0000ff00) >>  8;
                disk_info->DISK_TotalSector[2] = (total_sector & 0x00ff0000) >> 16;
                disk_info->DISK_TotalSector[3] = (total_sector & 0xff000000) >> 24;

                disk_info->DISK_FreeSector[0] = (free_sector & 0x000000ff) >>  0;
                disk_info->DISK_FreeSector[1] = (free_sector & 0x0000ff00) >>  8;
                disk_info->DISK_FreeSector[2] = (free_sector & 0x00ff0000) >> 16;
                disk_info->DISK_FreeSector[3] = (free_sector & 0xff000000) >> 24;

                disk_info->DISK_DiskFat = 0x0c; // FAT32?

                got_info = TRUE;
            }
            system_free_mem(info);
        }
    }

    return got_info;
}

static CH376_LOCK system_obtain_directory_lock(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock)
{
    struct Library *DOSBase = context->DOSBase;
    BPTR lock;
    BPTR old_lock = (BPTR)0;

    dbg_printf("system_obtain_directory_lock: %s\n", dir_path);

    if(root_lock)
    {
        old_lock = CurrentDir(root_lock);
    }

    lock = Lock(dir_path, SHARED_LOCK);

    if(lock != (BPTR)0)
    {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);

        if(fib)
        {
            if(Examine(lock, fib) == DOSFALSE || fib->fib_DirEntryType < 0)
            {
                UnLock(lock);
                lock = (BPTR)0;
            }
            FreeDosObject(DOS_FIB, fib);
        }
    }

    if(old_lock)
    {
        CurrentDir(old_lock);
    }

    return lock;
}

static void system_release_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    struct Library *DOSBase = context->DOSBase;
    UnLock(dir_lock);
}

static CH376_LOCK system_clone_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    struct Library *DOSBase = context->DOSBase;
    return DupLock(dir_lock);
}

static CH376_LOCK system_create_directory(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock)
{
    struct Library *DOSBase = context->DOSBase;
    BPTR lock;
    BPTR old_lock = (BPTR)0;

    dbg_printf("system_create_directory: %s\n", dir_path);

    if(root_lock)
    {
        old_lock = CurrentDir(root_lock);
    }

    lock = CreateDir(dir_path);

    if(old_lock)
    {
        CurrentDir(old_lock);
    }

    return lock;
}

static BPTR file_open(CH376_CONTEXT *context, CONST_STRPTR file_name, BPTR root_lock, LONG mode)
{
    struct Library *DOSBase = context->DOSBase;
    BPTR old_lock = (BPTR)0;
    BPTR file;

    if(root_lock)
    {
        old_lock = CurrentDir(root_lock);
    }

    file = Open(file_name, mode);

    if(old_lock)
    {
        CurrentDir(old_lock);
    }

    return file;
}

static CH376_FILE system_file_open_existing(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(context, file_name, root_lock, MODE_OLDFILE);
}

static CH376_FILE system_file_open_new(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(context, file_name, root_lock, MODE_NEWFILE);
}

static void system_file_close(CH376_CONTEXT *context, CH376_FILE file)
{
    struct Library *DOSBase = context->DOSBase;
    Close(file);
}

static CH376_S32 system_file_seek(CH376_CONTEXT *context, CH376_FILE file, int pos)
{
    struct Library *DOSBase = context->DOSBase;

    dbg_printf("system_file_seek trying to seek to position %d\n", pos);

    if(file)
        return Seek(file, pos, OFFSET_BEGINNING);
    else
        return -1;
}

static CH376_S32 system_file_read(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    struct Library *DOSBase = context->DOSBase;

    dbg_printf("system_file_read trying to read %d bytes\n", size);

    if(file)
        return Read(file, buffer, size);
    else
        return -1;
}

static CH376_S32 system_file_write(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    struct Library *DOSBase = context->DOSBase;

    dbg_printf("system_file_write trying to write %d bytes\n", size);

    if(file)
        return Write(file, buffer, size);
    else
        return -1;
}

static CH376_BOOL system_file_delete(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    struct Library *DOSBase = context->DOSBase;
    BPTR old_lock = (BPTR)0;
    LONG err;

    if(root_lock)
    {
        old_lock = CurrentDir(root_lock);
    }

    err = DeleteFile(file_name);

    if(old_lock)
    {
        CurrentDir(old_lock);
    }

    return (err != 0);
}

static CH376_BOOL system_directory_delete(CH376_CONTEXT *context, CH376_LOCK root_lock)
{
    // FIXME
    struct Library *DOSBase = context->DOSBase;

    dbg_printf("system_directory_delete: %s\n", root_lock);

    if(root_lock)
        return (DeleteFile(root_lock) != 0);

    return CH376_FALSE;
}

static CH376_DIR system_start_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    struct Library *DOSBase = context->DOSBase;
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);

    if(fib)
    {
        if(Examine(dir_lock, fib) == DOSFALSE)
        {
            FreeDosObject(DOS_FIB, fib);
            fib = NULL;
        }
    }

    return fib;
}

static CH376_BOOL system_go_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_DIR fib, struct FatDirInfo *dir_info, char *pattern)
{
    struct Library *DOSBase = context->DOSBase;
    BOOL is_done = FALSE;

    if(fib) while(!is_done && ExNext(dir_lock, fib) == DOSTRUE)
    {
        if(normalize_file_name(fib->fib_FileName, dir_info->DIR_Name) && pattern_match(pattern, dir_info->DIR_Name))
        {
            dir_info->DIR_Attr = 0;

            if(fib->fib_DirEntryType > 0)
                dir_info->DIR_Attr |= DIR_ATTR_DIRECTORY;
            if((fib->fib_Protection & FIBF_WRITE) != 0)
                dir_info->DIR_Attr |= DIR_ATTR_READ_ONLY;
            if((fib->fib_Protection & FIBF_ARCHIVE) == 0)
                dir_info->DIR_Attr |= DIR_ATTR_ARCHIVE;
              //if(fib->fib_Protection & FIBF_DELETE)
              //if(fib->fib_Protection & FIBF_EXECUTE)
              //if(fib->fib_Protection & FIBF_READ)
              //if(fib->fib_Protection & FIBF_PURE)
              //if(fib->fib_Protection & FIBF_SCRIPT)
            dbg_printf("system_go_examine_directory defined file attributes: %d\n", dir_info->DIR_Attr);

            dir_info->DIR_NTRes = 0;
            dir_info->DIR_CrtTimeTenth = 0;
            dir_info->DIR_CrtTime[0] = 0;
            dir_info->DIR_CrtTime[1] = 0;
            dir_info->DIR_CrtDate[0] = 0;
            dir_info->DIR_CrtDate[1] = 0;
            dir_info->DIR_LstAccDate[0] = 0;
            dir_info->DIR_LstAccDate[1] = 0;
            dir_info->DIR_FstClusHI[0] = 0;
            dir_info->DIR_FstClusHI[1] = 0;
            dir_info->DIR_WrtTime[0] = 0;
            dir_info->DIR_WrtTime[1] = 0;
            dir_info->DIR_WrtDate[0] = 0;
            dir_info->DIR_WrtDate[1] = 0;
            dir_info->DIR_FstClusLO[0] = 0;
            dir_info->DIR_FstClusLO[1] = 0;
            dir_info->DIR_FileSize[0] = (fib->fib_Size & 0x000000ff) >>  0;
            dir_info->DIR_FileSize[1] = (fib->fib_Size & 0x0000ff00) >>  8;
            dir_info->DIR_FileSize[2] = (fib->fib_Size & 0x00ff0000) >> 16;
            dir_info->DIR_FileSize[3] = (fib->fib_Size & 0xff000000) >> 24;

            is_done = TRUE;
        }
    }

    return is_done;
}

static void system_finish_examine_directory(CH376_CONTEXT *context, CH376_DIR fib)
{
    struct Library *DOSBase = context->DOSBase;

    if(fib)
    {
        FreeDosObject(DOS_FIB, fib);
    }
}

static CH376_S32 system_get_file_size(CH376_CONTEXT *context, CH376_FILE file)
{
    struct Library *DOSBase = context->DOSBase;
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    CH376_S32 file_size = 0xffffffff;

    if (fib)
    {
        if (file)
        {
            if ( ExamineFH(file, &fib) )
            {
                file_size = fib->fib_Size;
            }
            else
            {
               dbg_printf("system_get_file_size: error\n");
            }
        }
        else
        {
            dbg_printf("system_get_file_size: no file handle\n");
        }

        FreeDosObject(DOS_FIB, fib);
    }

    return file_size;
}

static CH376_S32 system_get_file_offset(CH376_CONTEXT *context, CH376_FILE file)
{
    CH376_S32 file_offset = 0xffffffff;

    if (file)
    {
       dbg_printf("system_get_file_offset: unsupported error\n");
    }
    else
        dbg_printf("system_get_file_offset: no file handle\n");

    return file_offset;
}

/* /// */

#elif defined(WIN32)

/* /// "Windows system functions" */

#ifdef DEBUG_CH376
static void dbg_printf(LPCTSTR str, ...)
{
	TCHAR buffer[256];

	va_list args;
	va_start(args, str);
	_vsnprintf_s(buffer, sizeof(buffer) - 1, sizeof(buffer) - 1, str, args);
	OutputDebugString(buffer);
	va_end(args);
}
#else
#define dbg_printf(...)
#endif

static void * system_alloc_mem(int size)
{
    return GlobalAlloc(GMEM_FIXED, size);
}

static void system_free_mem(void *ptr)
{
    GlobalFree(ptr);
}

static CH376_BOOL system_init_context(CH376_CONTEXT *context, UNUSED void *user_data)
{
    // Nothing to do?
    return CH376_TRUE;
};

static void system_clean_context(CH376_CONTEXT *context)
{
    // Nothing to do?
}

static CH376_BOOL system_is_root_dir(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_LOCK root_dir)
{
    if ((dir_lock->path != (CH376_LOCK) 0) && (root_dir->path  != (CH376_LOCK) 0))
        return strncmp(dir_lock->path, root_dir->path, MAX_PATH) == 0;

    return CH376_FALSE;
}

static CH376_BOOL system_get_disk_info(CH376_CONTEXT *context, CH376_LOCK root_lock, struct DiskQuery *disk_info)
{
    BOOL got_info = FALSE;
    char volume[4];
    DWORD sectors_per_cluster;
    DWORD bytes_per_sector;
    DWORD number_of_free_clusters;
    DWORD total_number_of_clusters;

    //@iss volume[0] = (char)(PathGetDriveNumber(root_lock->path) + 'A');
    volume[0] = ((char*)root_lock->path)[0] & 0xdf;
    volume[1] = ':';
    volume[2] = '\\';
    volume[3] = '\0';

    if(GetDiskFreeSpace(volume, &sectors_per_cluster, &bytes_per_sector, &number_of_free_clusters, &total_number_of_clusters))
    {
        INT64 total_sector = (total_number_of_clusters * sectors_per_cluster * bytes_per_sector) / 512;
        INT64 free_sector = (number_of_free_clusters * sectors_per_cluster * bytes_per_sector) / 512;

        disk_info->DISK_TotalSector[0] = (INT8)((total_sector & 0x000000ff) >>  0);
        disk_info->DISK_TotalSector[1] = (INT8)((total_sector & 0x0000ff00) >>  8);
        disk_info->DISK_TotalSector[2] = (INT8)((total_sector & 0x00ff0000) >> 16);
        disk_info->DISK_TotalSector[3] = (INT8)((total_sector & 0xff000000) >> 24);

        disk_info->DISK_FreeSector[0] = (INT8)((free_sector & 0x000000ff) >>  0);
        disk_info->DISK_FreeSector[1] = (INT8)((free_sector & 0x0000ff00) >>  8);
        disk_info->DISK_FreeSector[2] = (INT8)((free_sector & 0x00ff0000) >> 16);
        disk_info->DISK_FreeSector[3] = (INT8)((free_sector & 0xff000000) >> 24);

        disk_info->DISK_DiskFat = 0x0c; // FAT32?

        got_info = TRUE;
    }
    return got_info;
}

static CH376_LOCK system_obtain_directory_lock(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock)
{
    CH376_LOCK lock = NULL;
    TCHAR old_dir[MAX_PATH];
    struct stat s;

    if(root_lock)
    {
        GetCurrentDirectory(sizeof(old_dir), old_dir);
        SetCurrentDirectory(root_lock->path);
    };

    //@iss PathIsDirectory(dir_path)
    if(stat(dir_path,&s) == 0 && s.st_mode & S_IFDIR)
    {
        lock = (CH376_LOCK)system_alloc_mem(sizeof(struct _CH376_LOCK ));
        GetFullPathName(dir_path, MAX_PATH, lock->path, NULL);
        lock->handle = INVALID_HANDLE_VALUE;
    }

    if(root_lock)
    {
        SetCurrentDirectory(old_dir);
    };

    return lock;
}

static void system_release_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    if(dir_lock != (CH376_LOCK)0)
    {
        system_free_mem(dir_lock);
    }
}

static CH376_LOCK system_clone_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    CH376_LOCK dup_lock = (CH376_LOCK)system_alloc_mem(sizeof(struct _CH376_LOCK));

    CopyMemory(dup_lock, dir_lock, sizeof(struct _CH376_LOCK));

    return dup_lock;
}

static HANDLE file_open(CH376_CONTEXT *context, const char *file_name, PCHAR root_path, DWORD creation_disposition)
{
    TCHAR old_dir[MAX_PATH];
    HANDLE file;

    if(root_path)
    {
        GetCurrentDirectory(sizeof(old_dir), old_dir);
        SetCurrentDirectory(root_path);
    }

    file = CreateFile(file_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, creation_disposition, FILE_ATTRIBUTE_NORMAL, NULL);

    if(root_path)
    {
        SetCurrentDirectory(old_dir);
    }

    if(file != INVALID_HANDLE_VALUE)
        return file;
    else
        return NULL;
}

static CH376_BOOL system_file_delete(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
	LONG err;
	TCHAR old_dir[MAX_PATH];

	if (root_lock)
	{
		GetCurrentDirectory(sizeof(old_dir), old_dir);
		SetCurrentDirectory(root_lock->path);
	};

	dbg_printf("system_file_delete: %s\n", file_name);

	err = DeleteFile(file_name);

	if (root_lock)
	{
		SetCurrentDirectory(old_dir);
	};


	return (err != 0);
}

static CH376_BOOL system_directory_delete(CH376_CONTEXT *context, CH376_LOCK root_lock)
{
    dbg_printf("system_directory_delete: %s\n", root_lock);

    if (root_lock)
        return (RemoveDirectory((LPCSTR)root_lock) != 0);

    return CH376_FALSE;
}

static CH376_LOCK system_create_directory(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock)
{
    TCHAR old_dir[MAX_PATH];
    CH376_LOCK lock = NULL;

    if (root_lock)
    {
        GetCurrentDirectory(sizeof(old_dir), old_dir);
        SetCurrentDirectory(root_lock->path);
    };

    dbg_printf("system_create_directory: %s\n", dir_path);

    if (CreateDirectory(dir_path, NULL))
    {
        lock = (CH376_LOCK)system_alloc_mem(sizeof(struct _CH376_LOCK ));
        SetCurrentDirectory(dir_path);
        GetCurrentDirectory(sizeof(lock->path), lock->path);
        lock->handle = INVALID_HANDLE_VALUE;
    }

    if (root_lock)
    {
        SetCurrentDirectory(old_dir);
    };

    return lock;

}



static CH376_FILE system_file_open_existing(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(context, file_name, root_lock->path, OPEN_EXISTING);
}

static CH376_FILE system_file_open_new(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(context, file_name, root_lock->path, CREATE_ALWAYS);
}

static void system_file_close(CH376_CONTEXT *context, CH376_FILE file)
{
    if(file) CloseHandle(file);
}

static CH376_S32 system_file_seek(CH376_CONTEXT *context, CH376_FILE file, int pos)
{
    if(file != NULL)
        return SetFilePointer(file, pos, NULL, FILE_BEGIN);
    else
        return -1;
}

static CH376_S32 system_file_read(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    DWORD read_bytes;

    if(ReadFile(file, buffer, size, &read_bytes, NULL))
        return read_bytes;
    else
        return -1;
}

static CH376_S32 system_file_write(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    DWORD written_bytes;

    if(WriteFile(file, buffer, size, &written_bytes, NULL))
        return written_bytes;
    else
        return -1;
}

static CH376_DIR system_start_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    LPWIN32_FIND_DATA find_file_data = (LPWIN32_FIND_DATA)system_alloc_mem(sizeof(WIN32_FIND_DATA));
    TCHAR szDir[MAX_PATH];
    StringCchCopy(szDir, MAX_PATH, dir_lock->path);
    StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

    dir_lock->handle = FindFirstFile(szDir, find_file_data);

    if(dir_lock->handle == INVALID_HANDLE_VALUE)
    {
        system_free_mem(find_file_data);
        find_file_data = NULL;
    }

    return find_file_data;
}

static CH376_BOOL system_go_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_DIR fib, struct FatDirInfo *dir_info, char *pattern)
{
    BOOL is_done = FALSE;

    if(dir_lock->handle != INVALID_HANDLE_VALUE)
    {
        while(!is_done && FindNextFile(dir_lock->handle, fib))
        {
            if(normalize_file_name(fib->cFileName, dir_info->DIR_Name) && pattern_match(pattern, dir_info->DIR_Name))
            {
                // Storing attributes :)
                dir_info->DIR_Attr = (UINT8)fib->dwFileAttributes;

                dbg_printf("system_go_examine_directory defined file attributes: %d\n", dir_info->DIR_Attr);

                dir_info->DIR_NTRes = 0;
                dir_info->DIR_CrtTimeTenth = 0;
                dir_info->DIR_CrtTime[0] = 0;
                dir_info->DIR_CrtTime[1] = 0;
                dir_info->DIR_CrtDate[0] = 0;
                dir_info->DIR_CrtDate[1] = 0;
                dir_info->DIR_LstAccDate[0] = 0;
                dir_info->DIR_LstAccDate[1] = 0;
                dir_info->DIR_FstClusHI[0] = 0;
                dir_info->DIR_FstClusHI[1] = 0;
                dir_info->DIR_WrtTime[0] = 0;
                dir_info->DIR_WrtTime[1] = 0;
                dir_info->DIR_WrtDate[0] = 0;
                dir_info->DIR_WrtDate[1] = 0;
                dir_info->DIR_FstClusLO[0] = 0;
                dir_info->DIR_FstClusLO[1] = 0;
                dir_info->DIR_FileSize[0] = (UINT8)((fib->nFileSizeLow & 0x000000ff) >>  0);
                dir_info->DIR_FileSize[1] = (UINT8)((fib->nFileSizeLow & 0x0000ff00) >>  8);
                dir_info->DIR_FileSize[2] = (UINT8)((fib->nFileSizeLow & 0x00ff0000) >> 16);
                dir_info->DIR_FileSize[3] = (UINT8)((fib->nFileSizeLow & 0xff000000) >> 24);

                is_done = TRUE;
            }
        }
    }

    return is_done;
}

static void system_finish_examine_directory(CH376_CONTEXT *context, CH376_DIR fib)
{
    if(fib)
    {
        FindClose(fib);
        system_free_mem(fib);
    }
}

static CH376_S32 system_get_file_size(CH376_CONTEXT *context, CH376_FILE file)
{
    CH376_S32 file_size = 0xffffffff;
    BY_HANDLE_FILE_INFORMATION file_stat;

    if (file)
    {
        if ( GetFileInformationByHandle(file, &file_stat) )
        {
            // Only low DWORD
            file_size = file_stat.nFileSizeLow;
        }
        else
        {
           dbg_printf("system_get_file_size: error\n");
        }
    }
    else
        dbg_printf("system_get_file_size: no file handle\n");

    return file_size;
}

static CH376_S32 system_get_file_offset(CH376_CONTEXT *context, CH376_FILE file)
{
    CH376_S32 file_offset = 0xffffffff;

    if (file)
    {
       dbg_printf("system_get_file_offset: unsupported error\n");
    }
    else
        dbg_printf("system_get_file_offset: no file handle\n");

    return file_offset;
}

/* /// */

#elif defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)

/* /// "POSIX system functions" */

#ifdef DEBUG_CH376
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

/* static */ void * system_alloc_mem(int size)
{
    return malloc(size);
}

/* static */ void system_free_mem(void *ptr)
{
    if (ptr)
        free(ptr);
}

static CH376_BOOL system_init_context(CH376_CONTEXT *context, UNUSED void *user_data)
{
  /* Nothing to do */
    context = context;
    return CH376_TRUE;
}

static void system_clean_context(CH376_CONTEXT *context)
{
    context = context;
  /* Nothing to do */
}

static CH376_BOOL system_is_root_dir(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_LOCK root_dir)
{
    context = context;

    if ((dir_lock != (CH376_LOCK) 0) && (root_dir  != (CH376_LOCK) 0))
        return strncmp(dir_lock, root_dir, PATH_MAX) == 0;

    return CH376_FALSE;
}

static CH376_BOOL system_get_disk_info(CH376_CONTEXT *context, CH376_LOCK root_lock, struct DiskQuery *disk_info)
{
    CH376_BOOL got_info = CH376_FALSE;

    context = context;

    if(disk_info)
    {
        struct statvfs stats;

        if(statvfs(root_lock, &stats) == 0)
        {
            int64_t total_sector = (stats.f_blocks * stats.f_bsize) / 512;
            // int64_t free_sector = (stats.f_bfree * stats.f_bsize) / 512;
            int64_t free_sector = (stats.f_bavail * stats.f_bsize) / 512;
            dbg_printf("\n*** f_frsize=%ld, f_bsize=%ld\n", stats.f_frsize, stats.f_bsize);
            dbg_printf(  "*** f_blocks=%ld, f_bsize=%ld\n", stats.f_blocks, stats.f_bsize);
            dbg_printf(  "*** f_bfree =%ld, f_bsize=%ld\n", stats.f_bfree , stats.f_bsize);
            dbg_printf(  "*** f_bavail=%ld, f_bsize=%ld\n", stats.f_bavail, stats.f_bsize);

            disk_info->DISK_TotalSector[0] = (total_sector & 0x000000ff) >>  0;
            disk_info->DISK_TotalSector[1] = (total_sector & 0x0000ff00) >>  8;
            disk_info->DISK_TotalSector[2] = (total_sector & 0x00ff0000) >> 16;
            disk_info->DISK_TotalSector[3] = (total_sector & 0xff000000) >> 24;

            disk_info->DISK_FreeSector[0] = (free_sector & 0x000000ff) >>  0;
            disk_info->DISK_FreeSector[1] = (free_sector & 0x0000ff00) >>  8;
            disk_info->DISK_FreeSector[2] = (free_sector & 0x00ff0000) >> 16;
            disk_info->DISK_FreeSector[3] = (free_sector & 0xff000000) >> 24;

            disk_info->DISK_DiskFat = 0x0c; // FAT32?

            got_info = CH376_TRUE;
        }
    }

    return got_info;
}

static CH376_LOCK system_obtain_directory_lock(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock)
{
    CH376_LOCK lock = NULL;
    char *old_dir = NULL;
    struct stat path_stat;

    context = context;

    dbg_printf("system_obtain_directory_lock: %s\n", dir_path);

    if(root_lock)
    {
        if((old_dir = getcwd(NULL, 0)) != NULL)
            chdir(root_lock);
    }

    stat(dir_path, &path_stat);
    if(S_ISDIR(path_stat.st_mode))
    {
      lock = realpath(dir_path, NULL);
    }

    if(old_dir)
    {
        if(old_dir != NULL)
        {
            chdir(old_dir);
            system_free_mem(old_dir);
        }
    }

    return lock;
}

static void system_release_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{

    context = context;

    if(dir_lock != (CH376_LOCK)0)
    {
        system_free_mem(dir_lock);
    }
}

static CH376_LOCK system_clone_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    context = context;

    return strdup(dir_lock);
}

static FILE * file_open(const char *file_name,  CH376_LOCK root_lock, const char *mode)
{
    FILE *file;
    char *old_dir = NULL;
    // FIXME: unused variable
    //struct stat path_stat;

    if(root_lock)
    {
        if((old_dir = getcwd(NULL, 0)) != NULL)
            chdir(root_lock);
    }

    file = fopen(file_name, mode);
dbg_printf("=== file_open(%s)\n", file_name);
    if(old_dir)
    {
        chdir(old_dir);
        system_free_mem(old_dir);
    }

    return file;
}

static CH376_BOOL system_file_delete(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    int err;
    char *old_dir = NULL;

    context = context;

    if(root_lock)
    {
        if((old_dir = getcwd(NULL, 0)) != NULL)
            chdir(root_lock);
    }

    err = remove(file_name);

    dbg_printf("system_file_delete: %s\n", file_name);

    if(old_dir)
    {
        if(old_dir != NULL)
        {
            chdir(old_dir);
            system_free_mem(old_dir);
        }
    }

    return (err == 0);
}

static CH376_BOOL system_directory_delete(CH376_CONTEXT *context, CH376_LOCK root_lock)
{
    context = context;

    dbg_printf("system_directory_delete: %s\n", root_lock);

    if (root_lock)
        return (rmdir(root_lock) == 0);
    else
        return CH376_FALSE;
}

static CH376_LOCK system_create_directory(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock)
{
    CH376_LOCK lock = NULL;
    char *old_dir = NULL;

    context = context;

    if(root_lock)
    {
        if((old_dir = getcwd(NULL, 0)) != NULL)
            chdir(root_lock);
    }

    dbg_printf("system_create_directory: %s\n", dir_path);

    if (!mkdir(dir_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
        lock = realpath(dir_path, NULL);

    if(old_dir)
    {
        chdir(old_dir);
        system_free_mem(old_dir);
    }

    return lock;
}

static CH376_FILE system_file_open_existing(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    FILE *fp;

    context = context;

    fp = file_open(file_name, root_lock, "rb+");

    if (fp == NULL)
    {
        // Maybe read only file, try reopen
        fp = file_open(file_name, root_lock, "rb");
    }

    return fp;
}

static CH376_FILE system_file_open_new(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    context = context;

    return file_open(file_name, root_lock, "wb+");
}

static void system_file_close(CH376_CONTEXT *context, CH376_FILE file)
{
    context = context;

    if (file) fclose(file);
}

static CH376_S32 system_file_seek(CH376_CONTEXT *context, CH376_FILE file, int pos)
{
    context = context;

    dbg_printf("system_file_seek trying to seek to position %d\n", pos);

    if(file)
        if (!fseek(file, pos, SEEK_SET))
            return ftell(file);
//    else
        return -1;
}

static CH376_S32 system_file_read(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    context = context;

    dbg_printf("system_file_read trying to read %d bytes (%d, %x)\n", size, file, buffer);

    if(file)
        return fread(buffer, 1, size, file);
    else
        return -1;
}

static CH376_S32 system_file_write(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{

    context = context;

    dbg_printf("system_file_write trying to write %d bytes\n", size);

    if(file)
        return fwrite(buffer, 1, size, file);
    else
        return -1;
}

static CH376_DIR system_start_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    CH376_DIR fib = system_alloc_mem(sizeof(struct _CH376_DIR));

    context = context;

    if(fib)
    {
        fib->handle = opendir(dir_lock);
        fib->entry = NULL;
    }

    return fib;
}
#include <time.h>

static CH376_BOOL system_go_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_DIR fib, struct FatDirInfo *dir_info, char *pattern)
{
    CH376_BOOL is_done = CH376_FALSE;
    struct stat file_stat;
    struct tm *timeinfo;
    CH376_U16 dos_cdate, dos_ctime;
    CH376_U16 dos_adate;
    CH376_U16 dos_mdate, dos_mtime;
    char *old_dir = NULL;

    context = context;

    if(dir_lock)
    {
        if((old_dir = getcwd(NULL, 0)) != NULL)
            chdir(dir_lock);
    }

    if(fib) while(!is_done && (fib->entry = readdir(fib->handle)))
    {
        if(normalize_file_name(fib->entry->d_name, dir_info->DIR_Name) && pattern_match(pattern, dir_info->DIR_Name))
        {
            stat(fib->entry->d_name, &file_stat);

            dir_info->DIR_Attr = 0;

            if(S_ISDIR(file_stat.st_mode))
                dir_info->DIR_Attr |= DIR_ATTR_DIRECTORY;
            if((file_stat.st_mode & S_IWUSR) == 0) {
                dir_info->DIR_Attr |= DIR_ATTR_READ_ONLY;
            dbg_printf("--- READ ONLY ---");
            }

            dbg_printf("system_go_examine_directory defined file attributes: %04o -> %02x\n", file_stat.st_mode, dir_info->DIR_Attr);
            if (file_stat.st_ctim.tv_sec == 0)
            {
                dos_cdate = 0;
                dos_ctime = 0;
            }
            else
            {
                timeinfo = localtime(&(file_stat.st_ctim.tv_sec));
                dos_cdate = DIR_MAKE_FILE_DATE((timeinfo->tm_year + 1900), (timeinfo->tm_mon + 1), timeinfo->tm_mday);
                dos_ctime = DIR_MAKE_FILE_TIME(timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
                dbg_printf("system_go_examine_directory defined file cdate/time: %s", asctime(timeinfo));
            }

            if (file_stat.st_atim.tv_sec == 0)
            {
                dos_adate = 0;
            }
            else
            {
                timeinfo = localtime(&(file_stat.st_atim.tv_sec));
                dos_adate = DIR_MAKE_FILE_DATE((timeinfo->tm_year + 1900), (timeinfo->tm_mon + 1), timeinfo->tm_mday);
                dbg_printf("system_go_examine_directory defined file adate/time: %s", asctime(timeinfo));
            }

            if (file_stat.st_mtim.tv_sec == 0)
            {
                dos_mdate = 0;
                dos_mtime = 0;
            }
            else
            {
                timeinfo = localtime(&(file_stat.st_mtim.tv_sec));
                dos_mdate = DIR_MAKE_FILE_DATE((timeinfo->tm_year + 1900), (timeinfo->tm_mon + 1), timeinfo->tm_mday);
                dos_mtime = DIR_MAKE_FILE_TIME(timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
                dbg_printf("system_go_examine_directory defined file mdate/time: %s", asctime(timeinfo));
            }

            dir_info->DIR_NTRes = 0;
            dir_info->DIR_CrtTimeTenth = 0;
            dir_info->DIR_CrtTime[0] = (dos_ctime & 0x00ff);
            dir_info->DIR_CrtTime[1] = (dos_ctime >> 8);
            dir_info->DIR_CrtDate[0] = (dos_cdate & 0x00ff);
            dir_info->DIR_CrtDate[1] = (dos_cdate >> 8);
            dir_info->DIR_LstAccDate[0] = (dos_adate & 0x00ff);
            dir_info->DIR_LstAccDate[1] = (dos_adate >> 8);
            dir_info->DIR_FstClusHI[0] = 0;
            dir_info->DIR_FstClusHI[1] = 0;
            dir_info->DIR_WrtTime[0] = (dos_mtime & 0x00ff);
            dir_info->DIR_WrtTime[1] = (dos_mtime >> 8);
            dir_info->DIR_WrtDate[0] = (dos_mdate & 0x00ff);
            dir_info->DIR_WrtDate[1] = (dos_mdate >> 8);
            dir_info->DIR_FstClusLO[0] = 0;
            dir_info->DIR_FstClusLO[1] = 0;
            dir_info->DIR_FileSize[0] = (file_stat.st_size & 0x000000ff) >>  0;
            dir_info->DIR_FileSize[1] = (file_stat.st_size & 0x0000ff00) >>  8;
            dir_info->DIR_FileSize[2] = (file_stat.st_size & 0x00ff0000) >> 16;
            dir_info->DIR_FileSize[3] = (file_stat.st_size & 0xff000000) >> 24;

            is_done = CH376_TRUE;
        }
    }

    if(old_dir)
    {
            chdir(old_dir);
            system_free_mem(old_dir);
    }

    return is_done;
}

static void system_finish_examine_directory(CH376_CONTEXT *context, CH376_DIR fib)
{
    context = context;

    if (fib)
    {
        closedir(fib->handle);
        system_free_mem(fib);
    }
}

static CH376_S32 system_get_file_size(CH376_CONTEXT *context, CH376_FILE file)
{
    CH376_S32 file_size = 0xffffffff;
    struct stat file_stat;

    context = context;

    if (file)
    {
        if ( !fstat(fileno(file), &file_stat) )
        {
            file_size = file_stat.st_size;
        }
        else
        {
           dbg_printf("system_get_file_size: error\n");
        }
    }
    else
        dbg_printf("system_get_file_size: no file handle\n");

    return file_size;
}

static CH376_S32 system_get_file_offset(CH376_CONTEXT *context, CH376_FILE file)
{
    CH376_S32 file_offset = 0xffffffff;

    context = context;

    if (file)
    {
        file_offset = ftell(file);

	if(file_offset == -1)
        {
           dbg_printf("system_get_file_offset: error\n");
        }
    }
    else
        dbg_printf("system_get_file_offset: no file handle\n");

    return file_offset;
}

/* /// */

#else
#error "FixMe!"
#endif

/* /// "CH376 private subroutines" */

static int check_fat32_char(char c)
{
    // Check FAT32 allowed characters
    // Return -1 if not allowed, or the character if allowed
    switch(c)
    {
        case '.':
        //case ' ':
        case '!':
        case '#':
        case '$':
        case '%':
        case '&':
        //case '\'':
        case '(':
        case ')':
        case '{':
        case '}':
        case '-':
        case '@':
        //case '^':
        case '_':
        //case '`':
        case '~':
        // case '+':
        case '0': case '1': case '2':
        case '3': case '4': case '5':
        case '6': case '7': case '8':
        case '9':
        case 0: // allow EOS in the char
        case '/': // Allow / for root folder
            return c;

        // Allowed here to also allow pattern matching
        case '*':
            return c;

        // Allow token
        case '?':
            return c;

        default:
            if((c >='A' && c <='Z') || (c >= 128 && c <= 228) || c >= 230)
                return c;

            return -1;
    }
}


static char *clone_string(const char *string)
{
    int l;
    char *s;


    for(l=0; string[l]!='\0'; l++);

    s = system_alloc_mem(l+1);

    for(l=0; string[l]!='\0'; l++)
        s[l] = string[l];

    s[l] = '\0';

    return s;
}

static void clear_structure(struct ch376 *ch376)
{
    ch376->command = CH376_CMD_NONE;
    ch376->command_status = 0;
    ch376->interface_status = 0;
    ch376->nb_bytes_in_cmd_data = 0;
    ch376->pos_rw_in_cmd_data = 0;
    ch376->usb_mode = CH376_ARG_SET_USB_MODE_INVALID;
    ch376->bytes_to_read_write = 0;
    ch376->root_dir_lock = (CH376_LOCK)0;
    ch376->current_dir_lock = (CH376_LOCK)0;
    ch376->current_file = (CH376_FILE)0;
    ch376->current_directory_browsing = (CH376_DIR)0;
    ch376->current_pos = (CH376_S32)0;
}

static void cancel_all_io(struct ch376 *ch376)
{
    // Cancel data buffer read/write sessions
    ch376->bytes_to_read_write = 0;

    // End directory browing or file i/o sessions
    system_finish_examine_directory(&ch376->context, ch376->current_directory_browsing);
    ch376->current_directory_browsing = (CH376_DIR)0;
    system_file_close(&ch376->context, ch376->current_file);
    ch376->current_file = (CH376_FILE)0;

    // Release potential already obtained lock for root and current
    system_release_directory_lock(&ch376->context, ch376->root_dir_lock);
    ch376->root_dir_lock = (CH376_LOCK)0;
    system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
    ch376->current_dir_lock = (CH376_LOCK)0;
}

// Normalize a char (capitalized and ASCII)
// Return -1 if not possible
static int normalize_char(char c)
{
    if(c > 127)
        return -1;

    if(c >='a' && c <='z')
        return c - ('a' - 'A');

    return c;
}

// Normalized a dos filename in 8.3 format
// Return NULL if not possible or if filename is '.' or '..'
static const char * normalize_file_name(const char *file_name, char *normalized_file_name)
{
    int i = 0;
    int j = 0;
    int c;

    dbg_printf("normalize_file_name: '%s'\n", file_name);

    // '.' or '..'
    if(file_name[0] == '.')
    {
        if( (file_name[1] == '\0') || (file_name[1] == '.' && file_name[2] == '\0'))
            return NULL;
    }

    while(file_name[i] != '\0' && file_name[i] != '.' && j < 8)
    {
        c = normalize_char(file_name[i++]);

        if(c == -1)
            return NULL;
        else
            normalized_file_name[j++] = (char)c;
    }

    if(file_name[i] == '\0')
    {
        for(; j<8; j++)
        {
            normalized_file_name[j] = ' ';
        }
    }
    else if(file_name[i] == '.')
    {
        i++;
        for(; j<8; j++)
        {
            normalized_file_name[j] = ' ';
        }
    }
    else
    {
        dbg_printf("normalize_file_name impossible: '%s' (%d=%c,%d=%c)\n", file_name, i, file_name[i], j, normalized_file_name[j]);
        return NULL;
    }

    while(file_name[i] != '\0' && file_name[i] != '.' && j < 11)
    {
        c = normalize_char(file_name[i++]);

        if(c == -1)
            return NULL;
        else
            normalized_file_name[j++] = (char)c;
    }

    if(file_name[i] != '\0')
    {
        dbg_printf("normalize_file_name impossible: '%s' (%d=%c,%d=%c)\n", file_name, i, file_name[i], j, normalized_file_name[j]);
        return NULL;
    }

    dbg_printf("normalize_file_name done: '%s' (%d=&%02x,%d=&%02x)\n", file_name, i, file_name[i], j, normalized_file_name[j]);

    for(; j<11; j++)
    {
        normalized_file_name[j] = ' ';
    }

    normalized_file_name[j] = '\0';

    return normalized_file_name;
}

// Convert normalized 8.3 filename into regular filename
// (remove trailing spaces in name and extension, and clear extension trailing '.' if any)
static const char * trim_file_name(const char *file_name, char *trimmed_file_name)
{
    int i = 0;
    int j = 0;

    while(file_name[i] != ' ' && file_name[i] != '\0')
        trimmed_file_name[j++] = file_name[i++];

    while(file_name[i] == ' ')
        i++;

    while(file_name[i] != ' ' && file_name[i] != '\0')
        trimmed_file_name[j++] = file_name[i++];

    if(j > 0 && trimmed_file_name[j-1] == '.')
        j--;

    trimmed_file_name[j] = '\0';

    dbg_printf("trim_file_name from \"%s\" to \"%s\"\n", file_name, trimmed_file_name);

    return trimmed_file_name;
}

static void file_read_chunk(struct ch376 *ch376)
{
    CH376_S32 bytes_to_read_now;
    CH376_S32 bytes_actually_read;

    if(ch376->bytes_to_read_write > sizeof(ch376->cmd_data.CMD_IOBuffer))
        bytes_to_read_now = sizeof(ch376->cmd_data.CMD_IOBuffer);
    else
        bytes_to_read_now = ch376->bytes_to_read_write;

    ch376->buffer_read_count = (ch376->buffer_read_count + 1) % 0x03;
    if (!ch376->buffer_read_count && bytes_to_read_now > 2)
        bytes_to_read_now = 2;

dbg_printf("\n*** read count/ %d\n", ch376->buffer_read_count);
    bytes_actually_read = system_file_read(&ch376->context, ch376->current_file, &ch376->cmd_data.CMD_IOBuffer, bytes_to_read_now);
//dbg_printf("*** bytes_to_read_now=%d, bytes actually_read=%d, feof=%d, ferror=%d\n",bytes_to_read_now, bytes_actually_read,feof(ch376->current_file),ferror(ch376->current_file));
    if(bytes_actually_read >= 0)
    {
        ch376->bytes_to_read_write -= (CH376_U16)bytes_actually_read;

        ch376->nb_bytes_in_cmd_data = (CH376_U8)bytes_actually_read;
        ch376->interface_status = 127;
        // CH376 specification tells that the last buffer is returned with INT_SUCCESS
        // but it is not the case (at least with revision 4). An empty buffer is always
        // returned with INT_SUCCESS. As a consequence some routines do not read the last
        // buffer when INT_SUCCESS is returned. It is not correct regarding CH376
        // specification, but to keep compatibility with these (wrong) routines,
        // we also always return an empty buffer with INT_SUCCESS by checking
        // bytes_actually_read == 0 instead of bytes_actually_read < bytes_to_read_now
        if(bytes_to_read_now == 0 || bytes_actually_read == 0)
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_READ] all done: read operated with success (no more data)\n");
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_READ] all done: read operated with success (waiting data read for %d bytes out of %d)\n", ch376->nb_bytes_in_cmd_data, ch376->bytes_to_read_write);
            ch376->command_status = CH376_INT_DISK_READ;
        }
    }
    else
    {
        dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_READ] aborted: read failure\n");

        ch376->nb_bytes_in_cmd_data = 0;
        ch376->interface_status = 0;
        ch376->command_status = CH376_RET_ABORT;
    }
}

static void file_write_chunk(struct ch376 *ch376)
{
    CH376_S32 bytes_to_write_now;
    CH376_S32 bytes_actually_written;

    bytes_to_write_now = ch376->nb_bytes_in_cmd_data;

    bytes_actually_written = system_file_write(&ch376->context, ch376->current_file, &ch376->cmd_data.CMD_IOBuffer, bytes_to_write_now);

    if(bytes_actually_written == bytes_to_write_now)
    {
        ch376->bytes_to_read_write -= (CH376_U16)bytes_actually_written;

        ch376->nb_bytes_in_cmd_data = 0;
        ch376->interface_status = 127;
        if(ch376->bytes_to_read_write == 0)
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_WRITE] all done: write operated with success (no more data)\n");
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            if(ch376->bytes_to_read_write > sizeof(ch376->cmd_data.CMD_IOBuffer))
                ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_IOBuffer);
            else
                ch376->nb_bytes_in_cmd_data = (CH376_U8)ch376->bytes_to_read_write;

            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_WRITE] all done: write operated with success (waiting data write for %d bytes out of %d)\n", ch376->nb_bytes_in_cmd_data, ch376->bytes_to_read_write);
            ch376->command_status = CH376_INT_DISK_WRITE;
        }
    }
    else
    {
        dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_WRITE] aborted: write failure: write %d, written %d\n", bytes_to_write_now, bytes_actually_written);

        ch376->nb_bytes_in_cmd_data = 0;
        ch376->interface_status = 0;
        ch376->command_status = CH376_RET_ABORT;
    }
}

// Normalize file pattern (use normalize_file_name())
static const char * normalize_pattern(const char *pattern, char *normalized_pattern)
{
    int i = -1;

    dbg_printf("normalize_pattern: '%s'\n", normalized_pattern);

    if (!strcmp(pattern, "*"))
    {
        dbg_printf("normalize_pattern: * -> *.*\n");
        normalize_file_name("*.*", normalized_pattern);
    }
    else if (!normalize_file_name(pattern, normalized_pattern))
    {
        dbg_printf("normalize_pattern: error\n");
        normalized_pattern[0] = '\0'; // if used, pattern_match() will always return FALSE
        return NULL;
    }

    while (i<11)
    {
        i++;
        if (normalized_pattern[i] == '*')
        {
            // We can't use '?' as wildcard character because ch376 can't use '?' as meta character
            // Unlike DOS: Here '*' means any character up to end of filename including extension part
            while (i < 11)
                normalized_pattern[i++]='*';
        }
    }

    dbg_printf("normalize_pattern: '%s' (i=%d)\n", normalized_pattern, i);

    return normalized_pattern;
}

// Check file_name against pattern
// file_name and pattern must be normalized before calling the function
static CH376_BOOL pattern_match(const char *pattern, const char * file_name)
{
    CH376_BOOL found = CH376_FALSE;
    int i=0;

    dbg_printf("pattern_match('%s', '%s') => ", pattern, file_name);

    while(1)
    {
        if (pattern[i] != '*')
        {
            if (pattern[i] != file_name[i])
            {
                found = CH376_FALSE;
                break;
            }
            if (pattern[i] == '\0')
            {
                found = CH376_TRUE;
                break;
            }
        }
        else
        {
            if (file_name[i] == '\0')
            {
                found = CH376_TRUE;
                break;
            }
        }
        i++;
    }

    dbg_printf("i=%d, found=%d\n", i, found);
    return found;
}

/* /// */

/* /// "CH376 public read command port" */

CH376_U8 ch376_read_command_port(struct ch376 *ch376)
{
    dbg_printf("[READ][COMMAND] during command &%02x status &%02x\n", ch376->command, ch376->command_status);
    return ch376->interface_status;
}

/* /// */

/* /// "CH376 public read data port" */

CH376_U8 ch376_read_data_port(struct ch376 *ch376)
{
    CH376_U8 data_out = 0xff; // Hi-Z

    dbg_printf(">> [READ][DATA] for during command &%02x status &%02x\n", ch376->command, ch376->command_status);

    switch(ch376->command)
    {
    case CH376_CMD_CHECK_EXIST:
        data_out = ch376->cmd_data.CMD_CheckByte;
        dbg_printf("[READ][DATA][CH376_CMD_CHECK_EXIST] setting data port to &%02x\n", data_out);
        break;

    case CH376_CMD_READ_VAR32:
        if(ch376->nb_bytes_in_cmd_data)
        {
            if(ch376->nb_bytes_in_cmd_data != ch376->pos_rw_in_cmd_data)
            {
                data_out = ch376->cmd_data.CMD_VAR32[ch376->pos_rw_in_cmd_data];

                dbg_printf("[READ][DATA][CH376_CMD_READ_VAR32] read &%02x from i/o buffer at position &%02x\n", data_out, ch376->pos_rw_in_cmd_data);

                if(++ch376->pos_rw_in_cmd_data == ch376->nb_bytes_in_cmd_data)
                    ch376->nb_bytes_in_cmd_data = 0;
            }
        }
        else
        {
            data_out = 0;
            dbg_printf("[READ][DATA][CH376_CMD_READ_VAR32] nothing to read from i/o buffer\n");
        }

        break;

    case CH376_CMD_GET_IC_VER:
        data_out = (0x40 | CH376_DATA_IC_VER) & 0x7f;
        dbg_printf("[READ][DATA][CH376_CMD_GET_IC_VER] setting data port to &%02x\n", data_out);
        break;

    case CH376_CMD_SET_USB_MODE:
        if(ch376->usb_mode == CH376_ARG_SET_USB_MODE_INVALID)
        {
            data_out = CH376_RET_ABORT;
            dbg_printf("[READ][DATA][CH376_SET_USB_MODE] aborted!\n");
        }
        else
        {
            data_out = CH376_RET_SUCCESS;
            dbg_printf("[READ][DATA][CH376_SET_USB_MODE] completed!\n");
        }
        break;

    case CH376_CMD_GET_STATUS:
        data_out = ch376->command_status;
        dbg_printf("[READ][DATA][CH376_CMD_GET_STATUS] setting data port to &%02x\n", data_out);
        break;

    case CH376_CMD_RD_USB_DATA0:
        if (ch376->usb_mode == CH376_ARG_SET_USB_MODE_USB_HOST)
            dbg_printf("[READ][DATA][CH376_CMD_RD_USB_DATA0] Entering into usb host mode : ");

        if (ch376->usb_mode == CH376_ARG_SET_USB_MODE_SD_HOST)
            dbg_printf("[READ][DATA][CH376_CMD_RD_USB_DATA0] Entering into sdcard mode : ");

        if (ch376->device_connected_to_usb_port == USB_MOUSE_CLASS)
            dbg_printf("Mouse connected in usb port\n");

        if (ch376->device_connected_to_usb_port == USB_MASS_STORAGE_CLASS)
            dbg_printf("Mass storage in usb port\n");



        if (ch376->device_connected_to_usb_port != USB_MASS_STORAGE_CLASS && ch376->usb_mode == CH376_ARG_SET_USB_MODE_USB_HOST)
        {
            if (ch376->pos_in_usb_data == 0)
            {
                // Get mouse informations
                int x, y;
                // Bug it does not manage blank part. To manage it, it requires to have x and y of oric display
                SDL_GetRelativeMouseState(&x, &y);

                ch376->hid_mouse_deltax = (signed char)x;
                ch376->hid_mouse_deltay = (signed char)y;

                if (ch376->hid_mouse_deltax == 0 && ch376->hid_mouse_deltay == 0)
                    ch376->usb_data[0] = 0; // No movement
                else
                    ch376->usb_data[0] = 1; // Must be checked under real ch376, which value it will return
                ch376->usb_data[1] = 0; // button
                ch376->usb_data[2] = ch376->hid_mouse_deltax; // X
                ch376->usb_data[3] = ch376->hid_mouse_deltay; // Y
                ch376->usb_data[4] = 0; // wheel
                data_out = ch376->usb_data[0];
                ch376->pos_in_usb_data ++;
                ch376->hid_mouse_posx = x;
                ch376->hid_mouse_posy = y;

            }
            else
            {
                data_out = ch376->usb_data[ch376->pos_in_usb_data];
                dbg_printf("Sending byte %d value : 0x%x\n", ch376->pos_in_usb_data, data_out);
                // When we read registers, we init to 0 x ()
                if (ch376->pos_in_usb_data == 2) ch376->hid_mouse_deltax = 0;
                if (ch376->pos_in_usb_data == 3) ch376->hid_mouse_deltay = 0;
                // When we read registers, we init to 0 x ()
                ch376->usb_data[ch376->pos_in_usb_data] = 0;
                ch376->pos_in_usb_data ++;
            }


        }
        // Mass storage
        else if (ch376->device_connected_to_usb_port == USB_MASS_STORAGE_CLASS || ch376->usb_mode == CH376_ARG_SET_USB_MODE_SD_HOST)
        {

            if(ch376->nb_bytes_in_cmd_data)
            {
                if(ch376->pos_rw_in_cmd_data == CMD_DATA_REQ_SIZE)
                {
                    data_out = ch376->nb_bytes_in_cmd_data;
                    ch376->pos_rw_in_cmd_data = 0;
                    dbg_printf("[READ][DATA][CH376_CMD_RD_USB_DATA0] read i/o buffer size: &%02x\n", data_out);
                }
                else if(ch376->nb_bytes_in_cmd_data != ch376->pos_rw_in_cmd_data)
                {
                    data_out = ch376->cmd_data.CMD_IOBuffer[ch376->pos_rw_in_cmd_data];

                    if (!ch376->current_file_is_directory)
                        ++ch376->current_pos;

                    dbg_printf("[READ][DATA][CH376_CMD_RD_USB_DATA0] read \"%c\" (&%02x) from i/o buffer at position &%02x\n", data_out, data_out, ch376->pos_rw_in_cmd_data);

                    if(++ch376->pos_rw_in_cmd_data == ch376->nb_bytes_in_cmd_data)
                        ch376->nb_bytes_in_cmd_data = 0;
                }
            }
            else
            {
                data_out = 0;
                dbg_printf("[READ][DATA][CH376_CMD_RD_USB_DATA0] nothing to read from i/o buffer\n");
            }
        }
        break;

    case CH376_CMD_WR_REQ_DATA:
        if(ch376->nb_bytes_in_cmd_data)
        {
            if(ch376->pos_rw_in_cmd_data == CMD_DATA_REQ_SIZE)
            {
                data_out = ch376->nb_bytes_in_cmd_data;
                ch376->pos_rw_in_cmd_data = 0;
                dbg_printf("[READ][DATA][CH376_CMD_WR_REQ_DATA] read i/o buffer size: &%02x\n", data_out);
            }
        }
        else
        {
            data_out = 0;
            dbg_printf("[READ][DATA][CH376_CMD_WR_REQ_DATA] nothing to read from i/o buffer\n");
        }
        break;

    // Emulate CH376 bug which returns the 1st byte in data buffer
    // when read is performed on an unexpected command
    default:
        data_out = ch376->cmd_data.CMD_IOBuffer[0];
        break;
    }

    dbg_printf("<< [READ][DATA] for during command &%02x status &%02x\n", ch376->command, ch376->command_status);


  return data_out;
}

/* /// */

/* /// "CH376 public write command port" */

void ch376_write_command_port(struct ch376 *ch376, CH376_U8 command, struct expansion_bus *oric_bus)
{
    dbg_printf(">> [WRITE][COMMAND] Write command &%02x status &%02x\n", command, ch376->command_status);

    ch376->interface_status = 0;

    // Emulate CH376 bug which can get the check byte
    // from the command port instead of the data port!
    if(ch376->command == CH376_CMD_CHECK_EXIST)
    {
        ch376->cmd_data.CMD_CheckByte = ~command;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_CHECK_EXIST] got check byte &%02x from command port!\n", command);
    }

    switch(command)
    {
    case CH376_CMD_CHECK_EXIST:
        ch376->command = CH376_CMD_CHECK_EXIST;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_CHECK_EXIST] waiting for check byte\n");
        break;

    case CH376_CMD_READ_VAR32:
        ch376->command = CH376_CMD_READ_VAR32;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_READ_VAR32] wait for var offset byte\n");
        break;

    case CH376_CMD_GET_IC_VER:
        ch376->command = CH376_CMD_GET_IC_VER;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_GET_IC_VER]\n");
        break;

    case CH376_CMD_SET_USB_MODE:
        ch376->command = CH376_CMD_SET_USB_MODE;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_SET_USB_MODE] waiting for usb mode data\n");
        break;

    case CH376_CMD_DISK_MOUNT:
        ch376->command = CH376_CMD_DISK_MOUNT;
        cancel_all_io(ch376);
        // If directory is available, we consider that it's mounted!
        if(ch376->usb_mode == CH376_ARG_SET_USB_MODE_SD_HOST)
        {
            ch376->root_dir_lock = system_obtain_directory_lock(&ch376->context, ch376->sdcard_drive_path, NULL);
            // ch376->current_dir_lock = system_clone_directory_lock(&ch376->context, ch376->root_dir_lock);
        }
        else if(ch376->usb_mode == CH376_ARG_SET_USB_MODE_USB_HOST)
        {
            ch376->root_dir_lock = system_obtain_directory_lock(&ch376->context, ch376->usb_drive_path, NULL);
            // ch376->current_dir_lock = system_clone_directory_lock(&ch376->context, ch376->root_dir_lock);
	    }

        if(ch376->root_dir_lock)
        {
            ch376->current_dir_lock = system_clone_directory_lock(&ch376->context, ch376->root_dir_lock);
            ch376->cmd_data.CMD_MountInfo.MOUNT_DeviceType = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_RemovableMedia = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_Versions = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_DataFormatAndEtc = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_AdditionalLength = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_Reserved1 = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_Reserved2 = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_MiscFlag = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[0] = 'J';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[1] = 'E';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[2] = '+';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[3] = 'O';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[4] = 'F';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[5] = '+';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[6] = 'A';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[7] = 'S';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 0] = 'C';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 1] = 'H';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 2] = '3';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 3] = '7';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 4] = '6';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 5] = ' ';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 6] = 'E';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 7] = 'M';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 8] = 'U';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 9] = 'L';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[10] = 'A';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[11] = 'T';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[12] = 'O';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[13] = 'R';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[14] = ' ';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[15] = ' ';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductRevStr[0] = '0';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductRevStr[1] = '1';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductRevStr[2] = '0';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductRevStr[3] = '0';

            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_MOUNT] drive directory is found (mounted :); additional data available\n");

            ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_MountInfo);
            ch376->interface_status = 127; // Found :)
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_MOUNT] drive directory not found\n");
            ch376->command_status = 0x1f; // Don't know if it's this code
        }
        break;

    case CH376_CMD_GET_STATUS:
        ch376->command = CH376_CMD_GET_STATUS;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_GET_STATUS] waiting for status reading\n");
        break;

    case CH376_CMD_SET_FILE_NAME:
        ch376->command = CH376_CMD_SET_FILE_NAME;
        ch376->pos_rw_in_cmd_data = 0;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_SET_FILE_NAME] waiting for file name\n");
        break;

    case CH376_CMD_FILE_OPEN:
        ch376->command = CH376_CMD_FILE_OPEN;
        // mounted?
        if(ch376->root_dir_lock)
        {
            int i = 0;
            int j = 0;
            for (j = 0; j < 8+3+1+1; j++)
            {
                if (ch376->cmd_data.CMD_FileName[j] == '\0')
                {
                    // End of string
                    break;
                }

                if (check_fat32_char(ch376->cmd_data.CMD_FileName[j]) == -1)
                {
                    dbg_printf("[PANIC][WRITE][COMMAND][CH376_CMD_FILE_OPEN] error: invalid character in file name : %d/current 6502 PC : 0x%x\n", ch376->cmd_data.CMD_FileName[j], oric_bus->cpu->lastpc);
                    printf("[PANIC][WRITE][COMMAND][CH376_CMD_FILE_OPEN] error: invalid character in file name : %d/current 6502 PC : 0x%x\n", ch376->cmd_data.CMD_FileName[j], oric_bus->cpu->lastpc);
                    ch376->interface_status = 0;
                    ch376->command_status = CH376_RET_ABORT;
                    break;
                }
            }

            if (strlen(ch376->cmd_data.CMD_FileName) > 8+3+1+1) // 8.3 + EOS
            {
                printf("[PANIC] String for CH376_CMD_FILE_OPEN is too long : %s\n", ch376->cmd_data.CMD_FileName);
                dbg_printf("[PANIC][WRITE][COMMAND][CH376_CMD_FILE_OPEN] error: file name too long\n");
                ch376->interface_status = 0;
                ch376->command_status = CH376_RET_ABORT;
                break;
            }

                // back to root?
            if(ch376->cmd_data.CMD_FileName[i] == '/')
            {
                dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] opening root directory\n");
                system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
                ch376->current_dir_lock = system_clone_directory_lock(&ch376->context, ch376->root_dir_lock);
                i++;
            }

            if(ch376->cmd_data.CMD_FileName[i] == '\0')
            {
                // We are done
                ch376->interface_status = 127; // Found :)
                ch376->command_status = CH376_ERR_OPEN_DIR;
                ch376->current_file_is_directory = CH376_TRUE;
            }
            else
            {
                // wildcard?
                if(strchr(ch376->cmd_data.CMD_FileName,'*') || strchr(ch376->cmd_data.CMD_FileName,'?'))
                {
                    // Directory?
                    if (ch376->current_file_is_directory)
                    {
                            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] examining directory contents\n");
                            // Start a directory examine session
                            system_finish_examine_directory(&ch376->context, ch376->current_directory_browsing);
                            ch376->current_directory_browsing = system_start_examine_directory(&ch376->context, ch376->current_dir_lock);
                            normalize_pattern(ch376->cmd_data.CMD_FileName, ch376->dir_pattern);

                            goto file_enum_go;
                    }

                    dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] examining directory contents: not a directory\n");
                    ch376->interface_status = 0;
                    ch376->command_status = CH376_ERR_MISS_FILE;

                }
                else
                {
                    char fixed_file_name[13]; // Max = 8 + '.' + 3 + '\0'
                    CH376_LOCK current_dir_lock;
                    trim_file_name(&ch376->cmd_data.CMD_FileName[i], fixed_file_name);

                    current_dir_lock = system_obtain_directory_lock(&ch376->context, fixed_file_name, ch376->current_dir_lock);

                    // Enter new directory?
                    if(current_dir_lock)
                    {
                        dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] entering directory: %s\n", &ch376->cmd_data.CMD_FileName[i]);

                        system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
                        ch376->current_dir_lock = current_dir_lock;
                        ch376->current_file_is_directory = CH376_TRUE;

                        ch376->interface_status = 127; // Found :)
                        ch376->command_status = CH376_ERR_OPEN_DIR;
                    }
                    // Open existing file?
                    else
                    {
                        system_file_close(&ch376->context, ch376->current_file);
                        ch376->current_file = system_file_open_existing(&ch376->context, fixed_file_name, ch376->current_dir_lock);

                        dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] opening the file: %s\n", &ch376->cmd_data.CMD_FileName[i]);

                        if(ch376->current_file)
                        {
                            ch376->interface_status = 127; // Found :)
                            ch376->command_status = CH376_INT_SUCCESS;
                            ch376->current_file_is_directory = CH376_FALSE;
                            strncpy(ch376->dir_pattern, fixed_file_name, sizeof(fixed_file_name));
                            // ch376->buffer_read_count = 0; // Init read buffer count (also needed when opendir?)
                            ch376->current_pos = (CH376_S32)0;
                        }
                        else
                        {
                            ch376->interface_status = 0;
                            ch376->command_status = CH376_ERR_MISS_FILE;
                        }
                    }
                }
            }
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] no operation possible (device not mounted)\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_FILE_CREATE:
        ch376->command = CH376_CMD_FILE_CREATE;
        // mounted?
        if(ch376->root_dir_lock)
        {
            int i = 0;
            int j = 0;
            for (j=0; j < 8+3+1+1; j++)
            {
                if (ch376->cmd_data.CMD_FileName[j] == '\0')
                {
                    // End of string
                    break;
                }

                if (check_fat32_char(ch376->cmd_data.CMD_FileName[j]) == -1)
                {
                    dbg_printf("[PANIC][WRITE][COMMAND][CH376_CMD_FILE_CREATE] error: invalid character in file name : %d\n", ch376->cmd_data.CMD_FileName[j]);
                    printf("[PANIC][WRITE][COMMAND][CH376_CMD_FILE_CREATE] error: invalid character in file name : %d\n", ch376->cmd_data.CMD_FileName[j]);
                    ch376->interface_status = 0;
                    ch376->command_status = CH376_RET_ABORT;
                    break;
                }
            }

            if (strlen(ch376->cmd_data.CMD_FileName) > 8+3+1+1) // 8.3 + EOS
            {
                printf("[PANIC] String for CH376_CMD_FILE_CREATE is too long : %s\n", ch376->cmd_data.CMD_FileName);
                dbg_printf("[PANIC][WRITE][COMMAND][CH376_CMD_FILE_CREATE] error: file name too long\n");
                ch376->interface_status = 0;
                ch376->command_status = CH376_RET_ABORT;
                break;
            }


            // back to root?
            if(ch376->cmd_data.CMD_FileName[i] == '/')
            {
                dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_CREATE] opening root directory\n");
                system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
                ch376->current_dir_lock = system_clone_directory_lock(&ch376->context, ch376->root_dir_lock);
                i++;
            }

            if(ch376->cmd_data.CMD_FileName[i] == '\0')
            {
                // We are done
                ch376->interface_status = 127; // Found :)
                ch376->command_status = CH376_INT_SUCCESS;
            }
            else
            {
                char fixed_file_name[13]; // Max = 8 + '.' + 3 + '\0'
                trim_file_name(&ch376->cmd_data.CMD_FileName[i], fixed_file_name);

                // Create file
                system_file_close(&ch376->context, ch376->current_file);
                ch376->current_file = system_file_open_new(&ch376->context, fixed_file_name, ch376->current_dir_lock);

                dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_CREATE] creating the file: %s\n", &ch376->cmd_data.CMD_FileName[i]);

                if(ch376->current_file)
                {
                    ch376->interface_status = 127; // Found :)
                    ch376->command_status = CH376_INT_SUCCESS;
                    ch376->current_pos = (CH376_S32)0;
                }
                else
                {
                    ch376->interface_status = 0;
                    ch376->command_status = CH376_ERR_MISS_FILE;
                }
            }
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_CREATE] no operation possible (device not mounted)\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_DIR_CREATE:
        ch376->command = CH376_CMD_DIR_CREATE;
        // mounted?
        if(ch376->root_dir_lock)
        {
            char fixed_file_name[13]; // Max = 8 + '.' + 3 + '\0'
            CH376_LOCK created_dir_lock;
            CH376_FILE existing_file;
            int i = 0;
            int j = 0;

            if (ch376->cmd_data.CMD_FileName[j] == '\0')
            {
                // End of string
                break;
            }

            for (j = 0; j < 8+3+1+1; j++)
            {
                if (check_fat32_char(ch376->cmd_data.CMD_FileName[j]) == -1)
                {
                    dbg_printf("[PANIC][WRITE][COMMAND][CH376_CMD_FILE_CREATE] error: invalid character in file name : %d\n", ch376->cmd_data.CMD_FileName[j]);
                    printf("[PANIC][WRITE][COMMAND][CH376_CMD_FILE_CREATE] error: invalid character in file name : %d\n", ch376->cmd_data.CMD_FileName[j]);
                    ch376->interface_status = 0;
                    ch376->command_status = CH376_RET_ABORT;
                    break;
                }
            }

            if (strlen(ch376->cmd_data.CMD_FileName) > 8+3+1+1) // 8.3 + EOS
            {
                printf("[PANIC] String for CH376_CMD_FILE_CREATE is too long : %s\n", ch376->cmd_data.CMD_FileName);
                dbg_printf("[PANIC][WRITE][COMMAND][CH376_CMD_FILE_CREATE] error: file name too long\n");
                ch376->interface_status = 0;
                ch376->command_status = CH376_RET_ABORT;
                break;
            }

            // back to root?
            if(ch376->cmd_data.CMD_FileName[i] == '/')
            {
                dbg_printf("[WRITE][COMMAND][CH376_CMD_DIR_CREATE] opening root directory\n");
                system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
                ch376->current_dir_lock = system_clone_directory_lock(&ch376->context, ch376->root_dir_lock);
                i++;
            }

            trim_file_name(&ch376->cmd_data.CMD_FileName[i], fixed_file_name);

            created_dir_lock = system_obtain_directory_lock(&ch376->context, fixed_file_name, ch376->current_dir_lock);

           // Enter new directory?
           if(created_dir_lock)
           {
                dbg_printf("[WRITE][COMMAND][CH376_CMD_DIR_CREATE] entering existing directory: %s\n", &ch376->cmd_data.CMD_FileName[i]);

                system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
                ch376->current_dir_lock = created_dir_lock;

                ch376->interface_status = 127;
                ch376->command_status = CH376_INT_SUCCESS;
           }
           else
           {
                existing_file = system_file_open_existing(&ch376->context, fixed_file_name, ch376->current_dir_lock);

                if(existing_file)
                {
                    system_file_close(&ch376->context, existing_file);
                    ch376->interface_status = 0;
                    ch376->command_status = CH376_ERR_FOUND_NAME;
                }
                else
                {
                    // CH376_LOCK created_dir_lock;

                    // Already created?
                    created_dir_lock = system_obtain_directory_lock(&ch376->context, fixed_file_name, ch376->current_dir_lock);

                    if (!created_dir_lock)
                        created_dir_lock = system_create_directory(&ch376->context, fixed_file_name, ch376->current_dir_lock);

                    // Actually created?
                    if(created_dir_lock)
                    {
                        dbg_printf("[WRITE][COMMAND][CH376_CMD_DIR_CREATE] entering created directory: %s\n", &ch376->cmd_data.CMD_FileName[i]);

                        system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
                        ch376->current_dir_lock = created_dir_lock;

                        ch376->interface_status = 127;
                        ch376->command_status = CH376_INT_SUCCESS;
                    }
                    else
                    {
                        dbg_printf("[WRITE][COMMAND][CH376_CMD_DIR_CREATE] directory could not be created: %s\n", &ch376->cmd_data.CMD_FileName[i]);

                        ch376->interface_status = 0;
                        ch376->command_status = CH376_ERR_MISS_FILE;
                    }
                }
            }
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_DIR_CREATE] no operation possible (device not mounted)\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_FILE_ERASE:
        ch376->command = CH376_CMD_FILE_ERASE;
        // mounted?
        if(ch376->root_dir_lock)
        {
            if(ch376->current_file)
            {
                //if(system_file_delete(&ch376->context, ch376->current_file))
                system_file_close(&ch376->context, ch376->current_file);

                if(system_file_delete(&ch376->context, ch376->dir_pattern, ch376->current_dir_lock))
                {
                    dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ERASE] file deleted\n");

                    ch376->interface_status = 127;
                    ch376->command_status = CH376_INT_SUCCESS;
                }
                else
                {
                    dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ERASE] file could not be deleted\n");

                    ch376->interface_status = 0;
                    ch376->command_status = CH376_RET_ABORT;
                }

                // success or not, current_file is not valid anymore
                ch376->current_file = (CH376_FILE)0;
            }
            else if(!system_is_root_dir(&ch376->context, ch376->current_dir_lock, ch376->root_dir_lock))
            {
                if(system_directory_delete(&ch376->context, ch376->current_dir_lock))
                {
                    dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ERASE] directory deleted\n");

                    ch376->interface_status = 127;
                    ch376->command_status = CH376_INT_SUCCESS;
                }
                else
                {
                    dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ERASE] directory could not be deleted\n");

                    ch376->interface_status = 0;
                    ch376->command_status = CH376_RET_ABORT;
                }

                // success or not, current_dir_lock is not valid anymore, go back to root by default
                ch376->current_dir_lock = system_clone_directory_lock(&ch376->context, ch376->root_dir_lock);
            }
            else
            {
                dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ERASE] no operation possible (file or directory not opened)\n");

                ch376->interface_status = 0;
                ch376->command_status = CH376_ERR_MISS_FILE;
            }
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ERASE] no operation possible (device not mounted)\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_RD_USB_DATA0:
        ch376->command = CH376_CMD_RD_USB_DATA0;
        ch376->pos_rw_in_cmd_data = CMD_DATA_REQ_SIZE; // Will be reset when size is sent
        ch376->pos_in_usb_data = 0;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_RD_USB_DATA0] waiting for i/o buffer read\n");
        break;

    case CH376_CMD_WR_REQ_DATA:
        ch376->command = CH376_CMD_WR_REQ_DATA;
        ch376->pos_rw_in_cmd_data = CMD_DATA_REQ_SIZE; // Will be reset when size is sent
        dbg_printf("[WRITE][COMMAND][CH376_CMD_WR_REQ_DATA] waiting for i/o buffer write\n");
        break;

    case CH376_CMD_FILE_ENUM_GO:
        ch376->command = CH376_CMD_FILE_ENUM_GO;
file_enum_go:
        if(system_go_examine_directory(&ch376->context, ch376->current_dir_lock, ch376->current_directory_browsing, &ch376->cmd_data.CMD_FatDirInfo, ch376->dir_pattern))
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ENUM_GO] next directory entry in buffer\n");
            ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_FatDirInfo);
            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_DISK_READ;
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ENUM_GO] directory browing finished\n");

            system_finish_examine_directory(&ch376->context, ch376->current_directory_browsing);
            ch376->current_directory_browsing = (CH376_DIR)0;

            ch376->interface_status = 0;
            ch376->command_status = CH376_ERR_OPEN_DIR;
        }
        break;

    case CH376_CMD_DISK_CAPACITY:
        ch376->command = CH376_CMD_DISK_CAPACITY;
        // mounted?
        if(system_get_disk_info(&ch376->context, ch376->root_dir_lock, &ch376->cmd_data.CMD_DiskQuery))
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_CAPACITY] sector capacity in buffer\n");
            // Only first 4 bytes of cmd_data.CMD_DiskQuery
            ch376->nb_bytes_in_cmd_data = 4;
            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_CAPACITY] no operation possible (device not mounted)\n");
            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_DISK_QUERY:
        ch376->command = CH376_CMD_DISK_QUERY;
        // mounted?
        if(system_get_disk_info(&ch376->context, ch376->root_dir_lock, &ch376->cmd_data.CMD_DiskQuery))
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_QUERY] sector capacity in buffer\n");
            ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_DiskQuery);
            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_QUERY] no operation possible (device not mounted)\n");
            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_BYTE_LOCATE:
        ch376->command = CH376_CMD_BYTE_LOCATE;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_LOCATE] waiting for seek position\n");
        ch376->pos_rw_in_cmd_data = 0;
        break;

    case CH376_CMD_BYTE_READ:
        ch376->command = CH376_CMD_BYTE_READ;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_READ] waiting for read size\n");
        ch376->pos_rw_in_cmd_data = 0;
        break;

    case CH376_CMD_BYTE_RD_GO:
        ch376->command = CH376_CMD_BYTE_RD_GO;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_RD_GO] waiting for next chunk to read\n");
        file_read_chunk(ch376);
        break;

    case CH376_CMD_BYTE_WRITE:
        ch376->command = CH376_CMD_BYTE_WRITE;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_WRITE] waiting for write size\n");
        ch376->pos_rw_in_cmd_data = 0;
        break;

    case CH376_CMD_BYTE_WR_GO:
        ch376->command = CH376_CMD_BYTE_WR_GO;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_WR_GO] waiting for next chunk to write\n");
        file_write_chunk(ch376);
        break;

    case CH376_CMD_FILE_CLOSE:
        ch376->command = CH376_CMD_FILE_CLOSE;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_CLOSE] waiting for close mode\n");
        break;

    // USB management
    case CH376_CMD_SET_USB_SPEED:
        ch376->command = CH376_CMD_SET_USB_SPEED;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_SET_USB_SPEED] Waiting for one data\n");
        break;

    case CH376_CMD_SET_REGISTER:
        ch376->command = CH376_CMD_SET_REGISTER;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_SET_REGISTER] Waiting for two data (register and value)\n");
        break;

    case CH376_SET_USB_ADDR:
        ch376->command = CH376_SET_USB_ADDR;
        dbg_printf("[WRITE][COMMAND][CH376_SET_USB_ADDR] Waiting for data from data port\n");
        break;

    case CH376_CMD_SET_CONFIG:
        ch376->command = CH376_CMD_SET_CONFIG;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_SET_CONFIG] Waiting for data from data port\n");
        break;

    case CH376_CMD_ISSUE_TKN_X:
        ch376->command = CH376_CMD_ISSUE_TKN_X;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_ISSUE_TKN_X] Waiting for data from data port\n");
        break;

    case CH376_CMD_SET_ADDR:
        ch376->command = CH376_CMD_SET_ADDR;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_SET_ADDR] Waiting for data from data port\n");
        break;

    default:
        dbg_printf("[WRITE][COMMAND][Unsupported] command &%02x not implemented\n", ch376->command);
        ch376->interface_status = 0;
        ch376->command_status = CH376_RET_ABORT;
        break;
    }

    dbg_printf("<< [WRITE][COMMAND] Write command &%02x status &%02x\n", ch376->command, ch376->command_status);
}

/* /// */

/* /// "CH376 public write data port" */

void ch376_write_data_port(struct ch376 *ch376, CH376_U8 data, struct expansion_bus *oric_bus)
{
  dbg_printf(">> [WRITE][DATA] Write data &%02x status &%02x\n", data, ch376->command_status);

  switch(ch376->command)
  {
    case CH376_CMD_CHECK_EXIST:
        ch376->cmd_data.CMD_CheckByte = ~data;
        dbg_printf("[WRITE][DATA][CH376_CMD_CHECK_EXIST] got check byte &%02x\n", data);
        break;

    case CH376_CMD_READ_VAR32:
        // dbg_printf("[WRITE][DATA][CH376_CMD_GET_FILE_SIZE] got &%02x byte\n", data);
        if(data == CH376_VAR_FILE_SIZE)
        {
            CH376_S32 file_size = system_get_file_size(&ch376->context, ch376->current_file);

            ch376->cmd_data.CMD_VAR32[0] = (CH376_U8)((file_size & 0x000000ff) >>  0);
            ch376->cmd_data.CMD_VAR32[1] = (CH376_U8)((file_size & 0x0000ff00) >>  8);
            ch376->cmd_data.CMD_VAR32[2] = (CH376_U8)((file_size & 0x00ff0000) >> 16);
            ch376->cmd_data.CMD_VAR32[3] = (CH376_U8)((file_size & 0xff000000) >> 24);

            ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_VAR32);
            ch376->pos_rw_in_cmd_data = 0;

            dbg_printf("[WRITE][DATA][CH376_CMD_GET_FILE_SIZE] done, file size is %d, waiting for data read\n", file_size);

            // Lignes suivantes utiles?
            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_SUCCESS;
	    }
	    else if(data == CH376_VAR_CURRENT_OFFSET)
        {
            CH376_S32 file_offset = system_get_file_offset(&ch376->context, ch376->current_file);

            ch376->cmd_data.CMD_VAR32[0] = (CH376_U8)((file_offset & 0x000000ff) >>  0);
            ch376->cmd_data.CMD_VAR32[1] = (CH376_U8)((file_offset & 0x0000ff00) >>  8);
            ch376->cmd_data.CMD_VAR32[2] = (CH376_U8)((file_offset & 0x00ff0000) >> 16);
            ch376->cmd_data.CMD_VAR32[3] = (CH376_U8)((file_offset & 0xff000000) >> 24);

            ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_VAR32);
            ch376->pos_rw_in_cmd_data = 0;

            dbg_printf("[WRITE][DATA][CH376_CMD_GET_FILE_OFFSET] done, file position is %d, waiting for data read\n", file_offset);

            // Lignes suivantes utiles?
            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_SUCCESS;
        }
	    else
	    {
            dbg_printf("[WRITE][DATA][CH376_CMD_READ_VAR32] wrong command byte: looking for &68 or &6c, got &%02x\n", data);

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_SET_USB_MODE:
        cancel_all_io(ch376);
        switch(data)
        {
            case CH376_ARG_SET_USB_MODE_USB_HOST:
                ch376->usb_mode = CH376_ARG_SET_USB_MODE_USB_HOST;
                dbg_printf("[WRITE][DATA][CH376_SET_USB_MODE] USB host set\n");
                break;
            case CH376_ARG_SET_USB_MODE_SD_HOST:
                ch376->usb_mode = CH376_ARG_SET_USB_MODE_SD_HOST;
                dbg_printf("[WRITE][DATA][CH376_SET_USB_MODE] SD card set\n");
                break;
            case CH376_ARG_SET_USB_HOST_RESET_USB_BUS:
                ch376->usb_mode = CH376_ARG_SET_USB_HOST_RESET_USB_BUS;
                dbg_printf("[WRITE][DATA][CH376_SET_USB_MODE] Reset usb bus\n");
                break;
            default:
                ch376->usb_mode = CH376_ARG_SET_USB_MODE_INVALID;
                dbg_printf("[WRITE][DATA][CH376_SET_USB_MODE_CODE_INVALID] set\n");
                break;
        }
        break;

    case CH376_CMD_SET_FILE_NAME:
        dbg_printf("[WRITE][DATA][CH376_CMD_SET_FILE_NAME] got file name character \"%c\" (&%02x) for position %d\n", data, data, ch376->pos_rw_in_cmd_data);
        // protect against invalid characters

        if (check_fat32_char(data) == -1)
        {
            dbg_printf("[PANIC][WRITE][COMMAND][CH376_CMD_SET_FILE_NAME] error: invalid character in file name : %d/current 6502 PC : 0x%x\n", data, oric_bus->cpu->lastpc);
            printf("[PANIC][WRITE][COMMAND][CH376_CMD_SET_FILE_NAME] error: invalid character in file name : %d/current 6502 PC : 0x%x\n", data, oric_bus->cpu->lastpc);
            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
            break;
        }

        // protect buffer overflow
        if(ch376->pos_rw_in_cmd_data < sizeof(ch376->cmd_data.CMD_FileName))
        {
            // store new filename character
            ch376->cmd_data.CMD_FileName[ch376->pos_rw_in_cmd_data++] = data;
            ch376->cmd_data.CMD_FileName[ch376->pos_rw_in_cmd_data] = '\0';
//            ch376->current_file_is_directory = CH376_FALSE;
        }
        break;

    case CH376_CMD_BYTE_LOCATE:
        if(ch376->pos_rw_in_cmd_data < sizeof(ch376->cmd_data.CMD_FileSeek))
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_LOCATE] got byte #%d with value &%02x\n", ch376->pos_rw_in_cmd_data, data);
            ch376->cmd_data.CMD_FileSeek[ch376->pos_rw_in_cmd_data] = data;

            if(++ch376->pos_rw_in_cmd_data == sizeof(ch376->cmd_data.CMD_FileSeek))
            {
                CH376_S32 file_seek_pos = (ch376->cmd_data.CMD_FileSeek[0] <<  0)
                                        | (ch376->cmd_data.CMD_FileSeek[1] <<  8)
                                        | (ch376->cmd_data.CMD_FileSeek[2] << 16)
                                        | (ch376->cmd_data.CMD_FileSeek[3] << 24);

                file_seek_pos = system_file_seek(&ch376->context, ch376->current_file, file_seek_pos);

                if(file_seek_pos >= 0)
                {
                    dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_LOCATE] all done: seek operated with success: %d (waiting for data read)\n", file_seek_pos);

                    ch376->cmd_data.CMD_FileSeek[0] = (CH376_U8)((file_seek_pos & 0x000000ff) >>  0);
                    ch376->cmd_data.CMD_FileSeek[1] = (CH376_U8)((file_seek_pos & 0x0000ff00) >>  8);
                    ch376->cmd_data.CMD_FileSeek[2] = (CH376_U8)((file_seek_pos & 0x00ff0000) >> 16);
                    ch376->cmd_data.CMD_FileSeek[3] = (CH376_U8)((file_seek_pos & 0xff000000) >> 24);

                    ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_FileSeek);
                    ch376->interface_status = 127;
                    ch376->command_status = CH376_INT_SUCCESS;

                    ch376->current_pos = file_seek_pos;
                }
                else
                {
                    dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_LOCATE] all done: seek failure\n");

                    ch376->interface_status = 0;
                    ch376->command_status = CH376_RET_ABORT;
                }
            }
        }
        break;

    case CH376_CMD_WR_REQ_DATA:
        if(ch376->nb_bytes_in_cmd_data
        && ch376->pos_rw_in_cmd_data != CMD_DATA_REQ_SIZE)
        {
            CH376_U8 *raw = (CH376_U8 *)&ch376->cmd_data;

            if(ch376->nb_bytes_in_cmd_data != ch376->pos_rw_in_cmd_data)
            {
                raw[ch376->pos_rw_in_cmd_data] = data;

		if (!ch376->current_file_is_directory)
                    ++ch376->current_pos;

                dbg_printf("[WRITE][DATA][CH376_CMD_WR_REQ_DATA] write \"%c\" (&%02x) to i/o buffer at position &%02x\n", data, data, ch376->pos_rw_in_cmd_data);

                if(++ch376->pos_rw_in_cmd_data == ch376->nb_bytes_in_cmd_data)
                {
                    ch376->interface_status = 0;
                    ch376->command_status = CH376_RET_SUCCESS;
                }
            }
        }
        else
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_WR_REQ_DATA] nothing to write to i/o buffer\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_BYTE_READ:
        if(ch376->pos_rw_in_cmd_data < sizeof(ch376->cmd_data.CMD_FileReadWrite))
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_READ] got byte #%d with value &%02x\n", ch376->pos_rw_in_cmd_data, data);
            ch376->cmd_data.CMD_FileReadWrite[ch376->pos_rw_in_cmd_data] = data;

            if(++ch376->pos_rw_in_cmd_data == sizeof(ch376->cmd_data.CMD_FileReadWrite))
            {
                ch376->bytes_to_read_write = (ch376->cmd_data.CMD_FileReadWrite[0] <<  0)
                                           | (ch376->cmd_data.CMD_FileReadWrite[1] <<  8);
                ch376->buffer_read_count = 0; // Init read buffer count (also needed when opendir?)
                file_read_chunk(ch376);
            }
        }
        break;

    case CH376_CMD_BYTE_WRITE:
        if(ch376->pos_rw_in_cmd_data < sizeof(ch376->cmd_data.CMD_FileReadWrite))
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_WRITE] got byte #%d with value &%02x\n", ch376->pos_rw_in_cmd_data, data);
            ch376->cmd_data.CMD_FileReadWrite[ch376->pos_rw_in_cmd_data] = data;

            if(++ch376->pos_rw_in_cmd_data == sizeof(ch376->cmd_data.CMD_FileReadWrite))
            {
                ch376->bytes_to_read_write = (ch376->cmd_data.CMD_FileReadWrite[0] <<  0)
                                           | (ch376->cmd_data.CMD_FileReadWrite[1] <<  8);

                if(ch376->bytes_to_read_write > sizeof(ch376->cmd_data.CMD_IOBuffer))
                    ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_IOBuffer);
                else
                    ch376->nb_bytes_in_cmd_data = (CH376_U8)ch376->bytes_to_read_write;

                ch376->interface_status = 127;
                if (ch376->bytes_to_read_write == 1)
                    ch376->command_status = CH376_INT_DISK_WRITE;
                else
                    ch376->command_status = CH376_INT_SUCCESS;
            }
        }
        break;

    case CH376_CMD_FILE_CLOSE:
        // Note: close mode is not implemented; size if always updated
        // dbg_printf("[WRITE][DATA][CH376_CMD_FILE_CLOSE] update size: %s\n", ch376->pos_rw_in_cmd_data == 0 ? "no" : "yes");
        dbg_printf("[WRITE][DATA][CH376_CMD_FILE_CLOSE] update size: %s\n", data == 0 ? "no" : "yes");
        if(ch376->current_file)
        {
            system_file_close(&ch376->context, ch376->current_file);
            ch376->current_file = (CH376_FILE)0;

            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_FILE_CLOSE] failure: not file opened\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    // USB management
    case CH376_CMD_SET_USB_SPEED:
        ch376->usb_speed = data;
        dbg_printf("[WRITE][DATA][CH376_CMD_SET_USB_SPEED] setting usb speed to : ");
        if (ch376->usb_speed == CH376_USB_SPEED_FULL_12MBPS)
            dbg_printf("CH376_USB_SPEED_FULL_12MBPS\n");
        else if (ch376->usb_speed == CH376_USB_SPEED_FULL_1_5MBPS)
            dbg_printf("CH376_USB_SPEED_FULL_1_5MBPS\n");
        else if (ch376->usb_speed == CH376_USB_SPEED_LOW_1_5MBPS)
            dbg_printf("CH376_USB_SPEED_LOW_1_5MBPS\n");
        else
            dbg_printf("Panic !!! Unknown speed mode : %d\n", data);
        break;

    case CH376_CMD_SET_REGISTER:
        if (ch376->current_register_write == 0xff)
        {
            ch376->current_register_write = data;
            dbg_printf("[WRITE][DATA][CH376_CMD_SET_REGISTER] register 0x%x selected\n", data);
        }
        else
        {
            // Setting value
            dbg_printf("[WRITE][DATA][CH376_CMD_SET_REGISTER] register 0x%x set to 0x%x\n", ch376->current_register_write, data);
            ch376->chip_registers[ch376->current_register_write] = data;
            ch376->current_register_write = 0xff;
        }
        break;

    case CH376_SET_USB_ADDR:
        dbg_printf("[WRITE][DATA][CH376_SET_USB_ADDR] talking to 0x%x device\n", data);
        ch376->current_device_address = data;
        break;


    case CH376_CMD_ISSUE_TKN_X:
        if (ch376->issue_tkn_is_set == ISSUE_TKN_IS_NOT_SET)
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_ISSUE_TKN_X] setting tkn with %x value\n", data);
            ch376->issue_tkn = data;
            ch376->issue_tkn_is_set = ISSUE_TKN_IS_SET;
        }
        else
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_ISSUE_TKN_X] setting tkn with %x operation_descriptor, resetting tkn state ...\n", data);
            ch376->issue_tkn_is_set = ISSUE_TKN_IS_NOT_SET;
            ch376->operation_descriptor = data;
        }
        break;

    case CH376_CMD_SET_CONFIG:
        //USBDEVICE_Config;
        // Looking for device with current usb address
        int i;
        for (i = 0; i < CH376_MAX_USB_DEVICES; i++) // We are looking device from 0 to max usb devices
        {
            if (ch376->usbdevices[i].USBDEVICE_Address == ch376->current_device_address && ch376->usbdevices[i].USBDEVICE_Is_Connected == USBDEVICE_IS_CONNECTED)
                break;
            else
                 dbg_printf("Error %d for current device %d address : %d because device is %d connected\n", i, ch376->current_device_address, ch376->usbdevices[i].USBDEVICE_Address, USBDEVICE_IS_CONNECTED);
        }
        // We found device, setting to device
        if (i < CH376_MAX_USB_DEVICES)
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_SET_CONFIG] setting current 0x%x usb device with config 0x%x ...\n", ch376->current_device_address, data);
            ch376->usbdevices[i].USBDEVICE_Config = data;
        }
        else
            dbg_printf("[WRITE][DATA][CH376_CMD_SET_CONFIG] Panic we did not found device with usb address \n", ch376->current_device_address);
        break;


    case CH376_CMD_SET_ADDR:
        // Device not connected
        if (ch376->usbdevices[ch376->current_usb_device_to_set_adress].USBDEVICE_Is_Connected == USBDEVICE_IS_NOT_CONNECTED)
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_SET_ADDR] No device connected with current 0 address\n");
            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT; // Dunno what ch376 returns in that case when there is no connected devices
        }
        else
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_SET_ADDR] current device on bus : configuring with 0x%x usb adress device id : %x\n", data, ch376->current_usb_device_to_set_adress);
            ch376->usbdevices[ch376->current_usb_device_to_set_adress].USBDEVICE_Address = data;
            ch376->current_usb_device_to_set_adress ++;
            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_SUCCESS;

        }

        break;
    }

    dbg_printf("<< [WRITE][DATA] Write data &%02x status &%02x\n", data, ch376->command_status);

}



/* /// */

/* /// "CH376 public init & clean" */

struct ch376 * ch376_create(void *user_data)
{
    int i;
    struct ch376 *ch376 = system_alloc_mem(sizeof(struct ch376));

    if(ch376)
    {
        if(system_init_context(&ch376->context, user_data))
        {
            ch376->sdcard_drive_path = clone_string("ch376_sdcard_drive/");
            ch376->usb_drive_path = clone_string("ch376_usb_drive/");
            ch376->usb_speed = CH376_USB_SPEED_FULL_12MBPS;
            ch376->current_register_write = 0xff;
            for (i = 0; i < CH376_MAX_USB_DEVICES; i++)
            {
                ch376->usbdevices[CH376_MAX_USB_DEVICES].USBDEVICE_Address = 0;
                ch376->usbdevices[CH376_MAX_USB_DEVICES].USBDEVICE_Is_Connected = USBDEVICE_IS_NOT_CONNECTED;
            }
            ch376->current_usb_device_to_set_adress = 0;
            ch376->issue_tkn_is_set = ISSUE_TKN_IS_NOT_SET;
            //Connect an usb mass storage
            ch376->device_connected_to_usb_port = USB_MASS_STORAGE_CLASS;
            // Connect a mouse on usb port
            //ch376->device_connected_to_usb_port = USB_MOUSE_CLASS;
            ch376->usbdevices[0].USBDEVICE_Is_Connected = USBDEVICE_IS_CONNECTED;

            ch376->hid_mouse_deltax = 0;
            ch376->hid_mouse_deltay = 0;
            int state;
            state = SDL_GetMouseState(&ch376->hid_mouse_posx, &ch376->hid_mouse_posy);


            ch376->current_device_address = 0;
            clear_structure(ch376);
        }
        else
        {
            system_free_mem(ch376);
            ch376 = NULL;
        }
    }
    return ch376;
}

void ch376_destroy(struct ch376 *ch376)
{
    cancel_all_io(ch376);
    system_free_mem(ch376->sdcard_drive_path);
    system_free_mem(ch376->usb_drive_path);
    system_clean_context(&ch376->context);
    system_free_mem(ch376);
}

void ch376_reset(struct ch376 *ch376)
{
    cancel_all_io(ch376);
    clear_structure(ch376);
}

/* /// */

/* /// "CH376 public configuration" */

void ch376_set_sdcard_drive_path(struct ch376 *ch376, const char *path)
{
    dbg_printf("ch376_set_sdcard_drive_path: %s\n", path);
    cancel_all_io(ch376);
    system_free_mem(ch376->sdcard_drive_path);
    ch376->sdcard_drive_path = clone_string(path);
}

void ch376_set_usb_drive_path(struct ch376 *ch376, const char *path)
{
    dbg_printf("ch376_set_usb_drive_path: %s\n", path);
    cancel_all_io(ch376);
    system_free_mem(ch376->usb_drive_path);
    ch376->usb_drive_path = clone_string(path);
}

const char * ch376_get_sdcard_drive_path(struct ch376 *ch376)
{
    return ch376->sdcard_drive_path;
}

const char * ch376_get_usb_drive_path(struct ch376 *ch376)
{
    return ch376->usb_drive_path;
}
/* /// */
