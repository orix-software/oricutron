// Commands
#define CH376_CMD_NONE          0x00
#define CH376_CMD_GET_IC_VER    0x01
#define CH376_CMD_CHECK_EXIST   0x06
#define CH376_CMD_READ_VAR32    0x0c
#define CH376_CMD_SET_USB_MODE  0x15
#define CH376_CMD_GET_STATUS    0x22
#define CH376_CMD_RD_USB_DATA0  0x27
#define CH376_CMD_WR_REQ_DATA   0x2d
#define CH376_CMD_SET_FILE_NAME 0x2f
#define CH376_CMD_DISK_MOUNT    0x31
#define CH376_CMD_FILE_OPEN     0x32
#define CH376_CMD_FILE_ENUM_GO  0x33
#define CH376_CMD_FILE_CREATE   0x34
#define CH376_CMD_FILE_ERASE    0x35
#define CH376_CMD_FILE_CLOSE    0x36
#define CH376_CMD_BYTE_LOCATE   0x39
#define CH376_CMD_BYTE_READ     0x3a
#define CH376_CMD_BYTE_RD_GO    0x3b
#define CH376_CMD_BYTE_WRITE    0x3c
#define CH376_CMD_BYTE_WR_GO    0x3d
#define CH376_CMD_DISK_CAPACITY 0x3e
#define CH376_CMD_DISK_QUERY    0x3f
#define CH376_CMD_DIR_CREATE    0x40
#define CH376_CMD_DISK_RD_GO    0x55

#define CH376_ARG_SET_USB_MODE_INVALID  0x00
#define CH376_ARG_SET_USB_MODE_SD_HOST  0x03
#define CH376_ARG_SET_USB_MODE_USB_HOST 0x06

// Status & errors
#define CH376_ERR_OPEN_DIR   0x41
#define CH376_ERR_MISS_FILE  0x42
#define CH376_ERR_FOUND_NAME 0x43

#define CH376_RET_SUCCESS    0x51
#define CH376_RET_ABORT      0x5f

#define CH376_INT_SUCCESS    0x14
#define CH376_INT_DISK_READ  0x1d
#define CH376_INT_DISK_WRITE 0x1e



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
};

