#include "net.h"

#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "file.h"
#include "fs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "types.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};

static struct port ports[MAX_PORTS_COUNT];

void netinit(void) {
  // initialize network here

  for (int i = 0; i < MAX_PORTS_COUNT; i++) {
    initlock(&ports[i].lock, "port_lock");
    ports[i].free = 1;
  }
}

//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64 sys_bind(void) {
  int port_id;

  argint(0, &port_id);

  acquire(&ports[port_id].lock);

  if (!ports[port_id].free) {
    release(&ports[port_id].lock);
    return -1;
  }
  ports[port_id].free = 0;
  ports[port_id].count = 0;
  ports[port_id].head = 0;
  release(&ports[port_id].lock);
  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64 sys_unbind(void) {
  int d_port;
  argint(0, &d_port);

  acquire(&ports[d_port].lock);

  if (ports[d_port].free) {
    release(&ports[d_port].lock);
    return -1;
  }

  for (int i = 0; i < ports[d_port].count; i++) {
    kfree(ports[d_port]
              .packets[(ports[d_port].head + i) % MAX_PACKETS_COUNT]
              .payload);
  }

  ports[d_port].free = 1;
  ports[d_port].head = 0;
  ports[d_port].count = 0;
  release(&ports[d_port].lock);
  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64 sys_recv(void) {
  int dport;
  uint64 src_addr, sport_addr, buf_addr;
  int maxlen;

  argint(0, &dport);
  argaddr(1, &src_addr);
  argaddr(2, &sport_addr);
  argaddr(3, &buf_addr);
  argint(4, &maxlen);

  acquire(&ports[dport].lock);

  if (ports[dport].free) {
    release(&ports[dport].lock);
    return -1;
  }

  while (ports[dport].count == 0) {
    sleep(&ports[dport], &ports[dport].lock);
  }

  if (ports[dport].free) {
    release(&ports[dport].lock);
    return -1;
  }

  struct packet *current_packet = &ports[dport].packets[ports[dport].head];

  int len = maxlen < current_packet->length ? maxlen : current_packet->length;

  struct proc *proc = myproc();
  if (copyout(proc->pagetable, src_addr, (char *)&current_packet->ip,
              sizeof(uint32)) < 0 ||
      copyout(proc->pagetable, sport_addr, (char *)&current_packet->port,
              sizeof(uint16)) < 0 ||
      copyout(proc->pagetable, buf_addr, current_packet->payload, len) < 0) {
    release(&ports[dport].lock);
    return -1;
  }

  kfree(current_packet->payload);
  ports[dport].head = (ports[dport].head + 1) % MAX_PACKETS_COUNT;
  ports[dport].count--;

  release(&ports[dport].lock);
  return len;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short in_cksum(const unsigned char *addr, int len) {
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64 sys_send(void) {
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);
  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if (total > PGSIZE) return -1;

  char *buf = kalloc();
  if (buf == 0) {
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *)buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45;  // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if (copyin(p->pagetable, payload, bufaddr, len) < 0) {
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void ip_rx(char *buf, int len) {
  // don't delete this printf
  static int seen_ip = 0;
  if (seen_ip == 0) printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  if (len < sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp)) {
    return;
  }

  struct ip *ip = (struct ip *)(((struct eth *)buf) + 1);

  if (ip->ip_p != IPPROTO_UDP) {
    return;
  }

  struct udp *udp = (struct udp *)(ip + 1);
  uint16 d_port = ntohs(udp->dport);

  acquire(&ports[d_port].lock);

  if (ports[d_port].free) {
    release(&ports[d_port].lock);
    return;
  }

  if (ports[d_port].count >= MAX_PACKETS_COUNT) {
    release(&ports[d_port].lock);
    return;
  }

  struct packet *current_packets =
      &ports[d_port].packets[(ports[d_port].head + ports[d_port].count) %
                             MAX_PACKETS_COUNT];

  if (!((current_packets->payload = kalloc()))) {
    release(&ports[d_port].lock);
    return;
  }

  current_packets->length = ntohs(udp->ulen) - sizeof(struct udp);
  current_packets->ip = ntohl(ip->ip_src);
  current_packets->port = ntohs(udp->sport);

  memmove(current_packets->payload, (char *)(udp + 1), current_packets->length);

  ports[d_port].count++;

  release(&ports[d_port].lock);

  wakeup(&ports[d_port]);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void arp_rx(char *inbuf) {
  static int seen_arp = 0;

  if (seen_arp) {
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *)inbuf;
  struct arp *inarp = (struct arp *)(ineth + 1);

  char *buf = kalloc();
  if (buf == 0) panic("send_arp_reply");

  struct eth *eth = (struct eth *)buf;
  memmove(eth->dhost, ineth->shost,
          ETHADDR_LEN);  // ethernet destination = query source
  memmove(eth->shost, local_mac,
          ETHADDR_LEN);  // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));
}

void net_rx(char *buf, int len) {
  struct eth *eth = (struct eth *)buf;

  if (len >= sizeof(struct eth) + sizeof(struct arp) &&
      ntohs(eth->type) == ETHTYPE_ARP) {
    arp_rx(buf);
  } else if (len >= sizeof(struct eth) + sizeof(struct ip) &&
             ntohs(eth->type) == ETHTYPE_IP) {
    ip_rx(buf, len);
  }
  kfree(buf);
}
