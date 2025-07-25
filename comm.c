#include "comm.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/select.h>
#include "glueThread/glthread.h"
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include "tcp_ip_trace.h"

static unsigned int udp_port_number = 40000;

static char recv_buffer[MAX_RECEIVE_BUFFER_SIZE];
static char send_buffer[MAX_SEND_BUFFER_SIZE];

static unsigned int get_next_udp_port_number(void);
static void *__network_start_pkt_receiver_thread(void *arg);
static void _pkt_receive(node_t *receiving_node, char *pkt_with_aux_data, unsigned int pkt_size);
static int _send_pkt_out(int sock_fd, char *pkt_data, unsigned int pkt_size, unsigned int dst_udp_port_no);

static unsigned int get_next_udp_port_number(void)
{
    return udp_port_number++;
}

void init_udp_socket(node_t *node)
{
    struct sockaddr_in node_addr;
    node_addr.sin_family = AF_INET;
    node_addr.sin_addr.s_addr = INADDR_ANY;
    node->udp_port_number = get_next_udp_port_number();
    node_addr.sin_port = node->udp_port_number;

    int udp_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(bind(udp_sock_fd, (const struct sockaddr *)&node_addr, sizeof(struct sockaddr_in)) == -1)
    {
        printf("socket bind failed for node %s\n", node->node_name);
        return;
    }
    node->udp_sock_fd = udp_sock_fd;
}
extern void layer2_frame_recv(node_t *node, interface_t *interface, char *pkt, unsigned int pkt_size);
static void _pkt_receive(node_t *receiving_node, char *pkt_with_aux_data, unsigned int pkt_size)
{
    char *recv_intf_name = pkt_with_aux_data;
    interface_t *recv_intf = get_node_intf_by_name(receiving_node, recv_intf_name);
    if(!recv_intf){
        printf("Error - %s:Packet received on unknown interface %s-%s\n", __FUNCTION__, receiving_node->node_name, recv_intf_name);
        return;
    }
    else{
        sprintf(tlb, "message received : %s, on node %s, by interface %s\n", (pkt_with_aux_data + IF_NAME_SIZE), \
                                                                        receiving_node->node_name, recv_intf_name);
        tcp_trace(receiving_node, recv_intf, tlb);
    }

    /*Interface is not Operationally Up*/
    if(!IF_IS_UP(recv_intf)){
        return;
    }
    recv_intf->intf_nw_prop.pkt_recv++;

    tcp_dump_recv_logger(receiving_node, recv_intf, (char *)(pkt_with_aux_data + IF_NAME_SIZE), (pkt_size - IF_NAME_SIZE), ETH_HDR);
    /* Right align the received data */
    char *pkt = pkt_buffer_shift_right((pkt_with_aux_data + IF_NAME_SIZE), (pkt_size - IF_NAME_SIZE), (MAX_RECEIVE_BUFFER_SIZE-IF_NAME_SIZE));

    layer2_frame_recv(receiving_node, recv_intf, pkt, (pkt_size - IF_NAME_SIZE));
}

static int _send_pkt_out(int sock_fd, char *pkt_data, unsigned int pkt_size, unsigned int dst_udp_port_no)
{
    int rc;
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = dst_udp_port_no;
    //printf("%s: sending the pkt out ....\n", __FUNCTION__);
    tcp_trace(NULL, NULL, "Sending the pkt out ....\n");
    #if 0 // not working
    int dst_addr_int;
    char *localhost = "127.0.0.1";
    inet_pton(AF_INET, localhost, &dst_addr_int);
    dst_addr_int = htonl(dst_addr_int);
    dest_addr.sin_addr.s_addr = dst_addr_int;
    #endif
    
    struct hostent *host = (struct hostent *)gethostbyname("127.0.0.1");
    dest_addr.sin_addr = *((struct in_addr *)host->h_addr);
    
    rc = sendto(sock_fd, pkt_data, pkt_size, 0, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
    sprintf(tlb, "%s: Frame sent using sendto() function, which returns %d\n", __FUNCTION__, rc);
    tcp_trace(NULL, NULL, tlb);

    return rc;
}

static void *__network_start_pkt_receiver_thread(void *arg)
{
    node_t *node;
    glthread_t *curr;
    fd_set active_sock_fd_set;
    fd_set backup_sock_fd_set;

    int sock_max_fd = 0;
    int bytes_recvd = 0;
    graph_t *topo = (graph_t *)arg;

    int opt = 1;
    struct sockaddr_in sender_addr;
    int addr_len = sizeof(struct sockaddr);

    FD_ZERO(&active_sock_fd_set);
    FD_ZERO(&backup_sock_fd_set);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr){
        node = graph_glue_to_node(curr);
        if(node->udp_sock_fd < 3)
            continue;
        if(node->udp_sock_fd > sock_max_fd)
            sock_max_fd = node->udp_sock_fd;
        FD_SET(node->udp_sock_fd, &backup_sock_fd_set);
        setsockopt(node->udp_sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
        setsockopt(node->udp_sock_fd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt));
    }ITERATE_GLTHREAD_END(&topo->node_list, curr);

    while(1){
        pthread_testcancel();
        memcpy(&active_sock_fd_set, &backup_sock_fd_set, sizeof(fd_set));
        select(sock_max_fd + 1, &active_sock_fd_set, NULL, NULL, NULL);

        ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr){
            node = graph_glue_to_node(curr);

            if(FD_ISSET(node->udp_sock_fd, &active_sock_fd_set)){
                memset(recv_buffer, 0, MAX_RECEIVE_BUFFER_SIZE);
                bytes_recvd = recvfrom(node->udp_sock_fd, (char *)recv_buffer, MAX_RECEIVE_BUFFER_SIZE, 0, \
                                        (struct sockaddr *)&sender_addr, &addr_len);
                _pkt_receive(node, recv_buffer, bytes_recvd);
            }
        }ITERATE_GLTHREAD_END(&topo->node_list, curr);
    }
}

void network_start_pkt_receiver_thread(graph_t *topo)
{
    pthread_attr_t attr;
    pthread_t recv_pkt_thread;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if(pthread_create(&recv_pkt_thread, &attr, __network_start_pkt_receiver_thread, (void *)topo)){
        printf("Thread creation failed with error code %d\n", errno);
        exit(0); /* cancel point */
    }
}

int send_pkt_out(char *pkt, unsigned int pkt_size, interface_t *interface)
{
    int rc = 0;
    node_t *sending_node = interface->att_node;
    node_t *nbr_node = get_nbr_node(interface);

    interface_t *other_interface = &interface->link->intf1 == interface ? &interface->link->intf2 : &interface->link->intf1;

    if(!IF_IS_UP(interface)){
        return 0;
    }

    if(!nbr_node)
        return -1;

    if (pkt_size + IF_NAME_SIZE > MAX_PACKET_BUFFER_SIZE)
    {
        printf("Error : Node :%s, Pkt Size exceeded\n", sending_node->node_name);
        return -1;
    }

    unsigned int dst_udp_port_no = nbr_node->udp_port_number;
    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock_fd < 3)
    {
        printf("Socket creation failed\n");
        return -1;
    }
    memset(send_buffer, 0, MAX_SEND_BUFFER_SIZE);

    char *pkt_with_aux_data = send_buffer;
    strncpy(pkt_with_aux_data, other_interface->if_name, IF_NAME_SIZE);
    pkt_with_aux_data[15] = '\0';
    memcpy(pkt_with_aux_data + IF_NAME_SIZE, pkt, pkt_size);
    sprintf(tlb, "%s: sending the pkt out ....\n", __FUNCTION__);
    tcp_trace(NULL, NULL, tlb);
    rc = _send_pkt_out(sock_fd, pkt_with_aux_data, pkt_size + IF_NAME_SIZE, dst_udp_port_no);
    close(sock_fd);

    if(rc > 0U){
        interface->intf_nw_prop.pkt_sent++;   
        tcp_dump_send_logger(sending_node, interface, pkt_with_aux_data + IF_NAME_SIZE, pkt_size, ETH_HDR);
    }
    else{
        printf("%s: Error - Pkt send failed on node %s with error code %d\n", __FUNCTION__, sending_node->node_name, errno);
    }
    return rc;
}

int send_pkt_to_self(char *pkt, uint32_t pkt_size, interface_t *interface){

    int rc = 0;    
    node_t *sending_node = interface->att_node;
    node_t *nbr_node = sending_node;
   
    if(!IF_IS_UP(interface)){
        return 0;
    }

    uint32_t dst_udp_port_no = nbr_node->udp_port_number;
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    if(sock < 0){
        printf("Error : Sending socket Creation failed , errno = %d", errno);
        return -1;
    }
    
    interface_t *other_interface =  interface;

    memset(NODE_SEND_BUFFER(sending_node), 0, MAX_PACKET_BUFFER_SIZE);

    char *pkt_with_aux_data = NODE_SEND_BUFFER(sending_node);

    strncpy(pkt_with_aux_data, other_interface->if_name, IF_NAME_SIZE);

    pkt_with_aux_data[IF_NAME_SIZE - 1] = '\0';

    memcpy(pkt_with_aux_data + IF_NAME_SIZE, pkt, pkt_size);

    rc = _send_pkt_out(sock, pkt_with_aux_data, pkt_size + IF_NAME_SIZE, 
                        dst_udp_port_no);

    if(rc > 0){
        tcp_dump_send_logger(sending_node, interface, 
            pkt_with_aux_data + IF_NAME_SIZE, pkt_size, ETH_HDR);
    }
    close(sock);
    return rc; 
       
}
int
send_pkt_flood(node_t *node, interface_t *exempted_intf, 
                char *pkt, uint32_t pkt_size){

    uint32_t i = 0;
    interface_t *intf; 

    for( ; i < MAX_IF_PER_NODE; i++){

        intf = node->intf[i];
        if(!intf) return 0;

        if(intf == exempted_intf)
            continue;

        send_pkt_out(pkt, pkt_size, intf);
    }
    return 0;
}
