#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ch376.h"

#define MAX_LINE_LENGTH 256

void parse_hex(const char *hex_str, uint8_t *buffer, int max_len)
{
    char *ptr = strdup(hex_str);
    char *token = strtok(ptr, " ");
    int i = 0;
    while (token != NULL && i < max_len)
    {
        buffer[i++] = (uint8_t)strtol(token, NULL, 16);
        token = strtok(NULL, " ");
    }
    free(ptr);
}



void parse_usb_cfg(const char *filename, struct usb_device_descriptor_t *device)
{
    FILE *file = fopen(filename, "r");

    char cwd[PATH_MAX]; // PATH_MAX est défini dans <limits.h>


    if (file == NULL)
    {
        printf("[CH376 plugin] Erreur ouverture fichier %s\n", filename);
        exit(1);
        return;
    }
    else
    {
        printf("%s found. Reading descriptor\n", filename);
    }

    char line[MAX_LINE_LENGTH];
    uint8_t config_desc[64] = {0};
    uint8_t report_desc[64] = {0};
    int config_desc_len = 0;
    int report_desc_len = 0;
    int j = 0;

    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == ';' || line[0] == '\n')
            continue;

        if (strstr(line, "[USB_DEVICE_DESCRIPTOR]"))
        {
            while (fgets(line, sizeof(line), file) && line[0] != '[')
            {
                if (sscanf(line, "bLength=%hhx", &device->bLength) == 1)
                {
                    continue;
                }
                if (sscanf(line, "bDescriptorType=%hhx", &device->bDescriptorType) == 1) continue;
                if (sscanf(line, "bcdUSB=%hx", &device->bcdUSB) == 1) continue;
                //if (sscanf(line, "bDeviceClass=0x%x", &device->bDeviceClass) == 1) 
                if (strstr(line, "bDeviceClass=") == line)
                {
                    char *egal = strchr(line, '=');
                    if (egal != NULL)
                    {
                        int valeur;
                        sscanf(egal + 1, "%x", &device->bDeviceClass);
                    }
                };

                if (sscanf(line, "idVendor=%hx", &device->idVendor) == 1) continue;
                if (sscanf(line, "idProduct=%hx", &device->idProduct) == 1) continue;
                // ... (autres champs)

                j++;
        }
        printf("USB_DEVICE_DESCRIPTOR found, length found : %d\n", device->bLength);
        }
        else if (strstr(line, "[USB_HID_REPORT_DESCRIPTOR]")) {
            char *ptr = strstr(line, "Data = ");
            if (ptr) {
                parse_hex(ptr + 7, report_desc, sizeof(report_desc));
                report_desc_len = strlen(ptr + 7) / 3; // Approximation
            }
        }
    }

    fclose(file);

}
