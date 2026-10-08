#define ch395_ORIC_EXTENSION_DATA_PORT    0x380
#define ch395_ORIC_EXTENSION_COMMAND_PORT 0x381

#define CH395_CMD_GET_IC_VER              0x01
#define CH395_CMD_SET_BAUDRATE            0x02
#define CH395_CMD_ENTER_SLEEP             0x03
#define CH395_CMD_RESET_ALL               0x05 // 50ms
#define CH395_CMD_CHECK_EXIST             0x06
#define CH395_CMD_SET_PHY                 0x20
#define CH395_CMD_GET_GLOB_INT_STATUS_ALL 0x19
#define CH395_CMD_SET_MAC_ADDR            0x21 // 6 ?
#define CH395_CMD_SET_IP_ADDR             0x22 // 4
#define CH395_CMD_SET_GWIP_ADDR           0x23 // 4
#define CH395_CMD_SET_MASK_ADDR           0x24 // 4
#define CH395_CMD_SET_MAC_FILT            0x25
#define CH395_CMD_GET_PHY_STATUS          0x26
#define CH395_CMD_INIT                    0x27
#define CH395_CMD_GET_UNREACH_IPPORT      0x28
#define CH395_CMD_GET_GLOB_INT_STATUS     0x29
#define CH395_CMD_SET_RETRAN_COUNT        0x2A
#define CH395_CMD_SET_RETRAN_PERIOD       0x2B
#define CH395_CMD_GET_CMD_STATUS          0x2C
#define CH395_CMD_GET_REMOT_IPP_SN        0x2D
#define CH395_CMD_CLEAR_RECV_BUF_SN       0x2E
#define CH395_CMD_GET_SOCKET_STATUS_SN    0x2F
#define CH395_CMD_GET_INT_STATUS_SN       0x30
#define CH395_CMD_SET_IP_ADDR_SN          0x31
#define CH395_CMD_SET_DES_PORT_SN         0x32
#define CH395_CMD_SET_SOUR_PORT_SN        0x33
#define CH395_CMD_SET_PROTO_TYPE_SN       0x34
#define CH395_CMD_OPEN_SOCKET_SN          0x35
#define CH395_CMD_TCP_LISTEN_SN           0x36
#define CH395_CMD_TCP_CONNECT_SN          0x37
#define CH395_CMD_TCP_DISNCONNECT_SN      0x38
#define CH395_CMD_WRITE_SEND_BUF_SN       0x39
#define CH395_CMD_GET_RECV_LEN_SN         0x3B
#define CH395_CMD_READ_RECV_BUF_SN        0x3C
#define CH395_CMD_CLOSE_SOCKET_SN         0x3D
#define CH395_CMD_SET_IPRAW_PRO_SN        0x3E
#define CH395_CMD_PING_ENABLE             0x3F
#define CH395_CMD_GET_MAC_ADDR            0x40
#define CH395_CMD_DHCP_ENABLE             0x41
#define CH395_CMD_GET_DHCP_STATUS         0x42
#define CH395_CMD_GET_IP_INF              0x43
#define CH395_CMD_PPPOE_SET_USER_NAME     0x44
#define CH395_CMD_PPPOE_SET_PASSWORD      0x45
#define CH395_CMD_PPPOE_ENABLE            0x46
#define CH395_CMD_GET_PPPOE_STATUS        0x47
#define CH395_CMD_SET_TCP_MSS             0x50
#define CH395_CMD_SET_TTL                 0x51
#define CH395_CMD_SET_RECV_BUF            0x52
#define CH395_CMD_SET_SEND_BUF            0x53
#define CH395_CMD_SET_FUN_PARA            0x55
#define CH395_CMD_SET_KEEP_LIVE_IDLE      0x56
#define CH395_CMD_SET_KEEP_LIVE_INTVL     0x57
#define CH395_CMD_SET_KEEP_LIVE_CNT       0x58
#define CH395_CMD_SET_KEEP_LIVE_SN        0x59
#define CH395_CMD_EEPROM_ERASE            0xE9
#define CH395_CMD_EEPROM_WRITE            0xEA
#define CH395_CMD_EEPROM_READ             0xEB
#define CH395_CMD_READ_GPIO_REG           0xEC
#define CH395_CMD_WRITE_GPIO_REG          0xED

//Status

#define CH395_ERR_SUCCESS             0x00 // Success
#define CH395_ERR_BUSY                0x10 // Occupé, indiquant que la commande est en cours d'exécution
#define CH395_ERR_MEM                 0x11 // Erreur de gestion de la mémoire
#define CH395_ERR_BUF                 0x12 // Erreur de tampon
#define CH395_ERR_TIMEOUT             0x13 // Timeout
#define CH395_ERR_RTE                 0x14 // Erreur de routage
#define CH395_ERR_ABRT                0x15 // Abandon de la connexion
#define CH395_ERR_RST                 0x16 // Connexion réinitialisée
#define CH395_ERR_CLSD                0x17 // Connexion fermée
#define CH395_ERR_CONN                0x18 // Pas de connexion
#define CH395_ERR_VAL                 0x19 // Mauvaise valeur
#define CH395_ERR_ARG                 0x1A // Erreur de paramètre
#define CH395_ERR_USE                 0x1B // Déjà utilisé
#define CH395_ERR_IF                  0x1C // Erreur MAC
#define CH395_ERR_ISCONN              0x1D // Connecté
#define CH395_ERR_OPEN                0x20 // Ouvert

#define CH395_DHCP_ENABLE_VAL         0x01
#define CH395_DHCP_DISABLE_VAL        0x00

#define CH395_DHCP_STATUS_ENABLED     0x00
#define CH395_DHCP_STATUS_DISABLED    0x01

#define CH395_NUMBER_MAX_SOCKET       0x08

#define CH395_SOCKET0                 = 0
#define CH395_SOCKET1                 = 1
#define CH395_SOCKET2                 = 2
#define CH395_SOCKET3                 = 3
#define CH395_SOCKET4                 = 4
#define CH395_SOCKET5                 = 5
#define CH395_SOCKET6                 = 6
#define CH395_SOCKET7                 = 7

#define CH395_PROTO_TYPE_TCP          0x03
#define CH395_PROTO_TYPE_UDP          0x02
#define CH395_PROTO_TYPE_MAC_RAW      0x01
#define CH395_PROTO_TYPE_IP_RAW       0x00

#define CH395_SINT_STAT_TIM_OUT       0x40
#define CH395_SINT_STAT_DISCONNECT    0x10
#define CH395_SINT_STAT_CONNECT       0x08
#define CH395_SINT_STAT_RECV          0x04
#define CH395_SINT_STAT_SEND_OK       0x02
#define CH395_SINT_STAT_SENBUF_FREE   0x01

#define CH395_PHY_DISCONN             0x01
#define CH395_PHY_10M_FLL             0x02
#define CH395_PHY_10M_HALF            0x04
#define CH395_PHY_100M_FLL            0x08
#define CH395_PHY_100M_HALF           0x10

#define CH395_SOCKET_CLOSED           0x00
#define CH395_SOCKET_OPEN             0x05

#define CH395_TCP_CLOSED              0x00
#define CH395_TCP_LISTEN              0x01
#define CH395_TCP_SYN_SENT            0x02
#define CH395_TCP_SYN_REVD            0x03
#define CH395_TCP_ESTABLISHED         0x04
#define CH395_TCP_FIN_WAIT_1          0x05
#define CH395_TCP_FIN_WAIT_2          0x06
#define CH395_TCP_CLOSE_WAIT          0x07
#define CH395_TCP_CLOSING                   0x08
#define CH395_TCP_LAST_ACK                  0x09
#define CH395_TCP_TIME_WAIT                 0x0A

#define GINT_STAT_DHCP                      0x08 // Duplicate and should be removed
#define CH395_GINT_STAT_DHCP                0x08

#define CH395_FUN_PARA_FLAG_TCP_SERVER      0x02
#define CH395_FUN_PARA_FLAG_LOW_PWR         0x04
#define CH395_FUN_PARA_FLAG_SOCKET_CLOSE    0x08
#define CH395_FUN_PARA_FLAG_DISABLE_SEND_OK 0x10

#define CH395_GINT_STAT_SOCK0               16
#define CH395_GINT_STAT_SOCK1               32
#define CH395_GINT_STAT_SOCK2               64
#define CH395_GINT_STAT_SOCK3               128

#define CH395_GINT_STAT_PHY_CHANGE          4
#define CH395_GINT_STAT_IP_CONFLI           2
#define CH395_GINT_STAT_UNREACH             1

#define CH395_GINT_STAT_SOCK4               1
#define CH395_GINT_STAT_SOCK5               2
#define CH395_GINT_STAT_SOCK6               4
#define CH395_GINT_STAT_SOCK7               8

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef uint8_t CH395_U8;
typedef int8_t CH395_S8;
typedef uint16_t CH395_U16;
typedef int16_t CH395_S16;
typedef uint32_t CH395_U32;
typedef int32_t CH395_S32;
typedef uint64_t CH395_U64;
typedef int64_t CH395_S64;
typedef bool CH395_BOOL;


#define CH395_TRUE  true
#define CH395_FALSE false

/* For errors management */

#define CH395_CMD_SET_PROTO_TYPE_SN_MAX_BYTES_TO_DATA_PORT 2


union CommandData
{
    uint8_t          CMD_CheckByte;        // [R/W] CH376_CMD_CHECK_EXIST
    uint8_t          CMD_SocketSetProto[2];
    uint8_t          CMD_SocketSetIpAddr[5];
    uint8_t          CMD_SocketSetDesPort[3];
    uint8_t          CMD_SocketSetSrcPort[3];
    uint8_t          CMD_SocketState[1];
    uint8_t          CMD_SocketTTL[1];
    uint8_t          CMD_SocketSetRecvBuf[3];
    uint8_t          CMD_SocketSetSendBuf[3];
    uint8_t          CMD_SocketWriteBuffer[1];
    uint8_t          CMD_SocketGetRecvLen[1];
    uint8_t          CMD_SocketGetRecvBuf[4]; // Store socket, low length and high length : 4 to manage overflow
    uint8_t          CMD_SocketGetIntStatusSn[1];
    uint8_t          CMD_SocketTCPListenSn[1];

};

#define CH395_SIZE_BLOCK_BUFFER 512

struct ch395
{
    int command;
    CH395_U8 command_status;
    CH395_U8 interface_status;
    union CommandData cmd_data;
    CH395_U8 nb_bytes_in_cmd_data;
    CH395_U16 pos_rw_in_cmd_data;

    // Memory
    CH395_U8 buffer[24576];

    // General commands
    CH395_U8 cmd_status;
    CH395_U8 mac_address[6];
    CH395_U8 ip_chip[20];

    // States
    CH395_U8 glob_int_status;
    CH395_U8 glob_int_status_all[2];
    CH395_BOOL is_init; //
    CH395_U8 phy_state;

    // Socket commands
    CH395_U8 socket_proto[8]; // Store protocol
    CH395_U8 socket_status_sn[8][2];
    CH395_U8 socket_dest_port[8][2];
    CH395_U8 socket_src_port[8][2];
    CH395_U8 socket_dest_ip[8][4];
    CH395_U8 socket_ttl[8];
    CH395_U16 socket_length_received[8];
    CH395_U16 socket_length_to_send[8];
    CH395_U8 socket_int_status[8]; // Socket status


    CH395_U16 buffer_position_receive[8]; // Position receveive buffer for each socket
    CH395_U8 buffer_position_transmit[8]; // Position receveive buffer for each socket
    CH395_U16 buffer_position_write_from_data[8]; // ptr send data into buffer from cpu
    CH395_U16 buffer_position_read_from_data[8]; // ptr receive data into buffer from cpu


    CH395_U8 receive_buffer_start_block[8];
    CH395_U8 receive_buffer_number_of_block[8];
    CH395_U8 transmit_buffer_start_block[8];
    CH395_U8 transmit_buffer_number_of_block[8];

    int sockfd_host[8]; // Socket id for emulator


};

