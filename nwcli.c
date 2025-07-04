#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"
#include "cmdcodes.h"
#include "graph.h"
#include "utils.h"
#include "tcpconst.h"
#include <stdio.h>


extern graph_t *topo;

extern int isis_config_cli_tree(param_t *param);
extern int isis_show_cli_tree(param_t *param);
extern int isis_clear_cli_tree(param_t *param);
/* The definitions below allow to hook up new commands (show/run)
   to the CLI. The Applications can write their own CLI trees and 
   hook up them here in this file, to integrate their commands to
   the CLI.   
*/
typedef int(*cli_register_cb)(param_t *);

static cli_register_cb cli_register_cb_arr_config_node_node_name_protocol_level[] =
{
    isis_config_cli_tree,
    0 /* Last member must be NULL */
};

/* show node <node-name> protocol ... */
static cli_register_cb cli_register_cb_arr_show_node_node_name_protcol_level[] =
{
    isis_show_cli_tree,

    /* Add more CB here */

    0 /*  Last member must be NULL */
};

/* clear node <node-name> protocol ....*/
static cli_register_cb cli_register_cb_arr_clear_node_node_name_protocol_level[] =
{
    isis_clear_cli_tree,
    0
};

static void
cli_register_application_cli_trees(param_t * param, cli_register_cb *cli_register_cb_arr)
{

    int i = 0;
    while (cli_register_cb_arr[i])
    {
        (cli_register_cb_arr[i])(param);
        i++;
    }
}

/* Display function when user presses ? */
void display_graph_nodes(param_t *param, ser_buff_t *tlv_buf){
    node_t *node;
    glthread_t *curr;
    ITERATE_GLTHREAD_BEGIN(&node->graph_glue, curr){
        node = graph_glue_to_node(curr);
        printf("%s\n", node->node_name);
    }ITERATE_GLTHREAD_END(&node->graph_glue, curr);
}

/*Generic Topology Commands*/
static int
show_nw_topology_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    int CMDCODE = -1;
    node_t *node = NULL;
    char *node_name = NULL;
    tlv_struct_t *tlv = NULL;
    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(&tlv->leaf_id, "node-name", sizeof("node-name")) == 0)
            node_name = tlv->value;
        else
            assert(0);
    }TLV_LOOP_END;

    switch(CMDCODE){

        case CMDCODE_SHOW_NW_TOPOLOGY:
            dump_nw_graph(topo);
            break;
        default:
            ;
    }
    return 0;
}

/*Interface Config Handler*/
extern void interface_set_l2_mode(node_t *node, interface_t *interface, char *l2_mode_option);
extern void interface_unset_l2_mode(node_t *node, interface_t *interface, char *l2_mode_option);
extern void interface_set_vlan(node_t *node, interface_t *interface, uint32_t vlan);
extern void interface_unset_vlan(node_t *node,interface_t *interface, uint32_t vlan);
extern void dump_node_interface_stats(node_t * node);

static int show_interface_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
    
    int CMDCODE;
    node_t *node;
    char *node_name;
    char *protocol_name = NULL;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "protocol-name", strlen("protocol-name")) ==0)
            protocol_name = tlv->value;        
        else
            assert(0);
    } TLV_LOOP_END;
   
    node = get_node_by_node_name(topo, node_name);

    switch(CMDCODE){

        case CMDCODE_SHOW_INTF_STATS:
            dump_node_interface_stats(node);
            break;
        default:
            ;
    }
    return 0;
}

extern void send_arp_broadcast_request(node_t *node, interface_t *oif, char * ip_addr);
static int arp_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
    node_t *node;
    char *node_name;
    char *ip_addr;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) ==0)
            ip_addr = tlv->value;
    } TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);
    send_arp_broadcast_request(node, NULL, ip_addr);
    return 0;
}

/* Display Node Interfaces - list of interfaces in a node */
void display_node_interfaces(param_t *param, ser_buff_t *tlv_buf){

    node_t *node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;

    }TLV_LOOP_END;

    if(!node_name)
        return;

    node = get_node_by_node_name(topo, node_name);
    
    int i = 0;
    interface_t *intf;

    for(; i < MAX_IF_PER_NODE; i++){

        intf = node->intf[i];
        if(!intf) continue;

        printf(" %s\n", intf->if_name);
    }
}

/* General Validations */
int validate_node_existence(char *node_name)
{
    node_t *node = get_node_by_node_name(topo, node_name);
    if(node)
        return VALIDATION_SUCCESS;
    else{
        printf("Error: Node %s does not exist\n", node_name);
        return VALIDATION_FAILED;
    }
}

int validate_mask_value(char *mask_str){

    unsigned int mask = atoi(mask_str);
    if(!mask){
        printf("Error : Invalid Mask Value\n");
        return VALIDATION_FAILED;
    }
    if(mask >= 0 && mask <= 32)
        return VALIDATION_SUCCESS;
    return VALIDATION_FAILED;
}

static int validate_if_up_down_status(char *value){

    if((strncmp(value, "up", strlen("up")) == 0U) && (strlen("up") == strlen(value))){
        return VALIDATION_SUCCESS;
    }
    else if((strncmp(value, "down", strlen("down")) == 0U) && (strlen("down") == strlen(value))){
        return VALIDATION_SUCCESS;
    }
    return VALIDATION_FAILED;
}

/*Layer 2 Commands*/

typedef struct arp_table_ arp_table_t;
extern void dump_arp_table(arp_table_t *arp_table);


static int show_arp_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
    node_t *node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0U){
            node_name = tlv->value;
        }
    }TLV_LOOP_END(tlv_buf, tlv);

    node = get_node_by_node_name(topo, node_name);
    dump_arp_table(NODE_ARP_TABLE(node));
    return 0;
}

extern void dump_mac_table(mac_table_t *mac_table);
static int show_mac_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
    node_t *node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0U){
            node_name = tlv->value;
        }
    }TLV_LOOP_END(tlv_buf, tlv);

    node = get_node_by_node_name(topo, node_name);
    dump_mac_table(NODE_MAC_TABLE(node));
    return 0;
}

/*Layer 3 Commands*/
extern void layer5_ping_fn(node_t *node, char *dst_ip_addr);
extern void layer5_ero_ping_fn(node_t *node, char *dst_ip_addr, char *ero_ip_addr);

static int ping_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    int CMDCODE;
    node_t *node;
    char *ip_addr = NULL, 
         *ero_ip_addr = NULL;
    char *node_name;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    tlv_struct_t *tlv = NULL;
 
    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) ==0)
            ip_addr = tlv->value;
        else if(strncmp(tlv->leaf_id, "ero-ip-address", strlen("ero-ip-address")) ==0)
            ero_ip_addr = tlv->value;
        else
            assert(0);
    }TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);

    switch(CMDCODE){

        case CMDCODE_PING:
            layer5_ping_fn(node, ip_addr);
            break;
        case CMDCODE_ERO_PING:
            layer5_ero_ping_fn(node, ip_addr, ero_ip_addr);
        default:
            ;
    }
    return 0;
}

extern void dump_rt_table(rt_table_t **rt_table);
static int show_rt_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
    node_t * node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
    }TLV_LOOP_END(tlv_buf, tlv);

    node = get_node_by_node_name(topo, node_name);
    dump_rt_table(NODE_RT_TABLE(node));
    return 0;
}

static int
l3_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    node_t *node = NULL;
    char *node_name = NULL;
    char *intf_name = NULL;
    char *gwip = NULL;
    char *mask_str = NULL;
    char *dest = NULL;
    int CMDCODE = -1;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf); 
    
    tlv_struct_t *tlv = NULL;
    
    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0U)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) == 0U)
            dest = tlv->value;
        else if(strncmp(tlv->leaf_id, "gw-ip", strlen("gw-ip")) == 0U)
            gwip = tlv->value;
        else if(strncmp(tlv->leaf_id, "mask", strlen("mask")) == 0U)
            mask_str = tlv->value;
        else if(strncmp(tlv->leaf_id, "oif", strlen("oif")) == 0U)
            intf_name = tlv->value;
        else
            assert(0);

    }TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);

    char mask;
    if(mask_str){
        mask = atoi(mask_str);
    }

    switch(CMDCODE){
        case CMDCODE_CONF_NODE_L3ROUTE:
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                {
                    interface_t *intf;
                    if(intf_name){
                        intf = get_node_intf_by_name(node, intf_name);
                        if(!intf){
                            printf("Config Error : Non-Existing Interface : %s\n", intf_name);
                            return -1;
                        }
                        if(!IS_INTF_L3_MODE(intf)){
                            printf("Config Error : Not L3 Mode Interface : %s\n", intf_name);
                            return -1;
                        }
                    }
                    rt_table_add_route(NODE_RT_TABLE(node), dest, mask, gwip, intf_name);
                }
                break;
                case CONFIG_DISABLE:
                    delete_rt_table_entry(NODE_RT_TABLE(node), dest, mask);
                    break;
                default:
                    ;
            }
            break;
        default:
            break;
    }
    return 0;
}

static int intf_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
    char *node_name;
    node_t *node;
    char *intf_name;
    interface_t *interface;
    char *if_up_down;
    int CMDCODE;
    tlv_struct_t *tlv = NULL;

    char *l2_mode_option;
    uint32_t intf_new_metric_val;
    uint32_t vlan_id;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "if-name", strlen("if-name")) ==0)
            intf_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "vlan-id", strlen("vlan-d")) ==0)
            vlan_id = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "l2-mode-val", strlen("l2-mode-val")) == 0)
            l2_mode_option = tlv->value;
        else if(strncmp(tlv->leaf_id, "if-up-down", strlen("if-up-down")) == 0)
             if_up_down = tlv->value; 
        else if(strncmp(tlv->leaf_id, "metric-val", strlen("metric-val")) == 0)
             intf_new_metric_val = atoi(tlv->value);            
        else
            assert(0);

    } TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);
    interface = get_node_intf_by_name(node, intf_name);
    if(!interface){
        printf("%s: Interface %s does not exist\n", __FUNCTION__, intf_name);
        return -1;
    }

    uint32_t if_change_flags = 0U;
    switch(CMDCODE)
    {
        case CMDCODE_INTF_CONFIG_METRIC:
        {
            uint32_t intf_existing_metric = get_link_cost(interface);
            if(intf_existing_metric != intf_new_metric_val){
                SET_BIT(if_change_flags, IF_METRIC_CHANGE_F);
            }
            switch(enable_or_disable)
            {
                case CONFIG_ENABLE:
                {
                    interface->link->cost = intf_new_metric_val;
                    break;
                }
                case CONFIG_DISABLE:
                {
                    interface->link->cost = INTF_METRIC_DEFAULT;
                    break;
                }
                default:
                    ;
            }
            if(IS_BIT_SET(if_change_flags, IF_METRIC_CHANGE_F)){
                nfc_intf_invoke_notification_to_sbscribers(interface, 0, if_change_flags);
            }
            break;
        }
        case CMDCODE_CONF_INTF_UP_DOWN:
        {
            if(strncmp(if_up_down, "up", strlen("up")) == 0U){
                if(interface->intf_nw_prop.is_up == FALSE){
                    SET_BIT(if_change_flags, IF_UP_DOWN_CHANGE_F);
                }
                interface->intf_nw_prop.is_up = TRUE;
            }
            else if (strncmp(if_up_down, "down", strlen("down")) == 0U){
                if(interface->intf_nw_prop.is_up == TRUE){
                    SET_BIT(if_change_flags, IF_UP_DOWN_CHANGE_F);
                }
                interface->intf_nw_prop.is_up = FALSE;
            }
            if(IS_BIT_SET(if_change_flags, IF_UP_DOWN_CHANGE_F)){
                nfc_intf_invoke_notification_to_subscribers(interface, 0, if_change_flags);
            }
            break;
        }
        case CMDCODE_INTF_CONFIG_L2_MODE:
        {
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                    interface_set_l2_mode(node, interface, l2_mode_option);
                    break;
                case CONFIG_DISABLE:
                    interface_unset_l2_mode(node, interface, l2_mode_option);
                    break;
                default: 
                    ;
            }

            break;
        }
        case CMDCODE_INTF_CONFIG_VLAN:
        {
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                    interface_set_vlan(node, interface, vlan_id);
                    break;
                case CONFIG_DISABLE:
                    interface_unset_vlan(node, interface, vlan_id);
                    break;
                default:
                    ;
            }
            break;
        }
        default:
            ;
    }
}

extern void tcp_ip_refresh_tcp_log_file();

static int clear_topology_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
    int CMDCODE = -1;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    switch(CMDCODE) {
        case CMDCODE_CLEAR_LOG_FILE:
            tcp_ip_refresh_tcp_log_file();
            break;
        default: ;
    }
}

static int validate_interface_metric_val(char *value){

    uint32_t metric_val = atoi(value);
    if(metric_val > 0 && metric_val <= INTF_MAX_METRIC)
        return VALIDATION_SUCCESS;
    return VALIDATION_FAILED;
}

extern int spf_algo_handler(param_t param, ser_buff_t *tlv_buf, op_mode enable_or_disable);
extern void tcp_ip_traceoptions_cli(param_t *node_name_param, param_t *intf_name_param);
extern int traceoptions_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable);  

void nw_init_cli(void)
{
    init_libcli();

    param_t *show       = libcli_get_show_hook();
    param_t *debug      = libcli_get_debug_hook();
    param_t *config     = libcli_get_config_hook();
    param_t *run        = libcli_get_run_hook();
    param_t *debug_show = libcli_get_debug_show_hook();
    param_t *root       = libcli_get_root();
    param_t *clear      = libcli_get_clear_hook();

    /* Clear commands */
    {
        {
            /* clear log */
            static param_t log_file;
            init_param(&log_file, CMD, "log-file", clear_topology_handler, 0, INVALID, 0, "clear log-file");
            libcli_register_param(clear, &log_file);
            set_param_cmd_code(&log_file, CMDCODE_CLEAR_LOG_FILE);
        }
        /* clear node ....*/
        static param_t node;
        init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
        libcli_register_param(clear, &node);
        libcli_register_display_callback(&node, display_graph_nodes);
        {
            /*clear node <node-name>*/ 
            static param_t node_name;
            init_param(&node_name, LEAF, 0, 0, validate_node_existence, STRING, "node-name", "Node Name");
            libcli_register_param(&node, &node_name);	
		    {
			    /* clear node <node-name> protocol */
				static param_t protocol;
				init_param(&protocol, CMD, "protocol", 0, 0, INVALID, 0, "App protocol");
				libcli_register_param(&node_name, &protocol);

				/* show node <node-name> protocol ...*/
				cli_register_application_cli_trees(&protocol, cli_register_cb_arr_clear_node_node_name_protocol_level);
			}
        }
    }

    {
        /* show topology */
        static param_t topology;
        init_param(&topology, CMD, "topology", show_nw_topology_handler, 0, INVALID, 0, "Dump complete n/w topology");
        libcli_register_param(show, &topology);
        set_param_cmd_code(&topology, CMDCODE_SHOW_NW_TOPOLOGY);
        {
            /*show topology node*/ 
            static param_t node;
            init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
            libcli_register_param(&topology, &node);
            libcli_register_display_callback(&node, display_graph_nodes);
            {
               /*show topology node <node-name>*/ 
                static param_t node_name;
                init_param(&node_name, LEAF, 0, show_nw_topology_handler, validate_node_existence, STRING, "node-name", "Node Name");
                libcli_register_param(&node, &node_name);
                set_param_cmd_code(&node_name, CMDCODE_SHOW_NW_TOPOLOGY);
            }
        }

        /* Show node */
        static param_t node;
        init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
        libcli_register_param(show, &node);
        libcli_register_display_callback(&node, display_graph_nodes);
        {
            /* show node <node_name> */
            static param_t node_name;
            init_param(&node_name, LEAF, 0, 0, validate_node_existence, STRING, "node-name", "Node Name");
            libcli_register_param(&node, &node_name);
            {
                {
                    /* show node <node-name> protocol */
                    static param_t protocol;
                    init_param(&protocol, CMD, "protocol", 0, 0, INVALID, 0, "App protocol");
                    libcli_register_param(&node_name, &protocol);

                    /* show node <node-name> protocol ...*/
                    cli_register_application_cli_trees(&protocol, cli_register_cb_arr_show_node_node_name_protcol_level);
                }
                {
                    /* show node <node_name> log-status */
                    static param_t log_status;
                    init_param(&log_status, CMD, "log-status", traceoptions_handler, 0, INVALID, 0, "log-status");
                    libcli_register_param(&node_name, &log_status);
                    set_param_cmd_code(&log_status, CMDCODE_DEBUG_SHOW_LOG_STATUS);
                }

                {
                    /* Show node <node_name> arp */
                    static param_t arp;
                    init_param(&arp, CMD, "arp", show_arp_handler, 0, INVALID, 0, "Dump Arp Table");
                    libcli_register_param(&node_name, &arp);
                    set_param_cmd_code(&arp, CMDCODE_SHOW_NODE_ARP_TABLE);
                }
                {
                    /* show node <node_name> mac */
                    static param_t mac;
                    init_param(&mac, CMD, "mac", show_mac_handler, 0, INVALID, 0, "Dump Mac Table");
                    libcli_register_param(&node_name, &mac);
                    set_param_cmd_code(&mac, CMDCODE_SHOW_NODE_MAC_TABLE);
                }
                {
                    /* show node <node_name> rt */
                    static param_t rt;
                    init_param(&rt, CMD, "rt", show_rt_handler, 0, INVALID, 0, "Dump L3 Routing Table");
                    libcli_register_param(&node_name, &rt);
                    set_param_cmd_code(&rt, CMDCODE_SHOW_NODE_RT_TABLE);
                }
                {
                    /* show node <node_name> interface */
                    static param_t interface;
                    init_param(&interface, CMD, "interface", 0, 0, INVALID, 0, "\"interface\" keyword");
                    libcli_register_param(&node_name, &interface);
                    {
                        /* show node <node_name> interface statistics */
                        static param_t stats;
                        init_param(&stats, CMD, "statistics", show_interface_handler, 0, INVALID, 0, "Interface Statistics");
                        libcli_register_param(&interface, &stats);
                        set_param_cmd_code(&stats, CMDCODE_SHOW_INTF_STATS);
                        {
                            /* show node <node_name> interface statistics protocol */
                            static param_t protocol;
                            init_param(&protocol, CMD, "protocol", 0, 0, INVALID, 0, "Protocol specific intf stats");
                            libcli_register_param(&stats, &protocol);
                        }
                    }
                }
                {
                    /*show node <node-name> spf-result*/
                    static param_t spf;
                    init_param(&spf, CMD, "spf-result", spf_algo_handler, 0, INVALID, 0, "SPF Results");
                    libcli_register_param(&node_name, &spf);
                    set_param_cmd_code(&spf, CMDCODE_SHOW_SPF_RESULTS);
                 }           
            }
        }
    }

    {
        /* run spf */
        static param_t spf;
        init_param(&spf, CMD, "spf", 0, 0, INVALID, 0, "Shortest Path");
        libcli_register_param(run, &spf);
        {
            /* run spf all */
            static param_t all;
            init_param(&all, CMD, "all", spf_algo_handler, 0, INVALID, 0, "ALL nodes");
            libcli_register_param(&spf, &all);
            set_param_cmd_code(&all, CMDCODE_RUN_SPF_ALL);
        }
    }

    /* run node */
    {
        static param_t node;
        init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
        libcli_register_param(run, &node);
        libcli_register_display_callback(&node, display_graph_nodes);
        {
            /* run node <node_name> */
            static param_t node_name;
            init_param(&node_name, LEAF, 0, 0, validate_node_existence, STRING, "node-name", "Node Name");
            libcli_register_param(&node, &node_name);
            {
                /* run node <node_name> ping */
                static param_t ping;
                init_param(&ping, CMD, "ping", 0, 0, INVALID, 0, "Ping Utility");
                libcli_register_param(&node_name, &ping);
                {
                    /* run node <node_name> ping <ip-address> */
                    static param_t ip_addr;
                    init_param(&ip_addr, LEAF, 0, ping_handler, 0, IPV4, "ip-address", "Ipv4 Address");
                    libcli_register_param(&ping, &ip_addr);
                    set_param_cmd_code(&ip_addr, CMDCODE_PING);
                    {
                        static param_t ero;
                        init_param(&ero, CMD, "ero", 0, 0, INVALID, 0, "ERO(Explicit Route Object)");
                        libcli_register_param(&ip_addr, &ero);
                        {
                            static param_t ero_ip_addr;
                            init_param(&ero_ip_addr, LEAF, 0, ping_handler, 0, IPV4, "ero-ip-address", "ERO Ipv4 Address");
                            libcli_register_param(&ero, &ero_ip_addr);
                            set_param_cmd_code(&ero_ip_addr, CMDCODE_ERO_PING);
                        }
                    }
                }

                /* run node <node_name> resolve-arp */
                static param_t resolve_arp;
                init_param(&resolve_arp, CMD, "resolve-arp", 0, 0, INVALID, 0, "Resolve ARP");
                libcli_register_param(&node_name, &resolve_arp);
                {
                    /* run node <node_name> resolve-arp <ip-address> */
                    static param_t ip_addr;
                    init_param(&ip_addr, LEAF, 0, arp_handler, 0, IPV4, "ip-address", "Nbr IPv4 Address");
                    libcli_register_param(&resolve_arp, &ip_addr);
                    set_param_cmd_code(&ip_addr, CMDCODE_RUN_ARP);
                }

                /* run node <node_name> spf */
                static param_t spf;
                init_param(&spf, CMD, "spf", spf_algo_handler, 0, INVALID, 0, "Trigger spf");
                libcli_register_param(&node_name, &spf);
                set_param_cmd_code(&spf, CMDCODE_RUN_SPF);
            }
        }
    }

    {
        /* config global */
        static param_t global;
        init_param(&global, CMD, "global", 0, 0, INVALID, 0, "global network-wide config");
        libcli_register_param(config, &global);
        {
            /* config global stdout */
            static param_t _stdout;
            init_param(&_stdout, CMD, "stdout", traceoptions_handler, 0, INVALID, 0, "Turn ON stdio logging");
            libcli_register_param(&global, &_stdout);
            set_param_cmd_code(&_stdout, CMDCODE_DEBUG_GLOBAL_STDOUT);
        }
        {
            /* config global no-stdout */
            static param_t _no_stdout;
            init_param(&_no_stdout, CMD, "no-stdout", traceoptions_handler, 0, INVALID, 0, "Turn OFF stdio logging");
            libcli_register_param(&global, &_no_stdout);
            set_param_cmd_code(&_no_stdout, CMDCODE_DEBUG_GLOBAL_NO_STDOUT);
        }
    }
    {
        /* config node */
        static param_t node;
        init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
        libcli_register_param(config, &node);
        libcli_register_display_callback(&node, display_graph_nodes);
        {
            /*config node <node_name> */
            static param_t node_name;
            init_param(&node_name, LEAF, 0, 0, validate_node_existence, STRING, "node-name", "Node Name");
            libcli_register_param(&node, &node_name);
            {
                {
                    /*config node <node-name> [no] protocol*/
                    static param_t protocol;
                    init_param(&protocol, CMD, "protocol", 0, 0, INVALID, 0, "protocol");
                    libcli_register_param(&node_name, &protocol);

                    /* config node <node-name> protocol....*/
                    cli_register_application_cli_trees(&protocol,
                                                       cli_register_cb_arr_config_node_node_name_protocol_level);
                    support_cmd_negation(&protocol);
                }
                /* config node <node_name> traceoptions flag <flag-val> */
                tcp_ip_traceoptions_cli(&node_name, NULL);

                /* config node <node_name> interface */
                static param_t interface;
                init_param(&interface, CMD, "interface", 0, 0, INVALID, 0, "\"interface\" keyword");
                libcli_register_display_callback(&interface, display_node_interfaces);
                libcli_register_param(&node_name, &interface);
                {
                    /* config node <node_name> interface <if-name> */
                    static param_t if_name;
                    init_param(&if_name, LEAF, 0, 0, 0, STRING, "if-name", "Interface Name");
                    libcli_register_param(&interface, &if_name);
                    {
                        /* config node <node_name> interface <if-name> traceoptions flag <flag-val> */
                        tcp_ip_traceoptions_cli(NULL, &if_name);
                        
                        {
                            /* config node <node_name> interface <if_name> <up|down> */
                            static param_t if_up_down_status;
                            init_param(&if_up_down_status, LEAF, 0, intf_config_handler, validate_if_up_down_status, STRING, "if-up-down", "<up | down>");
                            libcli_register_param(&if_name, &if_up_down_status);
                            set_param_cmd_code(&if_up_down_status, CMDCODE_CONF_INTF_UP_DOWN);
                        }
                        {
                            static param_t metric;
                            init_param(&metric, CMD, "metric", 0, 0, INVALID, 0, "Interface Metric");
                            libcli_register_param(&if_name, &metric);
                            {
                                static param_t metric_val;
                                init_param(&metric_val, LEAF, 0, intf_config_handler, validate_interface_metric_val, INT, "metric-val", "Metric Value(1-16777215)");
                                libcli_register_param(&metric, &metric_val);
                                set_param_cmd_code(&metric_val, CMDCODE_INTF_CONFIG_METRIC);
                            }
                        }
                        {
                            /* config node <node-name> ineterface <if-name> ip-address <ip-addr> <mask>*/
                            static param_t ip_addr;
                            init_param(&ip_addr, CMD, "ip-address", 0, 0, INVALID, 0, "Interface IP Address");
                            libcli_register_param(&if_name, &ip_addr);
                            {
                                static param_t ip_addr_val;
                                init_param(&ip_addr_val, LEAF, 0, 0, 0, IPV4, "intf-ip-address", "IPV4 address");
                                libcli_register_param(&ip_addr, &ip_addr_val);
                                {
                                    static param_t mask;
                                    init_param(&mask, LEAF, 0, intf_config_handler, validate_mask_value, INT, "mask", "mask [0-32]");
                                    libcli_register_param(&ip_addr_val, &mask);
                                    set_param_cmd_code(&mask, CMDCODE_INTF_CONFIG_IP_ADDR);
                                }
                            }
                        }
                        /* config node <node_name> interface <if-name> l2mode */
                        {
                            /* config node <node_name> interface <if-name> l2mode <access|trunk> */
                        }
                        /* config node <node_name> interface <if-name> vlan */
                        {
                            /* config node <node_name> interface <if-name> vlan <vlan-id> */
                        }
                    }
                }
                /* config node <node_name> route */
                {
                    static param_t route;
                    init_param(&route, CMD, "route", 0, 0, INVALID, 0, "L3 route");
                    libcli_register_param(&node_name, &route);

                    /* config node <node_name> route <ip-address> */
                    {
                        static param_t ip_addr;
                        init_param(&ip_addr, LEAF, 0, 0, 0, IPV4, "ip-address", "IPv4 Address");
                        libcli_register_param(&route, &ip_addr);

                        /* config node <node_name> route <ip-address> <mask> */
                        {
                            static param_t mask;
                            init_param(&mask, LEAF, 0, l3_config_handler, validate_mask_value, INT, "mask", "mask(0-32)");
                            libcli_register_param(&ip_addr, &mask);
                            set_param_cmd_code(&mask, CMDCODE_CONF_NODE_L3ROUTE);

                            /* config node <node_name> route <ip-address> <mask> <gw-ip> */
                            {
                                static param_t gwip;
                                init_param(&gwip, LEAF, 0, 0, 0, IPV4, "gw-ip", "IPv4 Address");
                                libcli_register_param(&mask, &gwip);

                                /* config node <node_name> toute <ip-address> <mask> <gw-ip> <oif> */
                                {
                                    static param_t oif;
                                    init_param(&oif, LEAF, 0, l3_config_handler, 0, STRING, "oif", "Out-going intf Name");
                                    libcli_register_param(&gwip, &oif);
                                    set_param_cmd_code(&oif, CMDCODE_CONF_NODE_L3ROUTE);
                                }
                            }
                        }
                    }
                }
            }
            support_cmd_negation(&node_name);
        }
    }
    support_cmd_negation(config);
}