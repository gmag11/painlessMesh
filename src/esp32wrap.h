#ifndef ESP32WRAP_H
#define ESP32WRAP_H

//Test: pio run -d test/network/ -t upload -e esp32

// This might well be it https://github.com/devicexx/espconn-esp32
#include "lwip/dns.h"
// #include "esp_wifi_types.h"
#include "esp_event.h"
// #include "esp_types.h"

typedef int8_t   sint8;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef int16_t  sint16;
typedef uint32_t uint32;

#if 1
#define espconn_printf printf
#else 
#define espconn_printf(fmt, args...)
#endif


typedef void *espconn_handle;
typedef void (* espconn_connect_callback)(void *arg);
typedef void (* espconn_reconnect_callback)(void *arg, sint8 err);

/* Definitions for error constants. */

#define ESPCONN_OK          0    /* No error, everything OK. */
#define ESPCONN_MEM        -1    /* Out of memory error.     */
#define ESPCONN_TIMEOUT    -3    /* Timeout.                 */
#define ESPCONN_RTE        -4    /* Routing problem.         */
#define ESPCONN_INPROGRESS  -5   /* Operation in progress    */
#define ESPCONN_MAXNUM		-7	 /* Total number exceeds the set maximum*/

#define ESPCONN_ABRT       -8    /* Connection aborted.      */
#define ESPCONN_RST        -9    /* Connection reset.        */
#define ESPCONN_CLSD       -10   /* Connection closed.       */
#define ESPCONN_CONN       -11   /* Not connected.           */

#define ESPCONN_ARG        -12   /* Illegal argument.        */
#define ESPCONN_IF		   -14	 /* Low_level error			 */
#define ESPCONN_ISCONN     -15   /* Already connected.       */

#define ESPCONN_HANDSHAKE  -28   /* ssl handshake failed	 */
#define ESPCONN_RESP_TIMEOUT -29 /* ssl handshake no response*/
#define ESPCONN_PROTO_MSG  -61   /* ssl application invalid	 */

#define ESPCONN_SSL			0x01
#define ESPCONN_NORM		0x00

#define ESPCONN_STA			0x01
#define ESPCONN_AP			0x02
#define ESPCONN_AP_STA		0x03

#define STA_NETIF      0x00
#define AP_NETIF       0x01

/** Protocol family and type of the espconn */
enum espconn_type {
    ESPCONN_INVALID    = 0,
    /* ESPCONN_TCP Group */
    ESPCONN_TCP        = 0x10,
    /* ESPCONN_UDP Group */
    ESPCONN_UDP        = 0x20,
};

/** Current state of the espconn. Non-TCP espconn are always in state ESPCONN_NONE! */
enum espconn_state {
    ESPCONN_NONE,
    ESPCONN_WAIT,
    ESPCONN_LISTEN,
    ESPCONN_CONNECT,
    ESPCONN_WRITE,
    ESPCONN_READ,
    ESPCONN_CLOSE
};

typedef struct _esp_tcp {
    int remote_port;
    int local_port;
    uint8 local_ip[4];
    uint8 remote_ip[4];
	espconn_connect_callback connect_callback;
	espconn_reconnect_callback reconnect_callback;
	espconn_connect_callback disconnect_callback;
	espconn_connect_callback write_finish_fn;
} esp_tcp;

typedef struct _esp_udp {
    int remote_port;
    int local_port;
    uint8 local_ip[4];
	uint8 remote_ip[4];
} esp_udp;

typedef struct _remot_info{
	enum espconn_state state;
	int remote_port;
	uint8 remote_ip[4];
}remot_info;

/** A callback prototype to inform about events for a espconn */
typedef void (* espconn_recv_callback)(void *arg, char *pdata, unsigned short len);
typedef void (* espconn_sent_callback)(void *arg);

/** A espconn descriptor */
struct espconn {
    /** type of the espconn (TCP, UDP) */
    enum espconn_type type;
    /** current state of the espconn */
    enum espconn_state state;
    union {
        esp_tcp *tcp;
        esp_udp *udp;
    } proto;
    /** A callback function that is informed about events for this espconn */
    espconn_recv_callback recv_callback;
	espconn_sent_callback sent_callback;
	uint8 link_cnt;
	void *reserve;
};

typedef system_event_t System_Event_t;
typedef wifi_auth_mode_t _auth_mode;

#define AUTH_WPA2_PSK WIFI_AUTH_WPA2_PSK

#define PHY_MODE_11B WIFI_PROTOCOL_11B
#define PHY_MODE_11G WIFI_PROTOCOL_11G
#define PHY_MODE_11N WIFI_PROTOCOL_11B
typedef int phy_mode_t;

typedef struct bss_info {
    STAILQ_ENTRY(bss_info)     next;

    uint8 bssid[6];
    uint8 ssid[32];
    uint8 ssid_len;
    uint8 channel;
    sint8 rssi;
    _auth_mode authmode;
    uint8 is_hidden;
    sint16 freq_offset;
    sint16 freqcal_val;
	uint8 *esp_mesh_ie;
} bss_info_t;

#endif
