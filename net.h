#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "tcp_ip_trace.h"
#include "WheelTimer/WheelTimer.h"
#include "tcpconst.h"
#include "utils.h"


#define TRUE  1U
#define FALSE 0U
#define MAX_VLAN_MEMBERSHIP 10U /* upto 4095 Ids possible using 12-bit vlan field */

/* Interface change flags
   Used for notification to applications */
#define IF_UP_DOWN_CHANGE_F         (0U)
#define IF_IP_ADDR_CHANGE_F         (1U)
#define IF_OPER_MODE_CHANGE_F       (1U << 1)
#define IF_VLAN_MEMBERSHIP_CHANGE_F (1U << 2)
#define IF_METRIC_CHANGE_F          (1U << 3)

typedef bool bool_t;
typedef struct graph_ graph_t;
typedef struct node_ node_t;
typedef struct interface_ interface_t;
typedef struct arp_table_ arp_table_t;
typedef struct mac_table_ mac_table_t;
typedef struct rt_table_ rt_table_t;
typedef struct isis_node_info_ isis_node_info_t;
typedef struct isis_intf_info_ isis_intf_info_t;

typedef enum {
    ACCESS,
    TRUNK,
    L2_MODE_UNKNOWN
}intf_l2_mode_t;

#pragma pack(push,1)
typedef struct ip_add_{
    unsigned char ip_addr[16];
}ip_add_t;

typedef struct mac_add_{
    unsigned char mac_addr[6];
}mac_add_t;
#pragma pack(pop)

typedef struct node_nw_prop_{
    node_t *node;    /* back pointer to the node */
    uint32_t flags;  /* Encodes device type capabilities of nodes */
    /* L2 Properties */
    arp_table_t *arp_table;
    mac_table_t *mac_table;
    rt_table_t  *rt_table;

    /* L3 Properties */
    bool_t is_lb_addr_config;
    ip_add_t lb_addr; /* Loopback address of the node */

    /* Timer properties */
    wheel_timer_t *wt;

    /* sending buffer */
    char *send_buffer;  /* used to send out pkts */
    char *send_log_buffer; /* used for logging */

    /* ISIS protocol info */
    isis_node_info_t *isis_node_info;
}node_nw_prop_t;

typedef struct intf_nw_prop_{
    /* L1 Properties */
    bool_t is_up;
    uint32_t ifindex;

    /* L2 Properties */
    mac_add_t mac_addr;
    intf_l2_mode_t intf_l2_mode;
    unsigned int vlans[MAX_VLAN_MEMBERSHIP];

    /* L3 Properties */
    bool_t is_ipaddr_config_backup; /* a Marker to know if an IP address is previously configured, but removed to operate in L2 mode */
    bool_t is_ipaddr_config;
    ip_add_t ip_addr;
    char mask;

    /* Interface Statistics */
    uint32_t pkt_recv;
    uint32_t pkt_sent;
    uint32_t xmit_pkt_dropped;

    /* ISIS interface info */
    isis_intf_info_t *isis_intf_info;
}intf_nw_prop_t;

typedef union intf_prop_changed_{
    uint32_t intf_metric;
    struct{
        uint32_t ip_addr;
        uint8_t mask;
    }ip_addr;
    bool up_status;
    intf_l2_mode_t intf_l2_mode;
    uint32_t vlan;
}intf_prop_changed_t;

static inline void init_node_nw_prop(node_nw_prop_t *node_nw_prop);
static inline void init_intf_nw_prop(intf_nw_prop_t *intf_nw_prop);

extern void init_arp_table(arp_table_t **arp_table);
extern void init_mac_table(mac_table_t **mac_table);
extern void init_rt_table(rt_table_t **rt_table);

static inline void init_node_nw_prop(node_nw_prop_t *node_nw_prop)
{
    node_nw_prop->flags = 0;
    node_nw_prop->is_lb_addr_config = FALSE;
    memset(&node_nw_prop->lb_addr, 0, 16);
    init_arp_table(&(node_nw_prop->arp_table));
    init_mac_table(&(node_nw_prop->mac_table));
    init_rt_table(&(node_nw_prop->rt_table));
    node_nw_prop->wt = init_wheel_timer(60, 1, TIMER_SECONDS);
    start_wheel_timer(node_nw_prop->wt);
    node_nw_prop->send_buffer = calloc(1, 2*TCP_PRINT_BUFFER_SIZE);
    node_nw_prop->send_log_buffer = calloc(1, TCP_PRINT_BUFFER_SIZE);
}

static inline void init_intf_nw_prop(intf_nw_prop_t *intf_nw_prop)
{
    /* L1 propoerties */
    intf_nw_prop->is_up = TRUE;
    intf_nw_prop->ifindex = get_new_ifindex();

    /* L2 Propoerties */
    memset(&intf_nw_prop->mac_addr.mac_addr, 0, 6);
    intf_nw_prop->is_ipaddr_config = FALSE;
    memset(&intf_nw_prop->ip_addr.ip_addr, 0, 16);
    intf_nw_prop->mask = 0U;

    /* L3 properties */
    intf_nw_prop->is_ipaddr_config = FALSE;
    memset(&intf_nw_prop->ip_addr.ip_addr, 0, 16);
    intf_nw_prop->mask = 0U;

    /* Interface statistics */
    intf_nw_prop->pkt_recv = 0U;
    intf_nw_prop->pkt_sent = 0U;
    intf_nw_prop->xmit_pkt_dropped = 0U;
}


#define IF_MAC(intf_ptr) ((intf_ptr)->intf_nw_prop.mac_addr.mac_addr)
#define IF_IP(intf_ptr)  ((intf_ptr)->intf_nw_prop.ip_addr.ip_addr)
#define IF_IP_EXIST(intf_ptr) ((intf_ptr)->intf_nw_prop.is_ipaddr_config)
#define IF_MASK(intf_ptr) ((intf_ptr)->intf_nw_prop.mask)
#define IF_IS_UP(intf_ptr) ((intf_ptr)->intf_nw_prop.is_up == TRUE)
#define IF_INDEX(intf_ptr) ((intf_ptr)->intf_nw_prop.ifindex)
#define NODE_LO_ADDRESS(node_ptr)  ((node_ptr)->node_nw_prop.lb_addr.ip_addr)
#define NODE_ARP_TABLE(node_ptr)   ((node_ptr)->node_nw_prop.arp_table)
#define NODE_MAC_TABLE(node_ptr)   ((node_ptr)->node_nw_prop.mac_table)
#define NODE_RT_TABLE(node_ptr)    ((node_ptr)->node_nw_prop.rt_table)
#define IS_INTF_L3_MODE(intf_ptr)  ((intf_ptr)->intf_nw_prop.is_ipaddr_config)
#define IF_L2_MODE(intf_ptr)       ((intf_ptr)->intf_nw_prop.intf_l2_mode)
#define NODE_SEND_BUFFER(node_ptr)  (node_ptr->node_nw_prop.send_buffer)

/* macro to iterate over neighbours of a node */
/* Here oif and ip_addr_ptr(gw_ip) are outward pointers */
#define ITERATE_NODE_NBRS_BEGIN(node_ptr, nbr_ptr, oif_ptr, ip_addr_ptr)    \
    do{                                                                     \
        uint8_t idx = 0U;                                                   \
        interface_t *other_intf;                                            \
        for(idx = 0U; idx < MAX_IF_PER_NODE; idx++){                        \
            oif_ptr = node_ptr->intf[idx];                                  \
            if(!oif_ptr)                                                    \
                continue;                                                   \
            other_intf = &oif_ptr->link->intf1 == oif_ptr ?                 \
            &oif_ptr->link->intf2 : &oif_ptr->link->intf1;                  \
            if(!other_intf)                                                 \
                continue;                                                   \
            nbr_ptr = get_nbr_node(oif_ptr);                                \
            ip_addr_ptr = IF_IP(other_intf);                                \

#define ITERATE_NODE_NBRS_END(node_ptr, nbr_ptr, oif_ptr, ip_addr_ptr) }}while(0);

/* APIs to set network properties to nodes and interfaces */
void interface_assign_mac_address(interface_t *interface);
bool_t node_set_loopback_address(node_t *node, char *ip_addr);
bool_t node_set_intf_ip_address(node_t *node, char *local_if, char *ip_addr, char mask);
bool_t node_unset_intf_ip_address(node_t *node, char *local_if);

void dump_nw_graph(graph_t *graph);
void dump_node_nw_props(node_t *node);
void dump_intf_props(interface_t *interface);
void dump_node_interface_stats(node_t *node);
void dump_interface_stats(interface_t *interface);

char *pkt_buffer_shift_right(char *pkt, unsigned int pkt_size, unsigned int total_buffer_size);

unsigned int ip_addr_p_to_n(char *ip_addr);

char *tcp_ip_convert_ip_n_to_p(uint32_t ip_addr, char *output_buffer);

uint32_t tcp_ip_convert_ip_p_to_n(char *ip_addr);

void ip_addr_n_to_p(unsigned int ip_addr, char *ip_addr_str);

interface_t *node_get_matching_subnet_interface(node_t *node, char *ip_addr);

bool_t is_trunk_intf_vlan_enabled(interface_t *interface, unsigned int vlan_id);

/* Can only be used for intf in ACCESS mode */
unsigned int get_access_intf_operating_vlan_id(interface_t *interface);

bool_t is_interface_l3_bidirectional(interface_t *interface);
bool_t is_same_subnet(char *ip_addr, char mask, char *other_ip_addr);
#if 0
bool_t is_same_subnet(char *ip_addr, uint16_t mask, char *ip_to_compare);
#endif

wheel_timer_t *node_get_timer_instance(node_t *node);

static inline char *intf_l2_mode_str(intf_l2_mode_t intf_l2_mode)
{
    switch(intf_l2_mode)
    {
        case ACCESS:
            return "access";

        case TRUNK:
            return "trunk";

        default:
            return "L2_MODE_UNKNOWN";

    }
}

static inline char *tcp_ip_get_new_pkt_buffer(uint32_t pkt_size){

    char *pkt = calloc(1, MAX_PACKET_BUFFER_SIZE);
    return pkt_buffer_shift_right(pkt, pkt_size, MAX_PACKET_BUFFER_SIZE);
}

static inline void tcp_ip_free_pkt_buffer(char *pkt, uint32_t pkt_size){

    free(pkt - MAX_PACKET_BUFFER_SIZE - pkt_size - PKT_BUFFER_RIGHT_ROOM);
}

#endif /* NET_H */