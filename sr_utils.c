#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sr_protocol.h"
#include "sr_utils.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_if.h"

uint16_t cksum (const void *_data, int len) {
  const uint8_t *data = _data;
  uint32_t sum;

  /* add all pairs of bytes in the input buffer */
  for (sum = 0;len >= 2; data += 2, len -= 2)
    sum += data[0] << 8 | data[1];
  /* handle odd number of bytes in buffer */
  if (len > 0)
    sum += data[0] << 8;

  /* fold 32-bit sum into 16-bit var */
  while (sum > 0xffff)
    sum = (sum >> 16) + (sum & 0xffff);
  sum = htons (~sum);
  return sum;
}

uint16_t ethertype(uint8_t *buf) {
  sr_ethernet_hdr_t *ehdr = (sr_ethernet_hdr_t *)buf;
  return ntohs(ehdr->ether_type);
}

uint8_t ip_protocol(uint8_t *buf) {
  sr_ip_hdr_t *iphdr = (sr_ip_hdr_t *)(buf);
  return iphdr->ip_p;
}


/* Prints out formatted Ethernet address, e.g. 00:11:22:33:44:55 */
void print_addr_eth(uint8_t *addr) {
  int pos = 0;
  uint8_t cur;
  for (; pos < ETHER_ADDR_LEN; pos++) {
    cur = addr[pos];
    if (pos > 0)
      fprintf(stderr, ":");
    fprintf(stderr, "%02X", cur);
  }
  fprintf(stderr, "\n");
}

/* Prints out IP address as a string from in_addr */
void print_addr_ip(struct in_addr address) {
  char buf[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &address, buf, 100) == NULL)
    fprintf(stderr,"inet_ntop error on address conversion\n");
  else
    fprintf(stderr, "%s\n", buf);
}

/* Prints out IP address from integer value */
void print_addr_ip_int(uint32_t ip) {
  uint32_t curOctet = ip >> 24;
  fprintf(stderr, "%d.", curOctet);
  curOctet = (ip << 8) >> 24;
  fprintf(stderr, "%d.", curOctet);
  curOctet = (ip << 16) >> 24;
  fprintf(stderr, "%d.", curOctet);
  curOctet = (ip << 24) >> 24;
  fprintf(stderr, "%d\n", curOctet);
}


/* Prints out fields in Ethernet header. */
void print_hdr_eth(uint8_t *buf) {
  sr_ethernet_hdr_t *ehdr = (sr_ethernet_hdr_t *)buf;
  fprintf(stderr, "ETHERNET header:\n");
  fprintf(stderr, "\tdestination: ");
  print_addr_eth(ehdr->ether_dhost);
  fprintf(stderr, "\tsource: ");
  print_addr_eth(ehdr->ether_shost);
  fprintf(stderr, "\ttype: %d\n", ntohs(ehdr->ether_type));
}

/* Prints out fields in IP header. */
void print_hdr_ip(uint8_t *buf) {
  sr_ip_hdr_t *iphdr = (sr_ip_hdr_t *)(buf);
  fprintf(stderr, "IP header:\n");
  fprintf(stderr, "\tversion: %d\n", iphdr->ip_v);
  fprintf(stderr, "\theader length: %d\n", iphdr->ip_hl);
  fprintf(stderr, "\ttype of service: %d\n", iphdr->ip_tos);
  fprintf(stderr, "\tlength: %d\n", ntohs(iphdr->ip_len));
  fprintf(stderr, "\tid: %d\n", ntohs(iphdr->ip_id));

  if (ntohs(iphdr->ip_off) & IP_DF)
    fprintf(stderr, "\tfragment flag: DF\n");
  else if (ntohs(iphdr->ip_off) & IP_MF)
    fprintf(stderr, "\tfragment flag: MF\n");
  else if (ntohs(iphdr->ip_off) & IP_RF)
    fprintf(stderr, "\tfragment flag: R\n");

  fprintf(stderr, "\tfragment offset: %d\n", ntohs(iphdr->ip_off) & IP_OFFMASK);
  fprintf(stderr, "\tTTL: %d\n", iphdr->ip_ttl);
  fprintf(stderr, "\tprotocol: %d\n", iphdr->ip_p);

  /*Keep checksum in NBO*/
  /*fprintf(stderr, "\tchecksum: %d\n", ntohs(iphdr->ip_sum));*/
  fprintf(stderr, "\tchecksum: %d\n", iphdr->ip_sum);

  fprintf(stderr, "\tsource: ");
  print_addr_ip_int(ntohl(iphdr->ip_src));

  fprintf(stderr, "\tdestination: ");
  print_addr_ip_int(ntohl(iphdr->ip_dst));
}

/* Prints out ICMP header fields */
void print_hdr_icmp(uint8_t *buf) {
  sr_icmp_hdr_t *icmp_hdr = (sr_icmp_hdr_t *)(buf);
  fprintf(stderr, "ICMP header:\n");
  fprintf(stderr, "\ttype: %d\n", icmp_hdr->icmp_type);
  fprintf(stderr, "\tcode: %d\n", icmp_hdr->icmp_code);
  /* Keep checksum in NBO */
  fprintf(stderr, "\tchecksum: %d\n", icmp_hdr->icmp_sum);
}


/* Prints out fields in ARP header */
void print_hdr_arp(uint8_t *buf) {
  sr_arp_hdr_t *arp_hdr = (sr_arp_hdr_t *)(buf);
  fprintf(stderr, "ARP header\n");
  fprintf(stderr, "\thardware type: %d\n", ntohs(arp_hdr->ar_hrd));
  fprintf(stderr, "\tprotocol type: %d\n", ntohs(arp_hdr->ar_pro));
  fprintf(stderr, "\thardware address length: %d\n", arp_hdr->ar_hln);
  fprintf(stderr, "\tprotocol address length: %d\n", arp_hdr->ar_pln);
  fprintf(stderr, "\topcode: %d\n", ntohs(arp_hdr->ar_op));

  fprintf(stderr, "\tsender hardware address: ");
  print_addr_eth(arp_hdr->ar_sha);
  fprintf(stderr, "\tsender ip address: ");
  print_addr_ip_int(ntohl(arp_hdr->ar_sip));

  fprintf(stderr, "\ttarget hardware address: ");
  print_addr_eth(arp_hdr->ar_tha);
  fprintf(stderr, "\ttarget ip address: ");
  print_addr_ip_int(ntohl(arp_hdr->ar_tip));
}

/* Prints out all possible headers, starting from Ethernet */
void print_hdrs(uint8_t *buf, uint32_t length) {

  /* Ethernet */
  int minlength = sizeof(sr_ethernet_hdr_t);
  if (length < minlength) {
    fprintf(stderr, "Failed to print ETHERNET header, insufficient length\n");
    return;
  }

  uint16_t ethtype = ethertype(buf);
  print_hdr_eth(buf);

  if (ethtype == ethertype_ip) { /* IP */
    minlength += sizeof(sr_ip_hdr_t);
    if (length < minlength) {
      fprintf(stderr, "Failed to print IP header, insufficient length\n");
      return;
    }

    print_hdr_ip(buf + sizeof(sr_ethernet_hdr_t));
    uint8_t ip_proto = ip_protocol(buf + sizeof(sr_ethernet_hdr_t));

    if (ip_proto == ip_protocol_icmp) { /* ICMP */
      minlength += sizeof(sr_icmp_hdr_t);
      if (length < minlength)
        fprintf(stderr, "Failed to print ICMP header, insufficient length\n");
      else
        print_hdr_icmp(buf + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    }
  }
  else if (ethtype == ethertype_arp) { /* ARP */
    minlength += sizeof(sr_arp_hdr_t);
    if (length < minlength)
      fprintf(stderr, "Failed to print ARP header, insufficient length\n");
    else
      print_hdr_arp(buf + sizeof(sr_ethernet_hdr_t));
  }
  else {
    fprintf(stderr, "Unrecognized Ethernet Type: %d\n", ethtype);
  }
}

/* My helper functions start here */
struct sr_if *find_dst_if(struct sr_instance *sr, uint32_t dst){
    struct sr_rt* temp = sr->routing_table;
    while(temp) {
      uint32_t dst_ = temp->mask.s_addr & dst;
      if(dst_ == temp->dest.s_addr)
        return sr_get_interface(sr, temp->interface);
      temp = temp->next;
    }
    return NULL;
}

int sr_send_reply(struct sr_instance *sr, sr_ethernet_hdr_t *req_e_hdr,
  sr_arp_hdr_t *req_a_hdr, struct sr_if* iface){
   unsigned int len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
   /* allocate memory to packet */
   uint8_t *packet = (uint8_t *)malloc(len);
   bzero(packet, len);
   sr_ethernet_hdr_t *rep_e_hdr = get_eth_hdr(packet);
   sr_arp_hdr_t *rep_a_hdr = get_arp_hdr(packet);
   memcpy(rep_e_hdr->ether_dhost, req_e_hdr->ether_shost, ETHER_ADDR_LEN);
   memcpy(rep_e_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
   rep_e_hdr->ether_type = ntohs(ethertype_arp);
   rep_a_hdr->ar_hln = req_a_hdr->ar_hln;
   rep_a_hdr->ar_pln = req_a_hdr->ar_pln;
   rep_a_hdr->ar_op = htons(arp_op_reply);
   rep_a_hdr->ar_pro = req_a_hdr->ar_pro;
   rep_a_hdr->ar_hrd = req_a_hdr->ar_hrd;
   memcpy(rep_a_hdr->ar_tha, req_a_hdr->ar_sha, ETHER_ADDR_LEN);
   memcpy(rep_a_hdr->ar_sha, iface->addr, ETHER_ADDR_LEN);
   rep_a_hdr->ar_sip = iface->ip;
   rep_a_hdr->ar_tip = req_a_hdr->ar_sip;
   printf("\nSending reply\n");
   int res = sr_send_packet(sr, packet, len, iface->name);
   return res;
}

int sr_send_request(struct sr_instance *sr, uint32_t tip){
  unsigned int len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
  /* allocate memory to packet */
  uint8_t *packet = (uint8_t *)malloc(len);
  bzero(packet, len);

  struct sr_if *iface = find_dst_if(sr, tip);
  struct sr_ethernet_hdr *e_hdr = get_eth_hdr(packet);
  struct sr_arp_hdr *a_hdr = get_arp_hdr(packet);

  memset(e_hdr->ether_dhost, 0xff, ETHER_ADDR_LEN);
  memcpy(e_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
  e_hdr->ether_type = htons(ethertype_arp);

  a_hdr->ar_hln = ETHER_ADDR_LEN;
  a_hdr->ar_pln = 4;
  a_hdr->ar_op = htons(arp_op_request);
  a_hdr->ar_pro = htons(ethertype_ip);
  a_hdr->ar_hrd = htons(arp_hrd_ethernet);
  memset(a_hdr->ar_tha, 0xff, ETHER_ADDR_LEN);
  memcpy(a_hdr->ar_sha, iface->addr, ETHER_ADDR_LEN);
  a_hdr->ar_sip = iface->ip;
  a_hdr->ar_tip = tip;

  int res = sr_send_packet(sr, packet, len, iface->name);
  return res;
}

int sr_send_icmp_t0(struct sr_instance *sr, uint8_t *packet, uint8_t icmp_type,
  uint8_t icmp_code, unsigned int len, struct sr_if *iface){
  sr_ethernet_hdr_t *eth_hdr = get_eth_hdr(packet);
  sr_ip_hdr_t *ip_hdr = get_ip_hdr(packet);
  sr_icmp_hdr_t *icmp_hdr = get_icmp_hdr(packet);

  /*  */
  memcpy(eth_hdr->ether_dhost, eth_hdr->ether_shost, ETHER_ADDR_LEN);
  /* Get the interface using its destination IP address */
  struct sr_if *iface_ = find_dst_if(sr, ip_hdr->ip_src);
  memcpy(eth_hdr->ether_shost, iface_->addr, ETHER_ADDR_LEN);

  uint32_t temp = ip_hdr->ip_src;
  ip_hdr->ip_src = iface->ip;
  /* swap destination and source */
  ip_hdr->ip_dst = temp;
  icmp_hdr->icmp_type = icmp_type;
  icmp_hdr->icmp_code = icmp_code;
  /* Again, set checksum to 0 first */
  icmp_hdr->icmp_sum = 0;
  /* Then compute the correct checksum */
  icmp_hdr->icmp_sum = cksum(icmp_hdr, sizeof(sr_icmp_hdr_t));

  int res = sr_send_packet(sr, packet, len, iface_->name);
  return res;
  }

int sr_send_icmp_t3(struct sr_instance *sr, uint8_t icmp_type,
uint8_t icmp_code, uint8_t *rcvd_packet, struct sr_if *iface){
/* Calculate length of header */
unsigned int len = sizeof(sr_ethernet_hdr_t) +sizeof(sr_ip_hdr_t)
+ sizeof(sr_icmp_t3_hdr_t);
  /* Allocate memory space for ICMP packet */
  uint8_t *packet = (uint8_t *)malloc(len);
  bzero(packet, len);
  /* Assign new Headers */
  sr_ethernet_hdr_t *e_hdr = get_eth_hdr(packet);
  sr_ip_hdr_t *ip_hdr = get_ip_hdr(packet);
  sr_icmp_t3_hdr_t *icmp_hdr = get_icmp_t3_hdr(packet);
  /* Extract imformation from received data packets */
  sr_ip_hdr_t *rec_ip_hdr = get_ip_hdr(rcvd_packet);
  sr_ethernet_hdr_t *rec_eth_hdr = get_eth_hdr(rcvd_packet);
  /* Use helper function to find outgoing interface by looking up route table */
  struct sr_if *new_iface = find_dst_if(sr, rec_ip_hdr->ip_src);
  /* Assigning values to ethernet headers */
  memcpy(e_hdr->ether_dhost, rec_eth_hdr->ether_shost, ETHER_ADDR_LEN);
  e_hdr->ether_type = htons(ethertype_ip);
  memcpy(e_hdr->ether_shost, new_iface->addr, ETHER_ADDR_LEN);
  /* Assigning values to IP headers */
  /* set the packet id to equal the received packet id */
  ip_hdr->ip_id = rec_ip_hdr->ip_id;
  ip_hdr->ip_hl = rec_ip_hdr->ip_hl;
  ip_hdr->ip_p = ip_protocol_icmp;
  ip_hdr->ip_tos = rec_ip_hdr->ip_tos;
  ip_hdr->ip_v = rec_ip_hdr->ip_v;
  ip_hdr->ip_src = iface->ip;
  ip_hdr->ip_off = htons(IP_DF);
  ip_hdr->ip_ttl = INIT_TTL;
  ip_hdr->ip_sum = 0;
  ip_hdr->ip_sum = cksum(ip_hdr, sizeof(sr_ip_hdr_t));
  ip_hdr->ip_dst = rec_ip_hdr->ip_src;
  ip_hdr->ip_len = htons(len - sizeof(sr_ethernet_hdr_t));
  /* Assigning values to ICMP headers */
  icmp_hdr->icmp_type = icmp_type;
  icmp_hdr->icmp_code = icmp_code;
  memcpy(icmp_hdr->data, rec_ip_hdr, ICMP_DATA_SIZE);
  icmp_hdr->icmp_sum = 0;
  icmp_hdr->icmp_sum = cksum(icmp_hdr, sizeof(sr_icmp_t3_hdr_t));
  /* Send this new constructed ICMP type 3 packet */
  int res = sr_send_packet(sr, packet, len, new_iface->name);
  return res;
}

void sr_forward_packet(struct sr_instance *sr, uint8_t *packet,
  unsigned int len, struct sr_if *iface, uint8_t* mac) {

  sr_ethernet_hdr_t *e_hdr = get_eth_hdr(packet);
  sr_ip_hdr_t *ip_hdr = get_ip_hdr(packet);
  memcpy(e_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
  memcpy(e_hdr->ether_dhost, mac, ETHER_ADDR_LEN);
  /* We have to set ip_sum to 0 before we compute new sum */
  /* If we don't do that, the ip_sum will be 0 not matter how to calculate it */
  ip_hdr->ip_sum = 0;
  /* Compute checksum */
  ip_hdr->ip_sum = cksum((const void *)ip_hdr, sizeof(sr_ip_hdr_t));
  printf("\nChecksum of that packet is: %d\n",ip_hdr->ip_sum);
  printf("\nForwarding Packet");
  sr_send_packet(sr, packet, len, iface->name);
  }

void sr_forwarding (struct sr_instance *sr, uint8_t *packet,
  unsigned int len, struct sr_if *iface) {
    sr_ip_hdr_t *ip_hdr = get_ip_hdr(packet);
    struct sr_if *iface_found = find_dst_if(sr, ip_hdr->ip_dst);
    /* if we cannot find a interface for the destination ip */
    if(iface_found == NULL){
      printf("No interfaces found for this destination ip, sending ICMP\n");
      sr_send_icmp_t3(sr, icmp_type_dest_unreach,
      icmp_code_net_unreach, packet, iface);
    }
    /* if we can find interface */
    else
      {
        sr_arpentry_t* dst_entry = sr_arpcache_lookup(&sr->cache, ip_hdr->ip_dst);
        if (dst_entry==NULL)
        {
          sr_arpreq_t* new_req = sr_arpcache_queuereq(&sr->cache,
            ip_hdr->ip_dst, packet, len, iface_found->name);
          handle_arpreq(sr,new_req);
          return;
        }
        else
        {
          /* forward the packet */
          sr_forward_packet(sr, packet, len, iface_found, dst_entry->mac);
          free(dst_entry);
          printf("Forwarding successfully\n");
          return;
        }
    }
  }

sr_ethernet_hdr_t *get_eth_hdr(uint8_t *packet) {
  return (sr_ethernet_hdr_t *)packet;
}

sr_arp_hdr_t *get_arp_hdr(uint8_t *packet) {
  return (sr_arp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
}

sr_ip_hdr_t *get_ip_hdr(uint8_t *packet) {
  return (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
}

sr_icmp_hdr_t *get_icmp_hdr(uint8_t *packet) {
  return (sr_icmp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
}

sr_icmp_t3_hdr_t *get_icmp_t3_hdr(uint8_t *packet) {
  return (sr_icmp_t3_hdr_t *)get_icmp_hdr(packet);
}
/* Sanity check */

int sanity_check_arp(unsigned int len) {
  if ( len >= (sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t)) )
      return 1;
  else
      return 0;
}

int sanity_check_ip(unsigned int len) {
  if ( len >= (sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)) )
      return 1;
  else
      return 0;
}

int sanity_check_icmp(unsigned int len) {
  int check = len >= (sizeof(sr_ethernet_hdr_t) +
      sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_hdr_t));
  return check;
}

/* Returns 1 if equal */
int check_ip_chksum(sr_ip_hdr_t *ip_hdr) {
  uint16_t tmp_sum = ip_hdr->ip_sum;
  ip_hdr->ip_sum = 0;

  if(cksum(ip_hdr, sizeof(sr_ip_hdr_t)) == tmp_sum) {
    ip_hdr->ip_sum = tmp_sum;
    return 1;
  }
  else {
    ip_hdr->ip_sum = tmp_sum;
    return 0;
  }
}

int check_icmp_chksum(uint16_t len, sr_icmp_hdr_t *icmp_hdr) {
  uint16_t tmp_sum = icmp_hdr->icmp_sum;
  icmp_hdr->icmp_sum = 0;

  if(cksum((uint8_t *)icmp_hdr, ntohs(len) - sizeof(sr_ip_hdr_t)) == tmp_sum) {
    icmp_hdr->icmp_sum = tmp_sum;
    return 1;
  }
  else {
    icmp_hdr->icmp_sum = tmp_sum;
    return 0;
  }
}
