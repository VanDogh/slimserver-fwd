
#define __APPLE_USE_RFC_3542

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

void
forward_response(int sock_fd, unsigned char *buf, int len, struct sockaddr_in *dest,
		 struct in_pktinfo *src)
{
    struct iovec iovec[1];
    iovec[0].iov_base = buf;
    iovec[0].iov_len = len;

    unsigned char ctrl[256];
    memset(ctrl, 0, sizeof(ctrl));

    struct msghdr msg;
    msg.msg_name = dest;
    msg.msg_namelen = sizeof(*dest);
    msg.msg_iov = iovec;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl;
    msg.msg_controllen = sizeof(ctrl);
    msg.msg_flags = 0;

    if (src) {
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = IPPROTO_IP;
	cmsg->cmsg_type = IP_PKTINFO;
	cmsg->cmsg_len = CMSG_LEN(sizeof(*src));
	*(struct in_pktinfo*)CMSG_DATA(cmsg) = *src;
	msg.msg_controllen = CMSG_SPACE(sizeof(*src));
    } else {
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
    }

    // int nbytes = sendto(sock_fd, buf, len, 0, (struct sockaddr *) dest, sizeof(*dest));
    int nbytes = sendmsg(sock_fd, &msg, 0);
    if (nbytes < 0) {
	perror("Send error");
    } else if (nbytes < len) {
	printf("Only %d bytes forwarded to %08x\n", nbytes, ntohl(dest->sin_addr.s_addr));
    } else {
	printf("Forwarded %d bytes to %08x port %u\n", nbytes,
	       ntohl(dest->sin_addr.s_addr), ntohs(dest->sin_port));
    }
}

void
forward_bcast(int sock_fd, unsigned char *buf, int len)
{
    // Currently server is at: 10.8.0.1, use DNS someday and support command line options
    //
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET; // Use IPv4
    // dest.sin_addr.s_addr = htonl(0xAC1E0135); // Target IP
    dest.sin_addr.s_addr = htonl(0x0A080001); // Target IP of Media Server
    dest.sin_port = htons(3483); // Server Port for SlimServer

    forward_response(sock_fd, buf, len, &dest, NULL);
    /*
    int nbytes = sendto(sock_fd, buf, len, 0, (struct sockaddr *) &dest, sizeof(dest));
    if (nbytes < 0) {
	perror("Send error");
    } else if (nbytes < len) {
	printf("Only %d bytes forwarded\n", nbytes);
    } else {
	printf("Forwarded %d bytes\n", nbytes);
    }
    */
}

int
main(int argc, char *argv[])
{
    int sock_fd;
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Can't create socket");
        exit(-1);
    }

    int broadcast = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST,
		   &broadcast, sizeof broadcast)) {
	perror("Can't accept broadcast packets");
    }
    if (setsockopt(sock_fd, IPPROTO_IP, IP_PKTINFO,
		   &broadcast, sizeof broadcast)) {
	perror("Can't receive PKTINFO");
    }

    // Configure socket
    //
    struct sockaddr_in server;
    memset(&server, 0, sizeof server);
    server.sin_family = AF_INET; // Use IPv4
    server.sin_addr.s_addr = htonl(INADDR_ANY); // My IP
    server.sin_port = htons(3483); // Server Port for SlimServer

    // Bind socket
    //
    if ((bind(sock_fd, (struct sockaddr *) &server, sizeof(server))) == -1) {
        close(sock_fd);
        perror("Can't bind to port");
    }

    // Main message loop
    //
    unsigned char last_discovery = 0;
    struct sockaddr_in last_discoverer;
    memset(&last_discoverer, 0, sizeof(last_discoverer));

    while (1) {
	struct sockaddr_in si_caller;
	unsigned slen = sizeof(struct sockaddr_in);
	unsigned char buf [8192];
	memset(buf, 0, sizeof(buf));

	/*
	int nbytes = recvfrom(sock_fd, buf, sizeof(buf), 0,
			      (struct sockaddr *) &si_caller, &slen);
	*/

	struct iovec iov[1];
	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);

	unsigned char ctrl[256];

	struct msghdr msg;
	msg.msg_name = &si_caller;
	msg.msg_namelen = sizeof(si_caller);
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &ctrl;
	msg.msg_controllen = sizeof(ctrl);
	msg.msg_flags = 0;

	int nbytes = recvmsg(sock_fd, &msg, 0);
	if (nbytes > 0) {
	    int was_broadcast = 0;
	    struct cmsghdr *cmsg;
	    struct in_pktinfo pktinfo;
	    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		printf("Level: %u, Type: %u\n", cmsg->cmsg_level, cmsg->cmsg_type);
		if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
		    pktinfo = *((struct in_pktinfo *)CMSG_DATA(cmsg));
		    unsigned sent_to = ntohl(pktinfo.ipi_addr.s_addr);
		    printf("Message received to %08x\n", sent_to);
		    was_broadcast = (sent_to == 0xFFFFFFFF);
		}
	    }

	    if (si_caller.sin_family == AF_INET) {
		printf("Received %d bytes from %8X port %u\n",
		       nbytes,
		       (unsigned) ntohl(si_caller.sin_addr.s_addr),
		       ntohs(si_caller.sin_port));

		unsigned inx = 0;
		while (inx < nbytes) {
		    printf("%4x :", inx);
		    for (unsigned iny = 0; (iny < 16) && (inx < nbytes); ++inx, ++iny) {
			printf(" %02X", buf[inx]);
		    }
		    printf("\n");
		}
		if (was_broadcast) {
		    forward_bcast(sock_fd, buf, nbytes);
		    if (buf[0] == 0x64 || buf[0] == 0x65) {
			last_discovery = buf[0];
			last_discoverer = si_caller;
		    }
		} else if (last_discovery == (buf[0] | 0x20)) {
		    forward_response(sock_fd, buf, nbytes, &last_discoverer, &pktinfo);
		    last_discovery = 0;
		    memset(&last_discoverer, 0, sizeof(last_discoverer));
		}
	    } else {
		printf("Received %d bytes from unknown address family: %d\n",
		       nbytes, (unsigned) si_caller.sin_family);
	    }
	} else if (nbytes < 0) {
	    perror("Socket receive failed");
	    exit(1);
	}
    }
}
