# Plugin ch376

## Description du plugin

Ce plugin simule une interface à base de ch376 qui permet d'accèder à une carte mémoire SD ou USB.

Il permet de manière partielle de gérer les autres devices usb (autres que mass storage). Voir section "Devices usb"


Son adresse de base est $341 à $342.

>[!NOTE]
>Cette carte est nécessaire pour le fonctionnement d'Orix.

## Fonctions utilisées

Cet exemple ne nécessite que les fonctions:
* create()
* shutdown()
* reset()

    Initialisation de la carte.

* read()

    Lecture des registres du ch376.

* write()

    Écriture des registres du ch376.


* mon_update()

    Affiche une page dans le moniteur de Oricutron indiquant l'état du ch376 ainsi que la dernière commande exécutée.

* mon_store()

    Sauvegarde l'état du périphérique entre deux appels du moniteur


## Constantes

    #define BASE_ADDR 0x340
    #define END_ADDR 0x341

    #define INSTANCE_MAX 1


    static struct ch376 * userdata[INSTANCE_MAX];
    static struct ch376 * userdata_old[INSTANCE_MAX];

## Déclaration du plugin:

    struct PLUGIN plugin = { "CH376",
                    BASE_ADDR, END_ADDR-BASE_ADDR+1,
                    PLG_DEVICE,
                    NULL,
                    plugin_create,
                    plugin_shutdown,
                    plugin_reset,
                    plugin_read,
                    plugin_write,
		    NULL,
                    mon_plugin_update,
                    mon_plugin_store,
        };

## Activation du plugin

Ajouter les lignes suivantes dans le fichier plugins.cfg:

    [debug]
    load = yes
    plugin = 'plugins/libch376.so'
    enable = yes

>[!NOTE]
>Ce plugin n'autorise qu'une instance.

## Utilisation du plugin

Le port de donnée du ch376 est en $340 et son port de commande en $341.

Se reporter à la documentation du ch376 pour plus de détails.

>[!NOTE]
>Il faut également activer le plugin Twilighte si on veut utiliser Orix.

## Devices usb

Il est actuellement possible d'interroger le descriptor usb d'un device à partir du moment où on spécifie un fichier de conf qui définit son descriptor ou d'accéder au descriptor d'un device usb du hôte qui fait tourner l'émulation.

### Fichier de conf pour un device usb "virtuel"

Exemple de contenu du fichier ch376.

Ainsi l'appel sur le descriptor de l'usb branché sur le port principal (et unique du ch376) prendra les valeurs définies dans plugins/usb_device_mass_storage.cfg

```
device_connected_to_usb_port=usb_device_mass_storage.cfg
```

Exemple de fichier .cfg pour une souris usb :

```
[USB_DEVICE_DESCRIPTOR]
bLength=0x12
bDescriptorType=0x01
bcdUSB=0x0200
bDeviceClass=0x03
bDeviceSubClass=0x00
bDeviceProtocol=0x00
bMaxPacketSize0=0x40
idVendor=0x1e7d
idProduct=0x2c8b
bcdDevice=0x0100
iManufacturer=0x00
iProduct=0x00
iSerialNumber=0x00
bNumConfigurations=0x01
```

### Accéder à un device usb host

Pour accéder à un device usb physique local, il faut qu'il soit visible de lsusb et surtout il ne doit pas être occupé par le kernel (c'est le cas pour le clavier, souris, dongle wifi, et ethernet puisqu'ils sont nécessaire pour un fonctionnement normal).

Par exemple, le device bluetooth marchera s'il n'est pas utilisé, ou si un dongle wifi n'est pas configuré, ou un device ethernet.

Pour que la récupération de l'émulation fonctionne, il faut donc spécifier l'idvendor et l'idproduct du device dans la conf du ch376.cfg :

```
device_connected_to_usb_port=8087:0026
```

### Accéder à un device usb host sur wsl

Sur powershell

```
usbipd list

1-2    0bda:8156  Realtek Gaming USB 2.5GbE Family Controller                   Not shared
2-5    27c6:538d  Goodix fingerprint                                            Not shared
2-6    0c45:671b  Integrated Webcam                                             Not shared
2-10   8087:0026  Intel(R) Wireless Bluetooth(R)                                Shared
3-1    1e7d:2c8b  Périphérique d’entrée USB                                     Shared
3-2    1e7d:314c  Périphérique d’entrée USB                                     Not shared
```

Puis attacher le device à wsl :

```
 usbipd attach --wsl --busid 2-10
```
