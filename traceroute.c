/*-
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * traceroute host  - trace the route ip packets follow going to "host".
 * Notes
 * -----
 * This program must be run by root or be setuid.  (I suggest that
 * you *don't* make it setuid -- casual use could result in a lot
 * of unnecessary traffic on our poor, congested nets.)
 *
 * I stole the idea for this program from Steve Deering.  Since
 * the first release, I've learned that had I attended the right
 * IETF working group meetings, I also could have stolen it from Guy
 * Almes or Matt Mathis.  I don't know (or care) who came up with
 * the idea first.  I envy the originators' perspicacity and I'm
 * glad they didn't keep the idea a secret.
 *
 * Tim Seaver, Ken Adelman and C. Philip Wood provided bug fixes and/or
 * enhancements to the original distribution.
 *
 * I've hacked up a round-trip-route version of this that works by
 * sending a loose-source-routed udp datagram through the destination
 * back to yourself.  Unfortunately, SO many gateways botch source
 * routing, the thing is almost worthless.  Maybe one day...
 *
 *  -- Van Jacobson (van@helios.ee.lbl.gov)
 *     Tue Dec 20 03:50:13 PST 1988
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <endian.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>


#define MAXPACKET       65535   /* max ip packet size */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN  64
#endif

/*
 * format of a (udp) probe packet.
 */
struct opacket {
	struct ip ip;
	struct udphdr udp;
	u_char seq;             /* sequence number of this packet */
	u_char ttl;             /* ttl packet left with */
	struct timeval tv;      /* time packet left */
};

/*
 * Definitions for internet protocol version 4.
 * Per RFC 791, September 1981.
 */
#define IPVERSION       4


#include "busybox.h"

static int wait_for_reply (int, struct sockaddr_in *, int);
static void send_probe (int, int);
static double deltaT (struct timeval *, struct timeval *);
static int packet_ok (u_char *, int, struct sockaddr_in *, int);
static void print (u_char *, int, struct sockaddr_in *);
static char *inetname (struct in_addr);

static u_char  packet[512];            /* last inbound (icmp) packet */
static struct opacket  *outpacket;     /* last output (udp) packet */

static int s;                          /* receive (icmp) socket file descriptor */
static int sndsock;                    /* send (udp) socket file descriptor */

static struct sockaddr whereto;        /* Who to try to reach */
static int datalen;                    /* How much data */

static char *hostname;

static int max_ttl = 30;
static u_short ident;
static u_short port = 32768+666;       /* start udp dest port # for probe packets */

static int verbose;
static int waittime = 5;               /* time to wait for response (in seconds) */
static int nflag;                      /* print addresses numerically */

int
#ifndef BB_TRACEROUTE
main(argc, argv)
#else
traceroute_main(argc, argv)
#endif
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	struct hostent *hp;
	struct protoent *pe;
	struct sockaddr_in from, *to;
	int ch, i, on, probe, seq, tos, ttl;

	int options = 0;                /* socket options */
	char *source = 0;
	int nprobes = 3;

	on = 1;
	seq = tos = 0;
	to = (struct sockaddr_in *)&whereto;
	while ((ch = getopt(argc, argv, "dm:np:q:rs:t:w:v")) != EOF)
		switch(ch) {
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'm':
			max_ttl = atoi(optarg);
			if (max_ttl <= 1)
				error_msg_and_die("max ttl must be >1.");
			break;
		case 'n':
			nflag++;
			break;
		case 'p':
			port = atoi(optarg);
			if (port < 1)
				error_msg_and_die("port must be >0.");
			break;
		case 'q':
			nprobes = atoi(optarg);
			if (nprobes < 1)
				error_msg_and_die("nprobes must be >0.");
			break;
		case 'r':
			options |= SO_DONTROUTE;
			break;
		case 's':
			/*
			 * set the ip source address of the outbound
			 * probe (e.g., on a multi-homed host).
			 */
			source = optarg;
			break;
		case 't':
			tos = atoi(optarg);
			if (tos < 0 || tos > 255)
				error_msg_and_die("tos must be 0 to 255.");
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			waittime = atoi(optarg);
			if (waittime <= 1)
				error_msg_and_die("wait must be >1 sec.");
			break;
		default:
			show_usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		show_usage();

	setlinebuf (stdout);

	(void) bzero((char *)&whereto, sizeof(struct sockaddr));
	to->sin_family = AF_INET;
	to->sin_addr.s_addr = inet_addr(*argv);
	if (to->sin_addr.s_addr != -1)
		hostname = *argv;
	else {
		hp = gethostbyname(*argv);
		if (hp) {
			to->sin_family = hp->h_addrtype;
			bcopy(hp->h_addr, (caddr_t)&to->sin_addr, hp->h_length);
			hostname = (char *)hp->h_name;
		} else {
			error_msg_and_die("unknown host %s", *argv);
		}
	}
	if (*++argv)
		datalen = atoi(*argv);
	if (datalen < 0 || datalen >= MAXPACKET - sizeof(struct opacket))
		error_msg_and_die("packet size must be 0 <= s < %d.",
		    MAXPACKET - sizeof(struct opacket));
	datalen += sizeof(struct opacket);
	outpacket = (struct opacket *)xmalloc((unsigned)datalen);
	(void) bzero((char *)outpacket, datalen);
	outpacket->ip.ip_dst = to->sin_addr;
	outpacket->ip.ip_tos = tos;
	outpacket->ip.ip_v = IPVERSION;
	outpacket->ip.ip_id = 0;

	ident = (getpid() & 0xffff) | 0x8000;

	if ((pe = getprotobyname("icmp")) == NULL)
		error_msg_and_die("icmp: unknown protocol");
	if ((s = socket(AF_INET, SOCK_RAW, pe->p_proto)) < 0)
		perror_msg_and_die("icmp socket");
	if (options & SO_DEBUG)
		(void) setsockopt(s, SOL_SOCKET, SO_DEBUG,
				  (char *)&on, sizeof(on));
	if (options & SO_DONTROUTE)
		(void) setsockopt(s, SOL_SOCKET, SO_DONTROUTE,
				  (char *)&on, sizeof(on));

	if ((sndsock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		perror_msg_and_die("raw socket");
#ifdef SO_SNDBUF
	if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&datalen,
		       sizeof(datalen)) < 0)
		perror_msg_and_die("SO_SNDBUF");
#endif SO_SNDBUF
#ifdef IP_HDRINCL
	if (setsockopt(sndsock, IPPROTO_IP, IP_HDRINCL, (char *)&on,
		       sizeof(on)) < 0)
		perror_msg_and_die("IP_HDRINCL");
#endif IP_HDRINCL
	if (options & SO_DEBUG)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DEBUG,
				  (char *)&on, sizeof(on));
	if (options & SO_DONTROUTE)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DONTROUTE,
				  (char *)&on, sizeof(on));

	if (source) {
		(void) bzero((char *)&from, sizeof(struct sockaddr));
		from.sin_family = AF_INET;
		from.sin_addr.s_addr = inet_addr(source);
		if (from.sin_addr.s_addr == -1)
			error_msg_and_die("unknown host %s", source);
		outpacket->ip.ip_src = from.sin_addr;
#ifndef IP_HDRINCL
		if (bind(sndsock, (struct sockaddr *)&from, sizeof(from)) < 0)
			perror_msg_and_die("bind");
#endif IP_HDRINCL
	}

	fprintf(stderr, "traceroute to %s (%s)", hostname,
		inet_ntoa(to->sin_addr));
	if (source)
		fprintf(stderr, " from %s", source);
	fprintf(stderr, ", %d hops max, %d byte packets\n", max_ttl, datalen);
	(void) fflush(stderr);

	for (ttl = 1; ttl <= max_ttl; ++ttl) {
		u_long lastaddr = 0;
		int got_there = 0;
		int unreachable = 0;

		printf("%2d ", ttl);
		for (probe = 0; probe < nprobes; ++probe) {
			int cc, reset_timer;
			struct timeval t1, t2;
			struct timezone tz;
			struct ip *ip;

			(void) gettimeofday(&t1, &tz);
			send_probe(++seq, ttl);
			reset_timer = 1;
			while ((cc = wait_for_reply(s, &from, reset_timer)) != 0) {
				(void) gettimeofday(&t2, &tz);
				if ((i = packet_ok(packet, cc, &from, seq))) {
					reset_timer = 1;
					if (from.sin_addr.s_addr != lastaddr) {
						print(packet, cc, &from);
						lastaddr = from.sin_addr.s_addr;
					}
					printf("  %g ms", deltaT(&t1, &t2));
					switch(i - 1) {
					case ICMP_UNREACH_PORT:
#ifndef ARCHAIC
						ip = (struct ip *)packet;
						if (ip->ip_ttl <= 1)
							printf(" !");
#endif ARCHAIC
						++got_there;
						break;
					case ICMP_UNREACH_NET:
						++unreachable;
						printf(" !N");
						break;
					case ICMP_UNREACH_HOST:
						++unreachable;
						printf(" !H");
						break;
					case ICMP_UNREACH_PROTOCOL:
						++got_there;
						printf(" !P");
						break;
					case ICMP_UNREACH_NEEDFRAG:
						++unreachable;
						printf(" !F");
						break;
					case ICMP_UNREACH_SRCFAIL:
						++unreachable;
						printf(" !S");
						break;
					}
					break;
				} else
					reset_timer = 0;
			}
			if (cc == 0)
				printf(" *");
			(void) fflush(stdout);
		}
		putchar('\n');
		if (got_there || unreachable >= nprobes-1)
			exit(0);
	}

	return 0;
}

static int
wait_for_reply(sock, from, reset_timer)
	int sock;
	struct sockaddr_in *from;
	int reset_timer;
{
	fd_set fds;
	static struct timeval wait;
	int cc = 0;
	int fromlen = sizeof (*from);

	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	if (reset_timer) {
		/*
		 * traceroute could hang if someone else has a ping
		 * running and our ICMP reply gets dropped but we don't
		 * realize it because we keep waking up to handle those
		 * other ICMP packets that keep coming in.  To fix this,
		 * "reset_timer" will only be true if the last packet that
		 * came in was for us or if this is the first time we're
		 * waiting for a reply since sending out a probe.  Note
		 * that this takes advantage of the select() feature on
		 * Linux where the remaining timeout is written to the
		 * struct timeval area.
		 */
		wait.tv_sec = waittime;
		wait.tv_usec = 0;
	}

	if (select(sock+1, &fds, (fd_set *)0, (fd_set *)0, &wait) > 0)
		cc=recvfrom(s, (char *)packet, sizeof(packet), 0,
			    (struct sockaddr *)from, &fromlen);

	return(cc);
}


static void
send_probe(seq, ttl)
	int seq, ttl;
{
	struct opacket *op = outpacket;
	struct ip *ip = &op->ip;
	struct udphdr *up = &op->udp;
	int i;
	struct timezone tz;

	ip->ip_off = 0;
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_len = datalen;
	ip->ip_ttl = ttl;
	ip->ip_v = IPVERSION;
	ip->ip_id = htons(ident+seq);

	up->source = htons(ident);
	up->dest = htons(port+seq);
	up->len = htons((u_short)(datalen - sizeof(struct ip)));
	up->check = 0;

	op->seq = seq;
	op->ttl = ttl;
	(void) gettimeofday(&op->tv, &tz);

	i = sendto(sndsock, (char *)outpacket, datalen, 0, &whereto,
		   sizeof(struct sockaddr));
	if (i < 0 || i != datalen)  {
		if (i<0)
			perror("sendto");
		printf("traceroute: wrote %s %d chars, ret=%d\n", hostname,
			datalen, i);
		(void) fflush(stdout);
	}
}


static double
deltaT(t1p, t2p)
	struct timeval *t1p, *t2p;
{
	register double dt;

	dt = (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 +
	     (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
	return (dt);
}


/*
 * Convert an ICMP "type" field to a printable string.
 */
static const char *
pr_type(t)
	u_char t;
{
	static const char * const ttab[] = {
	"Echo Reply",   "ICMP 1",       "ICMP 2",       "Dest Unreachable",
	"Source Quench", "Redirect",    "ICMP 6",       "ICMP 7",
	"Echo",         "ICMP 9",       "ICMP 10",      "Time Exceeded",
	"Param Problem", "Timestamp",   "Timestamp Reply", "Info Request",
	"Info Reply"
	};

	if(t > 16)
		return("OUT-OF-RANGE");

	return(ttab[t]);
}


static int
packet_ok(buf, cc, from, seq)
	u_char *buf;
	int cc;
	struct sockaddr_in *from;
	int seq;
{
	register struct icmp *icp;
	u_char type, code;
	int hlen;
#ifndef ARCHAIC
	struct ip *ip;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (verbose)
			printf("packet too short (%d bytes) from %s\n", cc,
				inet_ntoa(from->sin_addr));
		return (0);
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
#else
	icp = (struct icmp *)buf;
#endif ARCHAIC
	type = icp->icmp_type; code = icp->icmp_code;
	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH) {
		struct ip *hip;
		struct udphdr *up;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;
		up = (struct udphdr *)((u_char *)hip + hlen);
		if (hlen + 12 <= cc && hip->ip_p == IPPROTO_UDP &&
		    up->source == htons(ident) &&
		    up->dest == htons(port+seq))
			return (type == ICMP_TIMXCEED? -1 : code+1);
	}
#ifndef ARCHAIC
	if (verbose) {
		int i;
		u_long *lp = (u_long *)&icp->icmp_ip;

		printf("\n%d bytes from %s to %s", cc,
			inet_ntoa(from->sin_addr), inet_ntoa(ip->ip_dst));
		printf(": icmp type %d (%s) code %d\n", type, pr_type(type),
		       icp->icmp_code);
		for (i = 4; i < cc ; i += sizeof(long))
			printf("%2d: x%8.8lx\n", i, *lp++);
	}
#endif ARCHAIC
	return(0);
}


static void
print(buf, cc, from)
	u_char *buf;
	int cc;
	struct sockaddr_in *from;
{
	struct ip *ip;
	int hlen;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	cc -= hlen;

	if (nflag)
		printf(" %s", inet_ntoa(from->sin_addr));
	else
		printf(" %s (%s)", inetname(from->sin_addr),
		       inet_ntoa(from->sin_addr));

	if (verbose)
		printf (" %d bytes to %s", cc, inet_ntoa (ip->ip_dst));
}


/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
static char *
inetname(in)
	struct in_addr in;
{
	register char *cp;
	static char line[50];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = index(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag && in.s_addr != INADDR_ANY) {
		hp = gethostbyaddr((char *)&in, sizeof (in), AF_INET);
		if (hp) {
			if ((cp = index(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = (char *)hp->h_name;
		}
	}
	if (cp)
		(void) strcpy(line, cp);
	else {
		in.s_addr = ntohl(in.s_addr);
#define C(x)    ((x) & 0xff)
		sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			C(in.s_addr >> 16), C(in.s_addr >> 8), C(in.s_addr));
	}
	return (line);
}
