#include "ch395.h"
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

#include "plugin.h"
#include "../../machine.h"

#define INSTANCE_MAX 1

struct ch395 *userdata[INSTANCE_MAX];
struct ch395 *userdata_old[INSTANCE_MAX];


//Used for Linux
#define ROUTE_PATH "/proc/net/route"
#define RESOLV_PATH "/etc/resolv.conf"

/*
TODO :
* Etant donné que le CH395 doit avoir un source port, si la socket a une connexion en court de fermeture et qu'on essaie sur une autre socket ou sur la même
de se connecter avec un source port identique, alors, la connexion ne sera jamais faite, et la tcp_connect échouera tant que le port source n'est pas libre (ou alors il faut en prendre un nouveau
pour initier la connexion)
*/

// --------1---------------------------------------------------------------------
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



// Pour les fonction de lecture du fichier de configuration
#include "../../main.h"

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

#elif defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__) || defined(__MINT__)

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#ifdef __ANDROID__
#include <sys/vfs.h>
#define statvfs statfs
#else
#include <sys/statvfs.h>
#endif
#include <sys/stat.h>
#include <time.h>



#else
#error "FixMe!"
#endif



#ifdef DEBUG_PLUGIN
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

#define BASE_ADDR 0x0360
#define END_ADDR 0x0361


// Nombre d'instances total
unsigned int plugin_instances = 0;


int ch395_check_socket(unsigned char socket, struct ch395 *ch395)
{
    if (socket > 7)
    {
        printf("Invalid socket %d : must be set from 0 to 7\n", socket);
        dbg_printf("Invalid socket %d : must be set from 0 to 7\n", socket);
        return 1;
    }
    return 0;
/*
    if (ch395->socket_state[socket] == CH395_SOCK_CLOSED)
    {
        printf("Socket %d fermé\n", socket);
        dbg_printf("Socket %d fermé\n", socket);
        return;
    }

    if (ch395->socket_state[socket] == CH395_SOCK_LISTEN)
    {
        printf("Socket %d en écoute\n", socket);
        dbg_printf("Socket %d en écoute\n", socket);
        return;
    }

    if (ch395->socket_state[socket] == CH395_SOCK_SYNSENT)
    {
        printf("Socket %d en SYNSENT\n", socket);
        dbg_printf("Socket %d en SYNSENT\n", socket);
        return;
    }

    if (ch395->socket_state[socket] == CH395_SOCK_SYNRECV)
    {
        printf("Socket %d en SYNRECV\n", socket);
        dbg_printf("Socket %d en SYNRECV\n", socket);
        return;
    }

    if (ch395->socket_state[socket] == CH395_SOCK_ESTABLISHED)
    {
        printf("Socket %d établi\n", socket);
        dbg_printf("Socket %d établi\n", socket);
        return;
    }

    if (ch395->socket_state[socket] == CH395_SOCK_FINWAIT)
    {
        printf("Socket %d en FINWAIT\n", socket);
        dbg_printf("Socket %d en FINWAIT\n", socket);
        return;
    }

    if (ch395->socket_state[socket] == CH395_SOCK_CLOSING)
    {
        printf("Socket %d en CLOSING\n", socket);
        dbg_printf("Socket %d en CLOSING\n", socket);
        return;
    }

    if (ch395->socket_state[socket] == CH395_SOCK_TIMEWAIT)
    {
        printf("Socket %d en TIMEWAIT\n", socket);
        dbg_printf("Socket %d en TIMEWAIT\n", socket);
        return;
    }

    if (ch395->socket_state[socket] == CH395_SOCK_CLOSEWAIT)
    {
        printf("Socket %d en CLOSEWAIT\n", socket);
        dbg_printf("Socket %d en CLOSEWAIT\n", socket);
        return;
    }
    */

}


void ch395_debug_concat(char *msg)
{
    printf("%s", msg);
    dbg_printf("%s", msg);

}

int ch395_fill_get_ip_inf_linux(struct ch395 *ch395)
{
    FILE *route_file, *resolv_file;
    char iface[IF_NAMESIZE];
    char line[256];
    struct ifaddrs *ifaddr, *ifa;
    char ip_address[INET_ADDRSTRLEN];
    char gateway_address[INET_ADDRSTRLEN];
    unsigned long gateway;
    char subnet_mask[INET_ADDRSTRLEN];

    unsigned char ip_first_octet = 0, dns1_first_octet = 0, dns2_first_octet = 0;

    // Ouvrir le fichier de routage
    route_file = fopen(ROUTE_PATH, "r");
    if (!route_file) {
        perror("Erreur lors de l'ouverture de /proc/net/route");
        return EXIT_FAILURE;
    }

    // Lire chaque ligne de la table de routage
    while (fgets(line, sizeof(line), route_file)) {
        unsigned long dest, flags;

        if (sscanf(line, "%s %lx %lx %lx", iface, &dest, &gateway, &flags) == 4) {
            if (dest == 0) { // La destination 0.0.0.0 correspond à la passerelle par défaut
                printf("Interface avec passerelle par défaut : %s\n", iface);

                // Convertir l'adresse de la passerelle en format lisible
                struct in_addr gw_addr;
                gw_addr.s_addr = gateway;

                unsigned char *ip_by_bytes = (unsigned char *)&gateway;
                // Gateway
                ch395->ip_chip[7] = ip_by_bytes[0];
                ch395->ip_chip[6] = ip_by_bytes[1];
                ch395->ip_chip[5] = ip_by_bytes[2];
                ch395->ip_chip[4] = ip_by_bytes[3];

                inet_ntop(AF_INET, &gw_addr, gateway_address, INET_ADDRSTRLEN);
                printf("Passerelle : %s\n", gateway_address);

                // Récupérer les adresses associées à cette interface
                if (getifaddrs(&ifaddr) == -1)
                {
                    perror("getifaddrs");
                    fclose(route_file);
                    return EXIT_FAILURE;
                }

                for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
                {
                    if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
                    {
                        continue;
                    }

                    if (strcmp(ifa->ifa_name, iface) == 0)
                    {
                        // Obtenir l'adresse IP en format lisible
                        struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;

                        // Convertir le masque en format lisible
                        inet_ntop(AF_INET, &netmask->sin_addr, subnet_mask, INET_ADDRSTRLEN);
                        printf("Masque de sous-réseau pour %s : %s\n", iface, subnet_mask);

                        unsigned char *ip_by_bytes = (unsigned char *)&netmask->sin_addr.s_addr;
                        //store mask
                        ch395->ip_chip[11] = ip_by_bytes[0];
                        ch395->ip_chip[10] = ip_by_bytes[1];
                        ch395->ip_chip[9] = ip_by_bytes[2];
                        ch395->ip_chip[8] = ip_by_bytes[3];

                        struct sockaddr_in *addr_ip = (struct sockaddr_in *)ifa->ifa_addr;

                        if (inet_ntop(AF_INET, &addr_ip->sin_addr, ip_address, INET_ADDRSTRLEN) != NULL)
                        {
                            printf("Adresse IP de l'interface %s : %s\n", iface, ip_address);
                            unsigned char *ip_by_bytes = (unsigned char *)&addr_ip->sin_addr.s_addr;
                            // Store IP
                            ch395->ip_chip[3] = ip_by_bytes[0];
                            ch395->ip_chip[2] = ip_by_bytes[1];
                            ch395->ip_chip[1] = ip_by_bytes[2];
                            ch395->ip_chip[0] = ip_by_bytes[3];

                        }
                        break;
                    }
                }

                freeifaddrs(ifaddr);
                fclose(route_file);

                // Lecture des serveurs DNS depuis /etc/resolv.conf
                resolv_file = fopen(RESOLV_PATH, "r");
                if (!resolv_file)
                {
                    perror("fileopen error : /etc/resolv.conf");
                    return EXIT_FAILURE;
                }

                printf("Serveurs DNS :\n");
                int dns_count = 0;
                while (fgets(line, sizeof(line), resolv_file))
                {
                    if (strncmp(line, "nameserver", 10) == 0)
                    {
                        char dns[INET_ADDRSTRLEN];
                        if (sscanf(line, "nameserver %s", dns) == 1)
                        {
                            printf("- %s\n", dns);

                            // Extraire le 1er octet du serveur DNS
                            struct in_addr dns_addr;
                            inet_pton(AF_INET, dns, &dns_addr);
                            if (dns_count == 0)
                            {
                                dns1_first_octet = ((unsigned char *)&dns_addr.s_addr)[0];
                                unsigned char *ip_by_bytes = (unsigned char *)&dns_addr.s_addr;

                                ch395->ip_chip[15] = ip_by_bytes[0];
                                ch395->ip_chip[14] = ip_by_bytes[1];
                                ch395->ip_chip[13] = ip_by_bytes[2];
                                ch395->ip_chip[12] = ip_by_bytes[3];

                            } else if (dns_count == 1)
                            {
                                dns2_first_octet = ((unsigned char *)&dns_addr.s_addr)[0];

                                unsigned char *ip_by_bytes = (unsigned char *)&dns_addr.s_addr;

                                ch395->ip_chip[20] = ip_by_bytes[0];
                                ch395->ip_chip[18] = ip_by_bytes[1];
                                ch395->ip_chip[17] = ip_by_bytes[2];
                                ch395->ip_chip[19] = ip_by_bytes[3];
                            }
                            dns_count++;
                        }
                    }
                }

                fclose(resolv_file);

                // Afficher les 1ers octets
                printf("First byte of IP from interface : %u\n", ip_first_octet);
                if (dns1_first_octet != 0)
                {
                    printf("1er octet du 1er serveur DNS : %u\n", dns1_first_octet);
                }
                if (dns2_first_octet != 0)
                {
                    printf("1er octet du 2ème serveur DNS : %u\n", dns2_first_octet);
                }


                return 0;
            }
        }
    }

    fclose(route_file);
    printf("No gateway found.\n");
    return 1;
}

int perform_recv_socket(struct ch395 *ch395, unsigned char socketid)
{
    void *p;
    // Send recv
    int bytes_to_read_from_ch395;
    unsigned char socket = ch395->cmd_data.CMD_SocketWriteBuffer[0];
    // Compute position of the buffer
    // Get buffer offset
    p = &ch395->buffer[ch395->transmit_buffer_start_block[socketid] * CH395_SIZE_BLOCK_BUFFER];
    // Read data from socket
    // We will read into the buffer the size of the received buffer
    // Compute the size
    bytes_to_read_from_ch395 = ch395->receive_buffer_number_of_block[socketid] * CH395_SIZE_BLOCK_BUFFER;
    // And put the size in recv
    int bytes_received = recv(ch395->sockfd_host[socketid], p, bytes_to_read_from_ch395, 0);
    printf("[EMULATOR] Host read (recv) %d bytes (size of receive buffer for the socket) and received : %d bytes, ip : %d.%d.%d.%d\n", bytes_to_read_from_ch395, bytes_received, ch395->socket_dest_ip[socket][0], ch395->socket_dest_ip[socket][1],  ch395->socket_dest_ip[socket][2], ch395->socket_dest_ip[socket][3]);
    dbg_printf("[EMULATOR] Host read (recv) %d bytes (size of receive buffer for the socket) and received : %d bytes, ip : %d.%d.%d.%d\n", bytes_to_read_from_ch395, bytes_received, ch395->socket_dest_ip[socket][0], ch395->socket_dest_ip[socket][1],  ch395->socket_dest_ip[socket][2], ch395->socket_dest_ip[socket][3]);

    if (bytes_received < 0)
    {
        perror("Erreur lors de la réception des données");
        return 1;
    }

    // Set flag recv
    printf("[EMULATOR] Setting CH395_SINT_STAT_RECV into socket %d\n", socketid);
    dbg_printf("[EMULATOR] Setting CH395_SINT_STAT_RECV into socket %d\n", socketid);
    ch395->socket_int_status[socketid] |= CH395_SINT_STAT_RECV;
    // Store the number of bytes received
    ch395->buffer_position_receive[socketid] = bytes_received;
    return bytes_received;
}


int perform_connect_socket(struct ch395 *ch395, unsigned char socketid)
{
    char ip[16]; // 16 pour contenir l'IP au format "xxx.xxx.xxx.xxx\"
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = ch395->sockfd_host[socketid];

    // Configurer l'adresse du serveur
    server_addr.sin_family = AF_INET;

    int port = ch395->socket_dest_port[socketid][0] + ch395->socket_dest_port[socketid][1]*256;
    server_addr.sin_port = htons(port); // Port par défaut pour HTTP


    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", ch395->socket_dest_ip[socketid][0], ch395->socket_dest_ip[socketid][1], ch395->socket_dest_ip[socketid][2], ch395->socket_dest_ip[socketid][3]);
    printf("%s %u.%u.%u.%u\n",ip, ch395->socket_dest_ip[socketid][0], ch395->socket_dest_ip[socketid][1], ch395->socket_dest_ip[socketid][2], ch395->socket_dest_ip[socketid][3]);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
    {
        perror("invalid IP adress ");
        return 1;
    }
    printf("Connection to : %s:%d\n",ip, port);
    dbg_printf("Connection to : %s:%d\n",ip, port);


    //Se connecter au serveur
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Erreur lors de la connexion au serveur");
        printf("erreur de la Connexion vers : %s:%d\n", ip, port);
        dbg_printf("erreur de la  Connexion vers : %s:%d\n", ip, port);
        return 1;
    }

    printf("la Connexion semble établie vers : %s:%d\n",ip, port);
    dbg_printf("la Connexion semble établie vers : %s:%d\n",ip, port);
    ch395->socket_status_sn[socketid][1] = CH395_TCP_ESTABLISHED;

    return 0;
}


void ch395_init_internal(struct ch395 *ch395)
{
    unsigned char i;
    // Global interrupt status
    //[Verified on real computer] If ch395 is not initialized (with init command) glob_int_status is set to 0 at boot
    ch395->glob_int_status = 0;
    //[Verified on real computer] If ch395 is not initialized (with init command) glob_int_status_all is set to 0 at boot
    ch395->glob_int_status_all[0] = 0;
    ch395->glob_int_status_all[1] = 0;
    for(i=0; i<8; i++)
    {
        //ch395->socket_state[i] = CH395_SOCKET_CLOSED; // Socket state FIXME doublon
        //ch395->socket_proto[i] = CH395_TCP_CLOSED; // State protocol
        ch395->socket_status_sn[i][0] = CH395_SOCKET_CLOSED; // State protocol
        ch395->socket_status_sn[i][1] = CH395_TCP_CLOSED; // State protocol
        ch395->socket_int_status[i] = CH395_SINT_STAT_DISCONNECT; //Socket state
        ch395->buffer_position_write_from_data[i] = 0;
        ch395->buffer_position_read_from_data[i] = 0;
        ch395->socket_length_received[i] = 0;
        ch395->socket_length_to_send[i] = 0;
    }

    // Define buffers for 4 sockets, others sockets are not allocated at the start of ch395. It must be user allocated

    // Init socket from 4 to 7
    for(i=4; i<8; i++)
    {
        ch395->receive_buffer_start_block[i] = 0; // Should be NULL
        ch395->transmit_buffer_number_of_block[i] = 0;
        ch395->receive_buffer_number_of_block[i] = 0;
        ch395->transmit_buffer_start_block[i] = 0; // Should be NULL
    }

    for(i=0; i<4; i++)
    {
        ch395->receive_buffer_number_of_block[i] = 8;
        ch395->transmit_buffer_number_of_block[i] = 4;

    }

    ch395->receive_buffer_start_block[0] = 0;
    ch395->receive_buffer_start_block[1] = 12;
    ch395->receive_buffer_start_block[2] = 24;
    ch395->receive_buffer_start_block[3] = 36;

    ch395->transmit_buffer_start_block[0] = 8;
    ch395->transmit_buffer_start_block[1] = 20;
    ch395->transmit_buffer_start_block[2] = 32;
    ch395->transmit_buffer_start_block[3] = 44;

    // For all socket
    for(i=0; i<8; i++)
    {
        // Set socket
        ch395->socket_ttl[i] = 0; // Set TTL : FIXME Check on real chip the value of TTL
    }

}


unsigned int ch395_create(struct machine *oric)
{
    int i;
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return 0;

    userdata[plugin_instances] = malloc(sizeof(struct ch395));
    userdata_old[plugin_instances] = malloc(sizeof(struct ch395));

    if (userdata[plugin_instances])
    {
        if (userdata_old[plugin_instances] == NULL)
        {
            free(userdata[plugin_instances]);
            return 0;
        }
        // ch395 is not initialized
        userdata[plugin_instances]->is_init = CH395_FALSE;
        userdata[plugin_instances]->phy_state = CH395_PHY_DISCONN;
        // Mac address can be accessed even if ch395 is not init
        userdata[plugin_instances]->mac_address[5] = 0xaa;
        userdata[plugin_instances]->mac_address[4] = 0xbb;
        userdata[plugin_instances]->mac_address[3] = 0xcc;
        userdata[plugin_instances]->mac_address[2] = 0xdd;
        userdata[plugin_instances]->mac_address[1] = 0xee;
        userdata[plugin_instances]->mac_address[0] = 0xff;

        for (i = 0; i< 20 ; i++) {
            userdata[plugin_instances]->ip_chip[i] = 0;
        }
        ch395_init_internal(userdata[plugin_instances]);


        return ++plugin_instances;
    }

    return 0;
}


unsigned char ch395_read_command_port(struct ch395 *ch395, SDL_bool run)
{
    ch395 = ch395;
    if (!run)
        // Don't know if ch395 return command port
        return 0;
    ch395_debug_concat(">>[CH395][READ][COMMAND] Can not read command port\n");
    return 0;
}

unsigned char ch395_read_data_port(struct ch395 *ch395, SDL_bool run)
{
    unsigned char data = 0xff;
    char msg[500];
    char *value;
    unsigned char socket;

    value = malloc(200);
    run = run;

    switch(ch395->command)
    {
        case CH395_CMD_CHECK_EXIST:
            data = ch395->cmd_data.CMD_CheckByte;
            dbg_printf("[CH395][READ][DATA][CH395_CMD_CHECK_EXIST] setting data port to 0x%02x data = %d\n", ch395->cmd_data.CMD_CheckByte; data);
            printf("[CH395][READ][DATA][CH395_CMD_CHECK_EXIST] setting data port to 0x%02x data = %d\n", ch395->cmd_data.CMD_CheckByte, data);
            return data;

         case CH395_CMD_GET_IC_VER:
            ch395_debug_concat("<<[CH395][READ][DATA][CH395_CMD_GET_IC_VER]\n");
            data = 70; //VERSION
            break;

        case CH395_CMD_GET_PHY_STATUS:
            ch395_debug_concat("<<[CH395][READ][DATA][CH395_CMD_GET_PHY_STATUS]");
            // If ch395 is not init, PHY_state is always disconnected
            if (ch395->is_init == CH395_FALSE)
            {
                printf("ch395 not init Cable disconnected\n");
                dbg_printf("ch395 not init Cable disconnected\n");
                data = CH395_PHY_DISCONN;
            }
            else
            {
                ch395->phy_state = CH395_PHY_100M_FLL;
                printf("Return val %x Cable connected\n", ch395->phy_state);
                dbg_printf("Return val %d Cable connected\n", ch395->phy_state);
                data = ch395->phy_state;
                // The next call, we set CH395_PHY_100M_FLL. FIXME : in order to be correct, it should test network host stack
                // Depending of the network when the oric is connected, this value should not be this, but we set this now

            }
            break;

        case CH395_CMD_GET_IP_INF:
            ch395_debug_concat(">>[CH395][READ][DATA][CH395_CMD_GET_IP_INF]");
            if (ch395->pos_rw_in_cmd_data  == 20)
            {
                ch395_debug_concat("CH395 PANIC impossible to read more than 4 bytes\n");
            }
            else
            {
                data = ch395->ip_chip[ch395->pos_rw_in_cmd_data];
                ch395->pos_rw_in_cmd_data ++;
                printf(" send : %d\n",data);
            }
            break;

        case CH395_CMD_READ_RECV_BUF_SN:
            ch395_debug_concat("<<[CH395][READ][DATA][CH395_CMD_READ_RECV_BUF_SN] ");
            int current_socket = ch395->cmd_data.CMD_SocketGetRecvBuf[0];
            int pos_buffer = ch395->pos_rw_in_cmd_data + ch395->transmit_buffer_start_block[current_socket] * CH395_SIZE_BLOCK_BUFFER;
            printf("POS buffer : %d ", pos_buffer);

            data = ch395->buffer[pos_buffer]; // FIXME
            printf("Send byte : %d ",ch395->pos_rw_in_cmd_data);
            dbg_printf("Send byte : %d ",ch395->pos_rw_in_cmd_data);
            switch (data)
            {
                case 0x0a:
                    printf("data : \\n\n");
                    dbg_printf("data : \\n\n");
                    break;

                case 0x0d:
                    printf("data : \\r\n");
                    dbg_printf("data : \\r\n");
                    break;

                default:
                    printf("data : %c\n", data);
                    dbg_printf("data : %c\n", data);
                    break;
            }

            ch395->pos_rw_in_cmd_data ++;
            break;

        case CH395_CMD_GET_MAC_ADDR:
            if (ch395->nb_bytes_in_cmd_data == 6)
            {
                ch395_debug_concat("CH395 panic : impossible to read mac adress more than 6 bytes");
                data = 0;
            }
            else
            {
                data = ch395->mac_address[ch395->nb_bytes_in_cmd_data];
                ch395->nb_bytes_in_cmd_data++;
            }
            break;

        case CH395_CMD_GET_RECV_LEN_SN:
            ch395_debug_concat("<<[CH395][READ][DATA][CH395_CMD_GET_RECV_LEN_SN]");

            dbg_printf("%s", msg);
            if ( ch395->pos_rw_in_cmd_data == 0 )
            {
                data = ch395->buffer_position_receive[ch395->cmd_data.CMD_SocketGetRecvLen[0]] & 0xFF;
                printf("Receiving length low %d\n", data);
                dbg_printf("Receiving length low %d\n", data);
            }

            if ( ch395->pos_rw_in_cmd_data == 1 )
            {
                data = ch395->buffer_position_receive[ch395->cmd_data.CMD_SocketGetRecvLen[0]] >> 8;
                printf("Receiving length high %d\n", data);
                dbg_printf("Receiving length high %d\n", data);
            }

            ch395->pos_rw_in_cmd_data ++;
            break;

        case CH395_CMD_GET_SOCKET_STATUS_SN:
            ch395_debug_concat("<<[CH395][READ][DATA][CH395_CMD_GET_SOCKET_STATUS_SN]");

            if ( ch395->pos_rw_in_cmd_data == 0 )
            {
                data = ch395->socket_status_sn[ch395->cmd_data.CMD_SocketState[0]][0];
                switch (data)
                {
                    case CH395_SOCKET_CLOSED:
                        strcpy(msg," SOCKET_CLOSED");
                        break;

                    case CH395_SOCKET_OPEN:
                        strcpy(msg," CH395_SOCKET_OPEN");
                        break;

                    default:
                        strcpy(msg," Unknown socket state");
                        break;
                }

                printf("Socket: %d socket State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
                dbg_printf("Socket: %d socket State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
            }

            if (ch395->pos_rw_in_cmd_data == 1)
            {
                data = ch395->socket_status_sn[ch395->cmd_data.CMD_SocketState[0]][1];
                switch(data)
                {
                    case CH395_TCP_CLOSED:
                        strcpy(msg," TCP_CLOSED");
                        break;

                    case CH395_TCP_LISTEN:
                        strcpy(msg," TCP_LISTEN");
                        break;

                    case CH395_TCP_SYN_SENT:
                        strcpy(msg," TCP_SYN_SENT");
                        break;

                    case CH395_TCP_SYN_REVD:
                        strcpy(msg," TCP_SYN_REVD");
                        break;

                    case CH395_TCP_ESTABLISHED:
                        strcpy(msg," TCP_ESTABLISHED");
                        break;

                    case CH395_TCP_FIN_WAIT_1:
                        strcpy(msg," TCP_FIN_WAIT_1");
                        break;

                    case CH395_TCP_FIN_WAIT_2:
                        strcpy(msg," TCP_FIN_WAIT_2");
                        break;

                    case CH395_TCP_CLOSE_WAIT:
                        strcpy(msg," CLOSE_WAIT");
                        break;

                    case CH395_TCP_CLOSING:
                        strcpy(msg," TCP_CLOSING");
                        break;

                    case CH395_TCP_LAST_ACK:
                        strcpy(msg," LAST_ACK");
                        break;

                    case CH395_TCP_TIME_WAIT:
                        strcpy(msg," TCP_TIME_WAIT");
                        break;

                    default:
                        strcpy(msg," Unknown protocol state");
                        break;
                }
                printf("Socket: %d socket protocol State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
                dbg_printf("Socket: %d socket protocol State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
            }


            ch395->pos_rw_in_cmd_data ++;
            break;

        case CH395_CMD_GET_GLOB_INT_STATUS:
            /*
            7 GINT_STAT_SOCK3 Socket3 interrupt
            6 GINT_STAT_SOCK2 Socket2 interrupt
            5 GINT_STAT_SOCK1 Socket1 interrupt
            4 GINT_STAT_SOCK0 Socket0 interrupt
            3 GINT_STAT_DHCP DHCP interrupt
            2 GINT_STAT_PHY_CHANGE PHY status change interrupt
            1 GINT_STAT_IP_CONFLI IP conflict
            0 GINT_STAT_UNREACH Inaccessible interrupt
            */
            ch395_debug_concat("<<[CH395][READ][DATA][CH395_CMD_GET_GLOB_INT_STATUS]");


            data = ch395->glob_int_status;

            if (ch395->glob_int_status & CH395_GINT_STAT_SOCK3)
            {
                ch395_debug_concat("SOCK3 interrupt detected, clear sock3 interrupt now");
                ch395->glob_int_status &= ~CH395_GINT_STAT_SOCK3;
            }

            if (ch395->glob_int_status & CH395_GINT_STAT_SOCK2)
            {
                ch395_debug_concat("SOCK2 interrupt detected, clear sock2 interrupt now");
                ch395->glob_int_status &= ~CH395_GINT_STAT_SOCK2;
            }

            if (ch395->glob_int_status & CH395_GINT_STAT_SOCK1)
            {
                ch395_debug_concat("SOCK1 interrupt detected, clear sock1 interrupt now");
                ch395->glob_int_status &= ~CH395_GINT_STAT_SOCK1;
            }

            if (ch395->glob_int_status & CH395_GINT_STAT_SOCK0)
            {
                ch395_debug_concat("SOCK0 interrupt detected, clear sock0 interrupt now");
                ch395->glob_int_status &= ~CH395_GINT_STAT_SOCK0;
            }

            if (ch395->glob_int_status & CH395_GINT_STAT_DHCP)
            {
                ch395_debug_concat("DHCP interrupt detected, clear DHCP interrupt now");
                ch395->glob_int_status &= ~CH395_GINT_STAT_DHCP;
            }

            if (ch395->glob_int_status & CH395_GINT_STAT_PHY_CHANGE)
            {
                ch395_debug_concat("PHY_CHANGE interrupt detected, clear PHY_CHANGE interrupt now");
                ch395->glob_int_status &= ~CH395_GINT_STAT_PHY_CHANGE;
            }

            if (ch395->glob_int_status & CH395_GINT_STAT_IP_CONFLI)
            {
                ch395_debug_concat("IP_CONFLI interrupt detected, clear IP_CONFLI interrupt now");
                ch395->glob_int_status &= ~CH395_GINT_STAT_IP_CONFLI;
            }

            if (ch395->glob_int_status & CH395_GINT_STAT_UNREACH)
            {
                ch395_debug_concat("UNREACH interrupt detected, clear UNREACH interrupt now");
                ch395->glob_int_status &= ~CH395_GINT_STAT_UNREACH;
            }

            ch395_debug_concat("\n");
            break;

        case CH395_CMD_GET_INT_STATUS_SN:
            socket = ch395->cmd_data.CMD_SocketGetIntStatusSn[0];
            data = ch395->socket_int_status[socket];
            // When a socket is not open CH395_CMD_GET_INT_STATUS_SN command returns always 0 for socket state
            // Clear status
            // Each time CH395_CMD_GET_INT_STATUS_SN is read, we reset necessary status


            if (ch395->socket_int_status[socket] & CH395_SINT_STAT_SEND_OK)
            {
                // Remettre le bit CH395_SINT_STAT_SEND_OK à 0
                ch395->socket_int_status[socket] &= ~CH395_SINT_STAT_SEND_OK;
            }

            if (ch395->socket_int_status[socket] & CH395_SINT_STAT_RECV)
            {
                // Remettre le bit CH395_SINT_STAT_RECV à 0
                ch395->socket_int_status[socket] &= ~CH395_SINT_STAT_RECV;
            }

            if (ch395->socket_int_status[socket] & CH395_SINT_STAT_SENBUF_FREE)
            {
                // Remettre le bit CH395_SINT_STAT_SENBUF_FREE à 0
                ch395->socket_int_status[socket] &= ~CH395_SINT_STAT_SENBUF_FREE;
                perform_recv_socket(ch395, socket);
            }


            sprintf(value, "INT_STATUS_SN states : %d=", data);
            strcpy(msg,"->");
            strcat(msg,value);

            if (data & CH395_SINT_STAT_TIM_OUT)
            {
                strcat(msg," CH395_SINT_STAT_TIM_OUT");
            }

            if (data & CH395_SINT_STAT_DISCONNECT)
            {
                strcat(msg," CH395_SINT_STAT_DISCONNECT");
            }

            if (data & CH395_SINT_STAT_CONNECT)
            {
                strcat(msg," CH395_SINT_STAT_CONNECT");
            }

            if (data & CH395_SINT_STAT_RECV)
            {
                strcat(msg," CH395_SINT_STAT_RECV");
            }

            if (data & CH395_SINT_STAT_SEND_OK)
            {
                strcat(msg," CH395_SINT_STAT_SEND_OK");
                strcat(msg," => CH395_SINT_STAT_SEND_OK state cleared, setting CH395_SINT_STAT_RECV now");
                //

                switch(socket)
                {
                    case 0:
                        ch395->glob_int_status |= CH395_GINT_STAT_SOCK0;
                        break;

                    case 1:
                        ch395->glob_int_status |= CH395_GINT_STAT_SOCK1;
                        break;

                    case 2:
                        ch395->glob_int_status |= CH395_GINT_STAT_SOCK2;
                        break;

                    case 3:
                        ch395->glob_int_status |= CH395_GINT_STAT_SOCK3;
                        break;
                }
            }

            if (data & CH395_SINT_STAT_SENBUF_FREE)
            {
                strcat(msg," CH395_SINT_STAT_SENBUF_FREE cleared");
            }

            printf("<<[CH395][READ][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d %s\n", socket, msg);
            dbg_printf("<<[CH395][READ][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d %s\n", socket, msg);
            //ch395->cmd_data.CMD_SocketGetIntStatusSn[0] = 0; // Reset status
            break;
    }
    free(value);
    return data;
}

void ch395_write_command_port(struct ch395 *ch395, uint8_t command)
{
    // FIXME test nb_bytes_in_cmd_data in order to see if there is partial data sent into buffer
    int val;
    ch395->nb_bytes_in_cmd_data = 0;
    ch395->pos_rw_in_cmd_data = 0;
    switch(command)
    {
        case CH395_CMD_GET_IC_VER:
            ch395->command = CH395_CMD_GET_IC_VER;
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_IC_VER]\n");
            break;

        case CH395_CMD_CHECK_EXIST:
            ch395->command = CH395_CMD_CHECK_EXIST;
            ch395_debug_concat("[CH395][WRITE][COMMAND][CH395_CMD_CHECK_EXIST] waiting for check byte\n");
            break;

        case CH395_CMD_SET_BAUDRATE:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_BAUDRATE] not emulated\n");
            ch395->command = CH395_CMD_SET_BAUDRATE;
            break;

        case CH395_CMD_ENTER_SLEEP:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_ENTER_SLEEP] not emulated\n");
            ch395->command = CH395_CMD_ENTER_SLEEP;
            break;

        case CH395_CMD_RESET_ALL:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_RESET_ALL]\n");
            ch395->command = CH395_CMD_RESET_ALL;
            break;

        case CH395_CMD_SET_PHY:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_PHY] not emulated\n");
            ch395->command = CH395_CMD_SET_PHY;
            break;

        case CH395_CMD_GET_GLOB_INT_STATUS_ALL:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_GLOB_INT_STATUS_ALL]\n");
            ch395->command = CH395_CMD_GET_GLOB_INT_STATUS_ALL;
            break;

        case CH395_CMD_SET_MAC_ADDR:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_MAC_ADDR] not emulated\n");
            ch395->command = CH395_CMD_SET_MAC_ADDR;
            break;

        case CH395_CMD_SET_IP_ADDR:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_IP_ADDR] not emulated\n");
            ch395->command = CH395_CMD_SET_IP_ADDR;
            break;

        case CH395_CMD_SET_GWIP_ADDR:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_GWIP_ADDR] not emulated\n");
            ch395->command = CH395_CMD_SET_GWIP_ADDR;
            break;

        case CH395_CMD_SET_MASK_ADDR:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_MASK_ADDR] not emulated\n");
            ch395->command = CH395_CMD_SET_MASK_ADDR;
            break;

        case CH395_CMD_SET_MAC_FILT:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_MAC_FILT] not emulated\n");
            ch395->command = CH395_CMD_SET_MAC_FILT;
            break;

        case CH395_CMD_GET_PHY_STATUS:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_PHY_STATUS]\n");
            ch395->command = CH395_CMD_GET_PHY_STATUS;
            break;

        case CH395_CMD_INIT:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_INIT]\n");
            ch395->command = CH395_CMD_INIT;
            ch395->is_init = CH395_TRUE;
            break;

        case CH395_CMD_GET_UNREACH_IPPORT:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_UNREACH_IPPORT] Command not supported in current emulation\n");
            ch395->command = CH395_CMD_GET_UNREACH_IPPORT;
            break;

        case CH395_CMD_GET_GLOB_INT_STATUS:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_GLOB_INT_STATUS]\n");
            ch395->command = CH395_CMD_GET_GLOB_INT_STATUS;
            break;

        case CH395_CMD_SET_RETRAN_COUNT:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_RETRAN_COUNT] not emulated\n");
            ch395->command = CH395_CMD_SET_RETRAN_COUNT;
            break;

        case CH395_CMD_SET_RETRAN_PERIOD:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_RETRAN_PERIOD] not emulated\n");
            ch395->command = CH395_CMD_SET_RETRAN_PERIOD;
            break;

        case CH395_CMD_GET_CMD_STATUS:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_CMD_STATUS]\n");
            ch395->command = CH395_CMD_GET_CMD_STATUS;
            break;

        case CH395_CMD_GET_REMOT_IPP_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_REMOT_IPP_SN] not emulated\n");
            ch395->command = CH395_CMD_GET_REMOT_IPP_SN;
            break;

        case CH395_CMD_CLEAR_RECV_BUF_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_CLEAR_RECV_BUF_SN] not emulated\n");
            ch395->command = CH395_CMD_CLEAR_RECV_BUF_SN;
            break;

        case CH395_CMD_GET_SOCKET_STATUS_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_SOCKET_STATUS_SN]\n");
            ch395->command = CH395_CMD_GET_SOCKET_STATUS_SN;
            break;

        case CH395_CMD_GET_INT_STATUS_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_INT_STATUS_SN]\n");
            ch395->command = CH395_CMD_GET_INT_STATUS_SN;
            break;

        case CH395_CMD_SET_IP_ADDR_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_IP_ADDR_SN]\n");
            ch395->command = CH395_CMD_SET_IP_ADDR_SN;
            break;

        case CH395_CMD_SET_DES_PORT_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_DES_PORT_SN]\n");
            ch395->command = CH395_CMD_SET_DES_PORT_SN;
            break;

        case CH395_CMD_SET_SOUR_PORT_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_SOUR_PORT_SN]\n");
            ch395->command = CH395_CMD_SET_SOUR_PORT_SN;
            break;

        case CH395_CMD_SET_PROTO_TYPE_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_PROTO_TYPE_SN]\n");
            ch395->command = CH395_CMD_SET_PROTO_TYPE_SN;
            break;

        case CH395_CMD_OPEN_SOCKET_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_OPEN_SOCKET_SN]\n");
            ch395->command = CH395_CMD_OPEN_SOCKET_SN;
            break;

        case CH395_CMD_TCP_LISTEN_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_TCP_LISTEN_SN]\n");
            ch395->command = CH395_CMD_TCP_LISTEN_SN;
            break;

        case CH395_CMD_TCP_CONNECT_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_TCP_CONNECT_SN]\n");
            ch395->command = CH395_CMD_TCP_CONNECT_SN;
            break;

        case CH395_CMD_TCP_DISNCONNECT_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_TCP_DISNCONNECT_SN]\n");
            ch395->command = CH395_CMD_TCP_DISNCONNECT_SN;
            break;

        case CH395_CMD_WRITE_SEND_BUF_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_WRITE_SEND_BUF_SN]\n");
            ch395->command = CH395_CMD_WRITE_SEND_BUF_SN;
            break;

        case CH395_CMD_GET_RECV_LEN_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_RECV_LEN_SN]\n");
            ch395->command = CH395_CMD_GET_RECV_LEN_SN;
            break;

        case CH395_CMD_READ_RECV_BUF_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_READ_RECV_BUF_SN]\n");
            ch395->command = CH395_CMD_READ_RECV_BUF_SN;
            break;

        case CH395_CMD_CLOSE_SOCKET_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_CLOSE_SOCKET_SN]\n");
            ch395->command = CH395_CMD_CLOSE_SOCKET_SN;
            break;

        case CH395_CMD_SET_IPRAW_PRO_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_IPRAW_PRO_SN]\n");
            ch395->command = CH395_CMD_SET_IPRAW_PRO_SN;
            break;

        case CH395_CMD_PING_ENABLE:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_PING_ENABLE]\n");
            ch395->command = CH395_CMD_PING_ENABLE;
            break;

        case CH395_CMD_GET_MAC_ADDR:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_MAC_ADDR]\n");
            ch395->command = CH395_CMD_GET_MAC_ADDR;
            break;

        case CH395_CMD_DHCP_ENABLE:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_DHCP_ENABLE]\n");
            ch395->command = CH395_CMD_DHCP_ENABLE;

            val = ch395_fill_get_ip_inf_linux(ch395);
            if (val == 0 ) ch395->cmd_status = ch395->cmd_status | CH395_ERR_SUCCESS;

            // Let's get IP and so on
            break;

        case CH395_CMD_GET_DHCP_STATUS:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_DHCP_STATUS]\n");
            ch395->command = CH395_CMD_GET_DHCP_STATUS;
            break;

        case CH395_CMD_GET_IP_INF:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_IP_INF]\n");
            ch395->command = CH395_CMD_GET_IP_INF;
            break;

        case CH395_CMD_PPPOE_SET_USER_NAME:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_PPPOE_SET_USER_NAME]\n");
            ch395->command = CH395_CMD_PPPOE_SET_USER_NAME;
            break;

        case CH395_CMD_PPPOE_SET_PASSWORD:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_PPPOE_SET_PASSWORD]\n");
            ch395->command = CH395_CMD_PPPOE_SET_PASSWORD;
            break;

        case CH395_CMD_PPPOE_ENABLE:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_PPPOE_ENABLE]\n");
            ch395->command = CH395_CMD_PPPOE_ENABLE;
            break;

        case CH395_CMD_GET_PPPOE_STATUS:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_PPPOE_STATUS]\n");
            ch395->command = CH395_CMD_GET_PPPOE_STATUS;
            break;

        case CH395_CMD_SET_TCP_MSS:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_TCP_MSS]\n");
            ch395->command = CH395_CMD_SET_TCP_MSS;
            break;

        case CH395_CMD_SET_TTL:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_TTL]\n");
            ch395->command = CH395_CMD_SET_TTL;
            break;

        case CH395_CMD_SET_RECV_BUF:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_RECV_BUF]\n");
            ch395->command = CH395_CMD_SET_RECV_BUF;
            break;

        case CH395_CMD_SET_SEND_BUF:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_SEND_BUF]\n");
            ch395->command = CH395_CMD_SET_SEND_BUF;
            break;

        case CH395_CMD_SET_FUN_PARA:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_FUN_PARA] not emulated\n");
            ch395->command = CH395_CMD_SET_FUN_PARA;
            break;

        case CH395_CMD_SET_KEEP_LIVE_IDLE:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_KEEP_LIVE_IDLE] Not emulated\n");
            ch395->command = CH395_CMD_SET_KEEP_LIVE_IDLE;
            break;

        case CH395_CMD_SET_KEEP_LIVE_INTVL:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_KEEP_LIVE_INTVL] Not emulated\n");
            ch395->command = CH395_CMD_SET_KEEP_LIVE_INTVL;
            break;

        case CH395_CMD_SET_KEEP_LIVE_CNT:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_KEEP_LIVE_CNT] Not emulated\n");
            ch395->command = CH395_CMD_SET_KEEP_LIVE_CNT;
            break;

        case CH395_CMD_SET_KEEP_LIVE_SN:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_KEEP_LIVE_SN] Not emulated\n");
            ch395->command = CH395_CMD_SET_KEEP_LIVE_SN;
            break;

        case CH395_CMD_EEPROM_ERASE:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_EEPROM_ERASE] Not emulated\n");
            ch395->command = CH395_CMD_EEPROM_ERASE;
            break;

        case CH395_CMD_EEPROM_WRITE:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_EEPROM_WRITE] Not emulated\n");
            ch395->command = CH395_CMD_EEPROM_WRITE;
            break;

        case CH395_CMD_EEPROM_READ:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_EEPROM_READ] Not emulated\n");
            ch395->command = CH395_CMD_EEPROM_READ;
            break;

        case CH395_CMD_READ_GPIO_REG:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_READ_GPIO_REG] Not emulated\n");
            ch395->command = CH395_CMD_READ_GPIO_REG;
            break;

        case CH395_CMD_WRITE_GPIO_REG:
            ch395_debug_concat(">>[CH395][WRITE][COMMAND][CH395_CMD_WRITE_GPIO_REG] Not emulated\n");
            ch395->command = CH395_CMD_WRITE_GPIO_REG;
            break;

        default:
            printf(">>[CH395][UNKNOWN_COMMAND]\n");
            break;
    }
}

int ch395_write_data_port(struct ch395 *ch395, uint8_t data)
{

    switch(ch395->command)
    {
        case CH395_CMD_GET_IC_VER:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_IC_VER]\n");
            break;

        case CH395_CMD_CHECK_EXIST:
            ch395->cmd_data.CMD_CheckByte = ~data;
            printf(">>[CH395][WRITE][DATA][CH395_CMD_CHECK_EXIST] check byte received : 0x%02x convert : 0x%02x\n",data, ch395->cmd_data.CMD_CheckByte);
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_CHECK_EXIST] check byte received : 0x%02x convert : 0x%02x\n",data, ch395->cmd_data.CMD_CheckByte);
            break;

        case CH395_CMD_SET_BAUDRATE:
            break;

        case CH395_CMD_ENTER_SLEEP:
            break;

        case CH395_CMD_RESET_ALL:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_RESET_ALL][ERROR] RESET_ALL can not accept any data on data port!\n");
            break;

        case CH395_CMD_SET_PHY:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_PHY][ERROR] Not emulated!\n");
            break;

        case CH395_CMD_GET_GLOB_INT_STATUS_ALL:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_GLOB_INT_STATUS_ALL] Socket : %d", data);
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_GLOB_INT_STATUS_ALL] Socket : %d", data);
            break;

        case CH395_CMD_SET_MAC_ADDR:
            break;

        case CH395_CMD_SET_IP_ADDR:
            break;

        case CH395_CMD_SET_GWIP_ADDR:
            break;

        case CH395_CMD_SET_MASK_ADDR :
            break;

        case CH395_CMD_SET_MAC_FILT:
            break;

        case CH395_CMD_GET_PHY_STATUS:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_PHY_STATUS] can not accept any data on data port!\n");
            break;

        case CH395_CMD_INIT:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_INIT][ERROR] INIT can not accept any data on data port!\n");
            break;

        case CH395_CMD_GET_UNREACH_IPPORT:
            break;

        case CH395_CMD_GET_GLOB_INT_STATUS:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_GLOB_INT_STATUS]\n");
            break;

        case CH395_CMD_SET_RETRAN_COUNT:
            break;

        case CH395_CMD_SET_RETRAN_PERIOD:
            break;

        case CH395_CMD_GET_CMD_STATUS:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_CMD_STATUS]\n");
            break;

        case CH395_CMD_GET_REMOT_IPP_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_REMOT_IPP_SN]\n");
            break;

        case CH395_CMD_CLEAR_RECV_BUF_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_CLEAR_RECV_BUF_SN]\n");
            break;

        case CH395_CMD_GET_SOCKET_STATUS_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_SOCKET_STATUS_SN]");


            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                if (ch395_check_socket(data, ch395) == 1) return 1;
                ch395->cmd_data.CMD_SocketState[0] = data;
                printf(" Socket : %d\n", data);
                dbg_printf(" Socket : %d\n", data);
            }
            else
            {
                ch395_debug_concat("Error too much bytes into data port\n");

            }
            ch395->nb_bytes_in_cmd_data++;
            break;

        case CH395_CMD_GET_INT_STATUS_SN:

            if (ch395->nb_bytes_in_cmd_data == 1)
            {
                ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_INT_STATUS_SN][ERROR] Accept only one byte (data already sent)\n");
                break;
            }

            if (ch395_check_socket(data, ch395) == 1) return 1;

            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d\n", data);
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d\n", data);

            ch395->cmd_data.CMD_SocketGetIntStatusSn[0] = data;
            ch395->nb_bytes_in_cmd_data++;

            break;

        case CH395_CMD_SET_IP_ADDR_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_IP_ADDR_SN]");


            if (ch395->nb_bytes_in_cmd_data > 5)
            {
                ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_IP_ADDR][ERROR] Too much data");
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                if (ch395_check_socket(data, ch395) == 1) return 1;
                printf("Socket : %d\n",data);
                dbg_printf("Socket %d\n",data);
                // Set socket
                ch395->cmd_data.CMD_SocketSetIpAddr[0] = data;
            }
            else
            {
                printf("Setting IP : %d\n",data);
                dbg_printf("Setting IP : %d\n",data);
                // Store dest ip
                ch395->socket_dest_ip[ch395->cmd_data.CMD_SocketSetIpAddr[0]][ch395->nb_bytes_in_cmd_data-1] = data;
            }
            // For errors detection we increment to test
            ch395->nb_bytes_in_cmd_data++;
            break;

        case CH395_CMD_SET_DES_PORT_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_DES_PORT_SN]");

            if (ch395->nb_bytes_in_cmd_data > 2)
            {
                ch395_debug_concat("[ERROR] Too much data");
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                if (ch395_check_socket(data, ch395) == 1) return 1;

                printf("Socket : %d\n",data);
                dbg_printf("Socket %d\n",data);
                // Set socket
                ch395->cmd_data.CMD_SocketSetDesPort[0] = data;

            }
            else
            {
                printf("Dest port : %d\n", data);
                dbg_printf("Dest port  : %d\n", data);
                // Store dest ip
                ch395->socket_dest_port[ch395->cmd_data.CMD_SocketSetDesPort[0]][ch395->nb_bytes_in_cmd_data-1] = data;

            }
            // For errors detection we increment to test
            ch395->nb_bytes_in_cmd_data ++;

            break;

        case CH395_CMD_SET_SOUR_PORT_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_SOUR_PORT_SN]");

            if (ch395->nb_bytes_in_cmd_data > 2)
            {
                ch395_debug_concat("[ERROR] Too much data");
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                if (ch395_check_socket(data, ch395) == 1) return 1;
                printf("Socket : %d\n",data);
                dbg_printf("Socket %d\n",data);
                // Set socket
                ch395->cmd_data.CMD_SocketSetSrcPort[0] = data;
            }
            else
            {
                printf("Src port : %d\n",data);
                dbg_printf("Src port  : %d\n",data);
                // Store dest ip
                ch395->socket_src_port[ch395->cmd_data.CMD_SocketSetSrcPort[0]][ch395->nb_bytes_in_cmd_data-1] = data;

            }
            // For errors detection we increment to test
            ch395->nb_bytes_in_cmd_data ++;
            break;

        case CH395_CMD_SET_PROTO_TYPE_SN:
            if (ch395->nb_bytes_in_cmd_data > 1)
            {
                ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_PROTO_TYPE_SN][ERROR] Too much data");
                break;
            }
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_PROTO_TYPE_SN]");

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                if (ch395_check_socket(data, ch395) == 1) return 1;
                printf("Selected socket : %d\n", data);
                dbg_printf("Selected socket : %d\n", data);

                // Set socket
                ch395->cmd_data.CMD_SocketSetProto[0] = data;
            }

            if (ch395->nb_bytes_in_cmd_data == 1)
            {
                if (data == CH395_PROTO_TYPE_TCP)
                {
                    ch395_debug_concat("Setting CH395_PROTO_TYPE_TCP\n");
                    ch395->socket_proto[ch395->cmd_data.CMD_SocketSetProto[0]] = SOCK_STREAM;
                }

                if (data == CH395_PROTO_TYPE_UDP)
                {
                    ch395_debug_concat("Setting CH395_PROTO_TYPE_UDP\n");
                    ch395->socket_proto[ch395->cmd_data.CMD_SocketSetProto[0]] = SOCK_DGRAM;
                }

                if (data == CH395_PROTO_TYPE_MAC_RAW)
                {
                    ch395_debug_concat("Setting CH395_PROTO_TYPE_MAC_RAW\n");
                    ch395->socket_proto[ch395->cmd_data.CMD_SocketSetProto[0]] = SOCK_RAW;
                }

                if (data == CH395_PROTO_TYPE_IP_RAW)
                {
                    ch395_debug_concat("Setting CH395_PROTO_TYPE_IP_RAW\n");
                    ch395->socket_proto[ch395->cmd_data.CMD_SocketSetProto[0]] = SOCK_RAW;
                }
                // Store Proto into socket_proto

            }
            // For errors detection we increment to test
            ch395->nb_bytes_in_cmd_data++;
            break;

        case CH395_CMD_OPEN_SOCKET_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_OPEN_SOCKET_SN]");
            if (ch395->nb_bytes_in_cmd_data == 0)
            {   // opening socket
                if (ch395_check_socket(data, ch395) == 1) return 1;
                printf("Opening socket : %d proto : %d\n", data, ch395->socket_proto[data]);
                dbg_printf("Opening socket : %d proto : %d\n", data, ch395->socket_proto[data]);

                // Set socket
                ch395->socket_status_sn[data][0] = CH395_SOCKET_OPEN;
                ch395->sockfd_host[data] = socket(AF_INET, ch395->socket_proto[data], 0);

                if (ch395->sockfd_host[data] < 0)
                {
                    printf("Erreur lors de la création du socket %d", data);
                    dbg_printf("Erreur lors de la création du socket %d", data);
                    return 1;
                }

            }
            break;

        case CH395_CMD_TCP_LISTEN_SN:
            if (ch395->nb_bytes_in_cmd_data == 0)
            {   // opening socket
                if (ch395_check_socket(data, ch395) == 1) return 1;
                ch395->socket_proto[ch395->cmd_data.CMD_SocketTCPListenSn[0]] = data;
                printf(">>[CH395][WRITE][DATA][CH395_CMD_TCP_LISTEN_SN] Selected socket : %d\n", data);
                dbg_printf("[CH395][WRITE][DATA][CH395_CMD_TCP_LISTEN_SN] Selected socket : %d\n", data);
            }
            else
            {
                ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_TCP_LISTEN_SN] Error two much bytes");
            }

            ch395->nb_bytes_in_cmd_data ++;
            break;

        case CH395_CMD_TCP_CONNECT_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_TCP_CONNECT_SN]");

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                if (ch395_check_socket(data, ch395) == 1) return 1;
                ch395->cmd_data.CMD_SocketState[0] = data;
                printf("Connecting socket %d ...\n", data);
                dbg_printf(" Connecting socket %d ...\n", data);
            }
            else
            {
                ch395_debug_concat("Error too much bytes into data port\n");
            }

            ch395->nb_bytes_in_cmd_data ++;
            int connect_error = perform_connect_socket(ch395, ch395->cmd_data.CMD_SocketWriteBuffer[0]);
            printf("Launching connect from socket %d\n", ch395->cmd_data.CMD_SocketWriteBuffer[0]);
            dbg_printf("Launching connect from socket %d\n", ch395->cmd_data.CMD_SocketWriteBuffer[0]);
            // Connect is OK, set CH395_SINT_STAT_CONNECT
            if (connect_error == 0)
                ch395->socket_int_status[ch395->cmd_data.CMD_SocketState[0]] = CH395_SINT_STAT_CONNECT;
            break;

        case CH395_CMD_TCP_DISNCONNECT_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_TCP_DISNCONNECT_SN] NOT EMULATED\n");
            break;

        case CH395_CMD_WRITE_SEND_BUF_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_WRITE_SEND_BUF_SN]");
            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                if (ch395_check_socket(data, ch395) == 1) return 1;
                ch395->cmd_data.CMD_SocketWriteBuffer[0] = data;
                printf("Socket : %d\n", data);
                dbg_printf("Socket : %d\n", data);
                ch395->nb_bytes_in_cmd_data ++;
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 1)
            {
                printf("length data low : %d\n", data);
                dbg_printf("length data low : %d\n", data);
                ch395->socket_length_to_send[ch395->cmd_data.CMD_SocketWriteBuffer[0]] = data;
                ch395->nb_bytes_in_cmd_data ++;
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 2)
            {
                printf("length data high : %d\n", data);
                dbg_printf("length data high : %d\n", data);
                ch395->socket_length_to_send[ch395->cmd_data.CMD_SocketWriteBuffer[0]] += data*256;
                ch395->nb_bytes_in_cmd_data ++;
                break;
            }
            // Launch socket (for instance stub)
            ch395->buffer[ch395->transmit_buffer_start_block[ch395->cmd_data.CMD_SocketWriteBuffer[0]] * CH395_SIZE_BLOCK_BUFFER + ch395->nb_bytes_in_cmd_data - 3] = data;

            if (data == 13)
                {
                printf("value : %d/char 13\n", data);
                dbg_printf("value : %d/char 13\n", data);
                }
            else
            {
                printf("value : %d/char %c\n", data, data);
                dbg_printf("value : %d/char %c\n", data, data);
            }

            ch395->nb_bytes_in_cmd_data++;
            if (ch395->nb_bytes_in_cmd_data == ch395->socket_length_to_send[ch395->cmd_data.CMD_SocketWriteBuffer[0]] + 3 )
            {
                // Send transmit buffer
                // Get the position of the buffer
                void *p = &ch395->buffer[ch395->transmit_buffer_start_block[ch395->cmd_data.CMD_SocketWriteBuffer[0]]*CH395_SIZE_BLOCK_BUFFER];



                if (ch395->socket_proto[ch395->cmd_data.CMD_SocketWriteBuffer[0]] == SOCK_STREAM)
                {
                    // TCP !!! Send buffer (p is the adress of the position of transmit_buffer)
                    if (send(ch395->sockfd_host[ch395->cmd_data.CMD_SocketWriteBuffer[0]], p, ch395->socket_length_to_send[ch395->cmd_data.CMD_SocketWriteBuffer[0]], 0) < 0)
                    {
                        perror("Erreur lors de l'envoi de la requête");
                        return 1;
                    }
                }

                else if (ch395->socket_proto[ch395->cmd_data.CMD_SocketWriteBuffer[0]] == SOCK_DGRAM)
                {
                    char ip[16]; // 16 pour contenir l'IP au format "xxx.xxx.xxx.xxx\"
                    struct sockaddr_in server_addr;
                    int socketid = ch395->cmd_data.CMD_SocketWriteBuffer[0];


                    // Configurer l'adresse du serveur
                    server_addr.sin_family = AF_INET;

                    int port = ch395->socket_dest_port[socketid][0] + ch395->socket_dest_port[socketid][1]*256;
                    server_addr.sin_port = htons(port); // Port par défaut pour HTTP

                    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", ch395->socket_dest_ip[socketid][0], ch395->socket_dest_ip[socketid][1], ch395->socket_dest_ip[socketid][2], ch395->socket_dest_ip[socketid][3]);
                    printf("%s %u.%u.%u.%u\n",ip, ch395->socket_dest_ip[socketid][0], ch395->socket_dest_ip[socketid][1], ch395->socket_dest_ip[socketid][2], ch395->socket_dest_ip[socketid][3]);
                    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
                    {
                        perror("invalid IP adress ");
                        return 1;
                    }

                    printf("[EMULATOR][UDP] Connection to : %s:%d with sockfd_host : %d\n",ip, port, ch395->sockfd_host[socketid]);
                    dbg_printf("[EMULATOR][UDP] Connection to : %s:%d with sockfd_host : %d\n",ip, port, ch395->sockfd_host[socketid]);


                    // UDP !!! Send buffer (p is the adress of the position of transmit_buffer)
                    if (sendto(
                        ch395->sockfd_host[socketid], // Sock
                        p, // Buffer to send
                        ch395->socket_length_to_send[ch395->cmd_data.CMD_SocketWriteBuffer[0]], // Length to send
                        0,
                        (struct sockaddr *)&server_addr, // Adress of the server
                        sizeof(server_addr) // Sizeof
                    ) < 0)
                    {
                        perror("Erreur lors de l'envoi de la requête (UDP)");
                        return 1;
                    }
                }
                // Set SINT_STAT_SEND_OK
                ch395->socket_int_status[ch395->cmd_data.CMD_SocketWriteBuffer[0]] = ch395->socket_int_status[ch395->cmd_data.CMD_SocketWriteBuffer[0]]  | CH395_SINT_STAT_SENBUF_FREE;

                p = &ch395->buffer[ch395->receive_buffer_start_block[ch395->cmd_data.CMD_SocketWriteBuffer[0]]*CH395_SIZE_BLOCK_BUFFER];
                // Réception des données

                ch395->socket_int_status[ch395->cmd_data.CMD_SocketState[0]] = ch395->socket_int_status[ch395->cmd_data.CMD_SocketState[0]] | CH395_SINT_STAT_SEND_OK;


                // Set irq flag for socket
                switch (ch395->cmd_data.CMD_SocketWriteBuffer[0])
                {
                    case 0:
                        ch395->glob_int_status = ch395->glob_int_status | CH395_GINT_STAT_SOCK0;
                        break;

                    case 1:
                        ch395->glob_int_status = ch395->glob_int_status | CH395_GINT_STAT_SOCK1;
                        break;

                    case 2:
                        ch395->glob_int_status = ch395->glob_int_status | CH395_GINT_STAT_SOCK2;
                        break;

                    case 3:
                        ch395->glob_int_status = ch395->glob_int_status | CH395_GINT_STAT_SOCK3;
                        break;
                }

                switch (ch395->cmd_data.CMD_SocketWriteBuffer[0])
                {
                    case 0:
                        ch395->glob_int_status = ch395->glob_int_status_all[0] | CH395_GINT_STAT_SOCK0;
                        break;

                    case 1:
                        ch395->glob_int_status = ch395->glob_int_status_all[0] | CH395_GINT_STAT_SOCK1;
                        break;

                    case 2:
                        ch395->glob_int_status = ch395->glob_int_status_all[0] | CH395_GINT_STAT_SOCK2;
                        break;

                    case 3:
                        ch395->glob_int_status = ch395->glob_int_status_all[0] | CH395_GINT_STAT_SOCK3;
                        break;

                    case 4:
                        ch395->glob_int_status = ch395->glob_int_status_all[1] | CH395_GINT_STAT_SOCK4;
                        break;

                    case 5:
                        ch395->glob_int_status = ch395->glob_int_status_all[1] | CH395_GINT_STAT_SOCK5;
                        break;

                    case 6:
                        ch395->glob_int_status = ch395->glob_int_status_all[1] | CH395_GINT_STAT_SOCK6;
                        break;

                    case 7:
                        ch395->glob_int_status = ch395->glob_int_status_all[1] | CH395_GINT_STAT_SOCK7;
                        break;

                }
            }
            break;

        case CH395_CMD_GET_RECV_LEN_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_RECV_LEN_SN]");
            if (ch395_check_socket(data, ch395) == 1) return 1;
            ch395->cmd_data.CMD_SocketGetRecvLen[0] = data;
            printf("Socket : %d\n", data);
            dbg_printf("Socket : %d\n", data);
            ch395->nb_bytes_in_cmd_data ++;
            break;

        case CH395_CMD_READ_RECV_BUF_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_READ_RECV_BUF_SN]");

            switch(ch395->nb_bytes_in_cmd_data)
            {
                case 0:
                    if (ch395_check_socket(data, ch395) == 1) return 1;
                    printf("Socket : %d\n",data);
                    dbg_printf("Socket : %d\n",data);
                    break;

                case 1:
                    printf("low length : %d\n",data);
                    dbg_printf("low length : %d\n",data);
                    break;

                case 2:
                    printf("high length %d\n",data);
                    dbg_printf("high length %d\n",data);
                    break;

                default:
                    printf("Panic\n");
                    dbg_printf("Panic\n");
                    break;
            }

            if (ch395->nb_bytes_in_cmd_data < 4 )
            {
                // Store the socket id + length (low) + length (high)
                // FIXME : Generate warn if length is greater than we can receive
                ch395->cmd_data.CMD_SocketGetRecvBuf[ch395->nb_bytes_in_cmd_data] = data;
                ch395->nb_bytes_in_cmd_data ++;
            }

            break;

        case CH395_CMD_CLOSE_SOCKET_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_CLOSE_SOCKET_SN] Socket %d\n", data);
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_CLOSE_SOCKET_SN] Socket %d\n", data);

            if (ch395_check_socket(data, ch395) == 1) return 1;

            close(ch395->sockfd_host[data]);
            ch395->socket_status_sn[data][0] = CH395_SOCKET_CLOSED;
            ch395->socket_status_sn[data][1] = CH395_TCP_CLOSED;

            ch395->cmd_status = ch395->cmd_status | CH395_ERR_SUCCESS;
            ch395->sockfd_host[data] = 0;
            break;

        case CH395_CMD_SET_IPRAW_PRO_SN:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_IPRAW_PRO_SN]");

            if  (ch395->pos_rw_in_cmd_data == 0)
            {
                if (ch395_check_socket(data, ch395) == 1) return 1;
                printf("socket %d\n", data);
                dbg_printf("socket %d\n", data);
                ch395->pos_rw_in_cmd_data ++;
            }
            else
            {
                char *msg_panic = "PANIC : CH395_CMD_SET_IPRAW_PRO_SN can not receive 2 bytes on data port";
                printf("%s socket %d\n", msg_panic, data);
                dbg_printf("%s socket %d\n", msg_panic, data);
            }
            break;

        case CH395_CMD_PING_ENABLE:
            break;

        case CH395_CMD_GET_MAC_ADDR:
            break;

        case CH395_CMD_DHCP_ENABLE:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_DHCP_ENABLE]\n");
            break;

        case CH395_CMD_GET_DHCP_STATUS:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_DHCP_STATUS]\n");
            break;

        case CH395_CMD_GET_IP_INF:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_GET_IP_INF] CH395 PANIC impossible to write into DATA port with CH395_CMD_GET_IP_INF\n");
            break;

        case CH395_CMD_PPPOE_SET_USER_NAME:
            break;

        case CH395_CMD_PPPOE_SET_PASSWORD:
            break;

        case CH395_CMD_PPPOE_ENABLE:
            break;

        case CH395_CMD_GET_PPPOE_STATUS:
            break;

        case CH395_CMD_SET_TCP_MSS:
            break;

        case CH395_CMD_SET_TTL:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_TTL]");
            switch(ch395->nb_bytes_in_cmd_data)
            {
                case 0:
                    if (ch395_check_socket(data, ch395) == 1) return 1;
                    ch395->cmd_data.CMD_SocketTTL[0] = data;
                    printf("Setting socket : %d\n", data);
                    dbg_printf("%d\n",data);
                    ch395->pos_rw_in_cmd_data ++;
                    break;

                case 1:
                    if (data > 128)
                    {
                        printf("PANIC : CH395_CMD_SET_TTL can not have a value greater than 128 received : %d\n", data);
                        dbg_printf("PANIC : CH395_CMD_SET_TTL can not have a value greater than 128 received : %d\n", data);
                    }
                    else
                    {
                        printf("socket : %d TTL : %d\n", ch395->cmd_data.CMD_SocketTTL[0], data);
                        ch395->socket_ttl[ch395->cmd_data.CMD_SocketTTL[0]] = data;
                        ch395->pos_rw_in_cmd_data ++;
                    }
                    break;

                default:
                    printf("PANIC : CH395_CMD_SET_TTL can not receive 2 bytes on data port %d\n", data);
                    dbg_printf("PANIC : CH395_CMD_SET_TTL can not receive 2 bytes on data port %d\n", data);
                    break;

            }
            break;

        case CH395_CMD_SET_RECV_BUF:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_RECV_BUF]");
            switch(ch395->nb_bytes_in_cmd_data)
            {
                case 0:
                    if (ch395_check_socket(data, ch395) == 1) return 1;
                    ch395->cmd_data.CMD_SocketSetRecvBuf[0] = data;
                    printf("Setting socket : %d\n", data);
                    dbg_printf("%s %d\n", data);
                    ch395->pos_rw_in_cmd_data ++;
                    break;

                case 1:
                    if (data > 48)
                    {
                        printf("PANIC : CH395_CMD_SET_RECV_BUF can not have a start value greater than 48, received : %d\n", data);
                        dbg_printf("PANIC : CH395_CMD_SET_RECV_BUF can not have a start value greater than 48, received : %d\n", data);
                    }
                    else
                    {
                        printf("socket : %d start_block : %d\n", ch395->cmd_data.CMD_SocketSetRecvBuf[0], data);
                        ch395->receive_buffer_start_block[ch395->cmd_data.CMD_SocketSetRecvBuf[0]] = data;
                        ch395->pos_rw_in_cmd_data ++;
                    }
                    break;

                case 3:
                    if (data > 48)
                    {
                        printf("PANIC : CH395_CMD_SET_RECV_BUF can not have a number block value greater than 48, received : %d\n", data);
                        dbg_printf("PANIC : CH395_CMD_SET_RECV_BUF can not have a number block value value greater than 48, received : %d\n",data);
                    }
                    else
                    {
                        printf("socket : %d number block : %d\n", ch395->cmd_data.CMD_SocketSetRecvBuf[0], data);
                        ch395->receive_buffer_number_of_block[ch395->cmd_data.CMD_SocketSetRecvBuf[0]] = data;
                        ch395->pos_rw_in_cmd_data ++;
                    }
                    break;

                default:
                    printf("PANIC : CH395_CMD_SET_RECV_BUF can not receive 3 bytes on data port %d\n", data);
                    dbg_printf("PANIC : CH395_CMD_SET_RECV_BUF can not receive 3 bytes on data port %d\n", data);
                    break;
                }
            break;

        case CH395_CMD_SET_SEND_BUF:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_SEND_BUF]");
            switch(ch395->nb_bytes_in_cmd_data)
            {
                case 0:
                    if (ch395_check_socket(data, ch395) == 1) return 1;
                    ch395->cmd_data.CMD_SocketSetSendBuf[0] = data;
                    printf("Setting socket : %d\n", data);
                    dbg_printf("%s %d\n", data);
                    ch395->pos_rw_in_cmd_data ++;
                    break;

                case 1:
                    if (data > 48)
                    {
                        printf("PANIC : CH395_CMD_SET_SEND_BUF can not have a start value greater than 48, received : %d\n", data);
                        dbg_printf("PANIC : CH395_CMD_SET_SEND_BUF can not have a start value greater than 48, received : %d\n",data);
                    }
                    else
                    {
                        printf("socket : %d start_block : %d\n", ch395->cmd_data.CMD_SocketSetSendBuf[0], data);
                        ch395->transmit_buffer_start_block[ch395->cmd_data.CMD_SocketSetSendBuf[0]] = data;
                        ch395->pos_rw_in_cmd_data ++;
                    }
                    break;

                case 3:
                    if (data > 48)
                    {
                        printf("PANIC : CH395_CMD_SET_SEND_BUF can not have a number block value greater than 48, received : %d\n", data);
                        dbg_printf("PANIC : CH395_CMD_SET_SEND_BUF can not have a number block value value greater than 48, received : %d\n",data);
                    }
                    else
                    {
                        printf("socket : %d number block : %d\n", ch395->cmd_data.CMD_SocketSetSendBuf[0], data);
                        ch395->transmit_buffer_number_of_block[ch395->cmd_data.CMD_SocketSetSendBuf[0]] = data;
                        ch395->pos_rw_in_cmd_data ++;
                    }
                    break;

                default:
                    printf("PANIC : CH395_CMD_SET_SEND_BUF can not receive 3 bytes on data port %d\n",data);
                    dbg_printf("PANIC : CH395_CMD_SET_SEND_BUF can not receive 3 bytes on data port %d\n", data);
                    break;

            }
            break;

        case CH395_CMD_SET_FUN_PARA:
            ch395_debug_concat(">>[CH395][WRITE][DATA][CH395_CMD_SET_FUN_PARA] Not emulated\n");
            break;

        case CH395_CMD_SET_KEEP_LIVE_IDLE:
            break;

        case CH395_CMD_SET_KEEP_LIVE_INTVL:
            break;

        case CH395_CMD_SET_KEEP_LIVE_CNT:
            break;

        case CH395_CMD_SET_KEEP_LIVE_SN:
            break;

        case CH395_CMD_EEPROM_ERASE:
            break;

        case CH395_CMD_EEPROM_WRITE:
            break;

        case CH395_CMD_EEPROM_READ:
            break;

        case CH395_CMD_READ_GPIO_REG:
            break;

        case CH395_CMD_WRITE_GPIO_REG:
            break;

        default:
            printf(">>[CH395][UNKNOWN]\n");
            break;
    }

    return 0;
}


Uint8 ch395_read(struct expansion_bus *oric, SDL_bool fBank, unsigned int instance, Uint16 addr, SDL_bool run)
{
    fBank = fBank; // gcc [-Wunused-parameter]
    oric = oric; // gcc [-Wunused-parameter]
    if ( (!instance) || (instance > plugin_instances) )
        return (Uint8) 0;

    instance--;

    if (run)
    {
        if (addr == 0x00) return ch395_read_data_port(userdata[instance], run);
        if (addr == 0x01) return ch395_read_command_port(userdata[instance], run);
    }

    return 0;

}

SDL_bool ch395_write(struct expansion_bus *oric, SDL_bool fBank, unsigned int instance, Uint16 addr, Uint8 data)
{
    fBank = fBank; // gcc [-Wunused-parameter]
    oric = oric;  // gcc [-Wunused-parameter]
    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    if (addr == 0x00)
    {
        ch395_write_data_port(userdata[instance], data);
        return CH395_TRUE;
    }

    if (addr == 0x01)
    {
        ch395_write_command_port(userdata[instance], data);
        return CH395_TRUE;
    }

    return CH395_TRUE;

}


// -----------------------------------------------------------------------------
//                              ch395_addresses
// -----------------------------------------------------------------------------
// Called to check plugin addresses (if multiples addresses)
SDL_bool ch395_addresses(unsigned int instance,  Uint16 offset)
{
    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    dbg_printf("---CH395 addresses(%04x)\n", offset);

    instance--;

    // [base_addr, base_addr+3]
    if (offset <= 0x03)
        return SDL_TRUE;

    // [base_addr+0x0c, base_addr+0x0f]
    if ((offset >= 0x22) && (offset <= 0x23))
        return SDL_TRUE;

    return SDL_FALSE;
}

// -----------------------------------------------------------------------------
//                              ch395_shutdown
// -----------------------------------------------------------------------------
// Called on exit
SDL_bool ch395_shutdown(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]
    instance = instance; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return SDL_FALSE;

    free(userdata[instance]);

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//                              plugin_init
// -----------------------------------------------------------------------------
// Run once right after thz library load
//
SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod)
{
    dbg_printf("---CH395 init\n");

    my_tzprintfpos = tzprintfpos;
    my_tzputc = tzputc;
    mon_periphmod = _mon_periphmod;

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//                                  ch395_reset
// -----------------------------------------------------------------------------
// Called by init_machine and [F4]
SDL_bool ch395_reset(struct expansion_bus *oric, unsigned int instance)
{
    oric = oric;
    instance = instance;

    dbg_printf("CH395_reset(%d)\n", instance);
    return SDL_TRUE;
}



// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)

void mon_ch395_status(struct textzone *ptz, unsigned int instance, int pos_state, int y)
{
    int i;
    i = y;
    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_SOCK3)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "Socket3 interrupt");
        pos_state += strlen("Socket3 interrupt");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_SOCK2)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "Socket2 interrupt");
        pos_state += strlen("Socket2 interrupt");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_SOCK1)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "Socket1 interrupt");
        pos_state += strlen("Socket1 interrupt");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_SOCK0)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "Socket0 interrupt");
        pos_state += strlen("Socket0 interrupt");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_DHCP)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "DHCP interrupt");
        pos_state += strlen("DHCP interrupt");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_PHY_CHANGE)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "PHY status change interrupt");
        pos_state += strlen("PHY status change interrupt");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_IP_CONFLI)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "DHCP IP conflict");
        pos_state += strlen("DHCP IP conflict");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_UNREACH)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "UNREACH");
        pos_state += strlen("DHCP IP conflict");
    }

}

void mon_ch395_update(struct textzone *ptz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    int i;
    base_addr = base_addr; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;
    if (oldvalid)
    {
        dbg_printf("oldvalid true");
    }

    //ptz->cfc = 5;
    dbg_printf("ch395: mon update (instance=%d)\n", instance);
    //ch395->socket_status_sn[i][0]
    my_tzprintfpos( ptz, 2, 2,  "Initialized :  %02d", userdata[instance]->is_init);
    my_tzprintfpos( ptz, 2, 3,  "S |  BUFBLOCK   | STATE |   FLAGS  | TTL | SN");
    int pos_state = 4;
    for (i = 0; i < 8 ; i++)
    {
        // Display socket ID
        my_tzprintfpos( ptz, 2, 4 + i,  "%d",i);
        pos_state = 5;
        my_tzprintfpos( ptz, pos_state, 4 + i,  "R%02d/%02d", userdata[instance]->receive_buffer_start_block[i], userdata[instance]->receive_buffer_number_of_block[i]);
        pos_state += 7;
        my_tzprintfpos( ptz, pos_state, 4 + i,  "T%02d/%02d", userdata[instance]->transmit_buffer_start_block[i], userdata[instance]->transmit_buffer_number_of_block[i]);
        pos_state += 7;
        //
        // Display block


        switch(userdata[instance]->socket_status_sn[i][0])
        {
            case CH395_SOCKET_CLOSED:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "CLOSED");
                break;
            case CH395_SOCKET_OPEN:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "OPENED");
                break;
            default:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "ERR");
                break;
        }
        pos_state += 8;

        switch(userdata[instance]->socket_status_sn[i][1])
        {
            case CH395_TCP_CLOSED:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "TCP_CLOSED");
                break;

            case CH395_TCP_LISTEN:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "TCP_LISTEN");
                break;

            case CH395_TCP_ESTABLISHED:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "TCP_ESTABL");
                break;
            default:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "ERR");
                break;
        }

        pos_state += 12;
        my_tzprintfpos( ptz, pos_state, 4 + i,  "%d", userdata[instance]->socket_ttl[i]);

        pos_state += 6;
        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_TIM_OUT)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "TIM_OUT");
            pos_state += strlen("TIM_OUT");
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_SEND_OK)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "SEND_OK");
            pos_state += strlen("SEND_OK") + 1;
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_RECV)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "RECV");
            pos_state += strlen("RECV");
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_SENBUF_FREE)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "SENBUF_FREE");
            pos_state += strlen("SENBUF_FREE");
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_CONNECT)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "CONNECT");
            pos_state += strlen("CONNECT");
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_DISCONNECT)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "DISCONNECT");
            pos_state += strlen("DISCONNECT");
        }
    }

    // Displays cmd_status
    pos_state = 1;
    my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "CMD_STATUS : ");
    pos_state += strlen("CMD_STATUS : ");
    switch(userdata[instance]->cmd_status)
    {
        case CH395_ERR_SUCCESS:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "SUCCESS");
            break;

        case CH395_ERR_BUSY:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Busy, the command is being executed");
            break;

        case CH395_ERR_MEM:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Memory Management error");
            break;

        case CH395_ERR_BUF:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Buffer error");
            break;

        case CH395_ERR_TIMEOUT:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Timeout");
            break;

        case CH395_ERR_RTE:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Route error");
            break;

        case CH395_ERR_ABRT:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Connection suspended");
            break;

        case CH395_ERR_RST:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Connection reset");
            break;

        case CH395_ERR_CLSD:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Connection closed");
            break;

        case CH395_ERR_CONN:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "No connection");
            break;

        case CH395_ERR_VAL:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Value error");
            break;

        case CH395_ERR_ARG:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Parameter error");
            break;

        case CH395_ERR_USE:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Used");
            break;

        case CH395_ERR_IF:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "MAC error");
            break;

        case CH395_ERR_ISCONN:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Connected");
            break;

        case CH395_ERR_OPEN:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Opened");
            break;

        default:
            my_tzprintfpos( ptz, pos_state, 4 + i + 1,  "Status error (PANIC)");
    }

    pos_state = 1;
    my_tzprintfpos( ptz, pos_state, 4 + i + 2,  "CMD_GLOB_INT_STATUS : ");
    pos_state += strlen("CMD_GLOB_INT_STATUS : ");
    mon_ch395_status(ptz,instance,pos_state,i);

    pos_state = 1;
    my_tzprintfpos( ptz, pos_state, 4 + i + 2,  "CMD_GLOB_INT_STATUS_ALL : ");
    pos_state += strlen("CMD_GLOB_INT_STATUS_ALL : ");

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_SOCK7)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "Socket7 interrupt");
        pos_state += strlen("Socket7 interrupt");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_SOCK2)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "Socket6 interrupt");
        pos_state += strlen("Socket6 interrupt");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_SOCK5)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "Socket5 interrupt");
        pos_state += strlen("Socket5 interrupt");
    }

    if (userdata[instance]->glob_int_status&CH395_GINT_STAT_SOCK4)
    {
        my_tzprintfpos( ptz, pos_state, 4 + i + 2, "Socket4 interrupt");
        pos_state += strlen("Socket4 interrupt");
    }

    mon_ch395_status(ptz,instance,pos_state,i);
    pos_state = 1;
    my_tzprintfpos( ptz, pos_state, 4 + i + 3, "IP : %d.%d.%d.%d",userdata[instance]->ip_chip[3],userdata[instance]->ip_chip[2],userdata[instance]->ip_chip[1],userdata[instance]->ip_chip[0]);
    my_tzprintfpos( ptz, pos_state, 4 + i + 4, "Gateway : %d.%d.%d.%d",userdata[instance]->ip_chip[7],userdata[instance]->ip_chip[6],userdata[instance]->ip_chip[5],userdata[instance]->ip_chip[4]);
    my_tzprintfpos( ptz, pos_state, 4 + i + 5, "Mask: %d.%d.%d,%d",userdata[instance]->ip_chip[11],userdata[instance]->ip_chip[10],userdata[instance]->ip_chip[9],userdata[instance]->ip_chip[8]);
    my_tzprintfpos( ptz, pos_state, 4 + i + 6, "NS2 : %d.%d.%d.%d",userdata[instance]->ip_chip[15],userdata[instance]->ip_chip[14],userdata[instance]->ip_chip[13],userdata[instance]->ip_chip[12]);
    my_tzprintfpos( ptz, pos_state, 4 + i + 7, "NS1 : %d.%d.%d.%d",userdata[instance]->ip_chip[19],userdata[instance]->ip_chip[18],userdata[instance]->ip_chip[17],userdata[instance]->ip_chip[16]);

}

// -------------------------------------------------------------------------
//                      Sauvegarde de l'état
// -------------------------------------------------------------------------
// Called by monitor
void mon_ch395_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "ch395",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                NULL,
                ch395_create,
                ch395_shutdown,
                ch395_reset,
                ch395_read,
                ch395_write,
                NULL,
                mon_ch395_update,
                mon_ch395_store,
		NULL
    };
