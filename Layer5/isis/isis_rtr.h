#ifndef __ISIS_RTR_H__
#define __ISIS_RTR_H__

#include "../../EventDispatcher/event_dispatcher.h"
#include "../../avlTree/avlTree.h"

typedef struct isis_lsp_pkt_ isis_lsp_pkt_t; 

typedef struct isis_reconcil_data_{
    /* reconsiliation status */
    bool reconciliation_in_progress;
    /* reconciliation timer */
    timer_event_handle *reconciliation_timer;
}isis_reconcil_data_t;

typedef struct isis_timer_data_ {

    node_t *node;
    interface_t *intf;
    void *data;
    size_t data_size;
} isis_timer_data_t;

typedef struct isis_node_info_{
    uint16_t adj_up_count;
    uint32_t seq_no;
    /* Pointer to cached copy of own LSP */
    isis_lsp_pkt_t *self_lsp_pkt;
    /* LSP DB */
    avltree_t lspdb_avl_root;
    /* Task to schedule self LSP pkt generation */
    task_t *lsp_pkt_gen_task;
    /* Timer to flood lsp packet periodically*/
    timer_event_handle *periodic_lsp_flood_timer;
    /* lsp pkt life time interval in lspdb */
    uint32_t lsp_lifetime_interval;
    /* lsp gen controlling flags - for purge lsp pkt*/
    uint8_t lsp_gen_flags;
    /* reconciliation handle */
    isis_reconcil_data_t reconcil;
    /* overload timer */
    timer_event_handle *lsp_overload_timer;
    /*Layer 2 Mapping */
    bool layer2_mapping;
}isis_node_info_t;

#define ISIS_NODE_INFO(node_ptr) (isis_node_info_t *)((node_ptr)->node_nw_prop.isis_node_info)
#define ISIS_INCREMENT_NODE_STATS(node_ptr, field)  ((ISIS_NODE_INFO(node_ptr))->field++)
#define ISIS_DECREMENT_NODE_STATS(node_ptr, field)  ((ISIS_NODE_INFO(node_ptr))->field--)

void isis_init(node_t *node);
void isis_de_init(node_t *node);

bool isis_is_protocol_enable_on_node(node_t *node);
void isis_show_node_protocol_state(node_t *node);
void isis_one_time_registration(void);

#endif