/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/* TODO: Add constant definitions here... */

/* TODO: Add helper functions here... */

/* See pseudo-code in sr_arpcache.h */
void handle_arpreq(struct sr_instance* sr, struct sr_arpreq *req){
  /* TODO: Fill this in */

}

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

    /* TODO: (opt) Add initialization code here */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT free either (signified by "lent" comment).
 * Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

 /*
 struct sr_instance
 {
     int  sockfd;    socket to server
     char user[32];  user name
     char host[32];  host name
     char template[30];  template name if any
     unsigned short topo_id;
     struct sockaddr_in sr_addr;  address to server
     struct sr_if* if_list;  list of interfaces
     struct sr_rt* routing_table;  routing table
     struct sr_arpcache cache;    ARP cache
     pthread_attr_t attr;
     FILE* logfile;
 };
 */


void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */){


  /*len = data length
    uint8_t is 8bits integer
    interface store which interface comes from
  */

  /* REQUIRES */
  assert(sr);
  assert(packet);
  /*assert(len); */
  assert(interface);


  printf("*** -> Received packet of length %d\n",len);

  /* TODO: Add forwarding logic here */
  /* uint32_t sum = cksum (const void *_data, int len); */
  /* struct if_tt*           arp_table = 0; */
  struct sr_if *iface = sr_get_interface(sr, interface);
  assert(iface);

  sr_ethernet_hdr_t *e_hdr = get_eth_hdr(packet);
  sr_arp_hdr_t *a_hdr = get_arp_hdr(packet);
  /* a_hdr = get_arp_hdr(packet); */
  /* printf("\nEthernet header is: %d, ARP header is: %d", e_hdr->ether_type,
   a_hdr); */
  uint16_t type_ = ntohs(e_hdr->ether_type);
  switch(type_){
/*------------------------------------------------------------------------------*/
    case ethertype_arp:
      printf("\nARP, Packet type: %d, Arp type: %d",type_,ethertype_arp);
      if(!sanity_check_arp(len)){
        printf("\nSanity check failed, ARP packet dropped.");
        break;
      }
      handle_arp(sr, e_hdr, a_hdr, iface);
      /*sr_handlearp(sr, packet, len, iface); */
      break;
/*------------------------------------------------------------------------------*/
    case ethertype_ip:
      printf("\n received IP packet");
      sr_ip_hdr_t *ip_header= get_ip_hdr(packet);
      /*print_hdr_ip(ip_header);*/
      uint8_t ip_type=ip_header->ip_p;

      switch(ip_type)
      {
        case ip_protocol_icmp:
          /*printf("\nICMP, Packet type: %d, IP type: %d",type_,ethertype_ip);*/

          printf("\n This is a ICMP packet");

          handle_ICMP(sr, e_hdr, len, iface);
          break;
        /*----------------------------------------------------------------------*/
        default:
          printf("\nThis is a IP packet");
          handle_IP(sr, e_hdr, a_hdr, iface);
      }
      break;
/*------------------------------------------------------------------------------*/
    default:
      printf("\nUnknown Packet type. Packet dropped.");
      printf("\nPacket type: %d,IP type: %d, ARP type: %d",type_,ethertype_ip,ethertype_arp);
      break;
  }

}/* -- sr_handlepacket -- */

void handle_ICMP(struct sr_instance* sr, uint8_t *packet, unsigned int len, struct sr_if *iface)
{
  sr_icmp_hdr_t* icmp_header=get_icmp_hdr(packet);
  sr_ip_hdr_t* ip_header=get_ip_hdr(packet);
  sr_ethernet_hdr_t* eth_header=get_eth_hdr(packet);

  sr_arpentry_t* dst_entry = sr_arpcache_lookup(&sr->cache, ip_header->ip_dst);
  if (dst_entry==NULL) {
    sr_arpreq_t* new_req = sr_arpcache_queuereq(&sr->cache, ip_header->ip_dst, packet, len, iface->name);
  }
  else
  {
    /*printf("=============================found MAC\n");
    printf("Sender MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
    dst_entry->mac[0] & 0xff, dst_entry->mac[1] & 0xff, dst_entry->mac[2] & 0xff,
    dst_entry->mac[3] & 0xff, dst_entry->mac[4] & 0xff, dst_entry->mac[5] & 0xff);

    printf("%s\n",iface->name);*/

    sr_if_t* dst_iface=find_dst_if(sr, ip_header->ip_dst);

    sr_forward_packet(sr, packet, len, dst_iface, dst_entry->mac);
    printf("Sending successfully\n");
  }

}
void handle_IP(struct sr_instance* sr, uint8_t *packet, unsigned int len, struct sr_if *iface)
{
  sr_ip_hdr_t *ip_hdr = get_ip_hdr(packet);
  if(!sanity_check_ip(len)) {
    printf("Sanity check for IP failed. Dropping\n");
  }
  if(!check_ip_chksum(ip_hdr)){
    printf("Checksum check for IP failed. Dropping\n");
  }
  struct sr_if *temp = sr->if_list;
  while(temp){
    /* Check if it matches one of our interfaces */
    if(temp->ip == ip_hdr->ip_dst){
      printf("Got a IP packet from interface: %s\n",temp->name);
      /* handle it */
    }
    temp = temp->next;
  }
  /* forward it if it is not destined to us */
  printf("This IP packet is not for us, forwarding it\n");
  /* decrement TTL, if TTL still > 0, forward it */
  ip_hdr->ip_ttl --;
  if(ip_hdr->ip_ttl == 0){
    printf("TTL is decremented to be 0, sending TTL expired ICMP\n");
    /* send ICM type 3 message here */
    return;
  }
  /* we'll forward packet here */
}



void handle_arp(struct sr_instance* sr, uint8_t *packet, unsigned int len,
  struct sr_if *iface){
      sr_ethernet_hdr_t *e_hdr = get_eth_hdr(packet);
      sr_arp_hdr_t *a_hdr = get_arp_hdr(packet);
      printf("\nAn ARP packet received! Validating...");
      switch(ntohs(a_hdr->ar_op)) {
        /* Handle ARP request */
        case arp_op_request:
          printf("\nThis ARP is correct! Processing...");
          /*firstly should check the arp cache using sr_arpcache_lookup
          */

          sr_arpcache_insert(&sr->cache, a_hdr->ar_sha, a_hdr->ar_sip);
          sr_send_reply(sr,e_hdr, a_hdr, iface);

          /*printf("Sender MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
          a_hdr->ar_sha[0] & 0xff, a_hdr->ar_sha[1] & 0xff, a_hdr->ar_sha[2] & 0xff,
          a_hdr->ar_sha[3] & 0xff, a_hdr->ar_sha[4] & 0xff, a_hdr->ar_sha[5] & 0xff);*/


          break;
        /* Handle ARP reply */
        case arp_op_reply:
          printf("\nAn ARP reply received!");
          /* if (a_hdr->ar_tip == iface->ip) */
          pthread_mutex_lock(&sr->cache.lock);
          sr_arpreq_t *req = sr_arpcache_insert(&sr->cache, a_hdr->ar_sha, a_hdr->ar_sip);
          if(a_hdr->ar_tip != iface->ip){
            /* If the ARP reply is not for us */
            printf("We are not the destination of that ARP reply packet\n");
            break;
          }
          if (req!=NULL)
          {
            sr_packet_t *pkg=req->packets;
            printf("###$#$#$#$#$\n");
            while (pkg!=NULL)
            {
              sr_if_t* dst_iface=find_dst_if(sr, req->ip);
              /* Since we requested the MAC address from the sender of ARP reply */
              /* We will need to send the IP packet waiting for that MAC address */
              /* To send this packet, we'll use the same interface as reply packet */
              /* printf("\nDestination Interface found is : %s", dst_iface->name);
              printf("\nDestination Interface in packet is : %s", iface->name);*/
              sr_forward_packet(sr, pkg->buf, pkg->len, iface,
                a_hdr->ar_sha);
              pkg=pkg->next;
              printf("sending pkg \n");
            }
            printf("Sending pkg finished\n");
            /*sr_arpreq_destroy(&sr->cache, req);*/
          }
          pthread_mutex_unlock(&sr->cache.lock);
          /*printf("Sender MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
          a_hdr->ar_sha[0] & 0xff, a_hdr->ar_sha[1] & 0xff, a_hdr->ar_sha[2] & 0xff,
          a_hdr->ar_sha[3] & 0xff, a_hdr->ar_sha[4] & 0xff, a_hdr->ar_sha[5] & 0xff);*/



          /*should save into request queue*/

          /*The ARP reply processing code should move entries from the ARP request
          queue to the ARP cache:

          # When servicing an arp reply that gives us an IP->MAC mapping
          req = arpcache_insert(ip, mac)

          if req:
              send all packets on the req->packets linked list
              arpreq_destroy(req)*/
          break;
        default:
          printf("\nCannot recognize this ARP frame");
          return;
      }
  }
