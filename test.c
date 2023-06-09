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
#include <unistd.h>
#include <memory.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/netlink.h>

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
	int interface_index;
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

struct route_req
{
	struct nlmsghdr msghdr;
	struct rtmsg msg;
};
struct gateway_data
{
	int gateway;
	int interface_index;
};

void get_data(struct nlmsghdr *resp, struct gateway_data *data)
{
	struct rtmsg *route = (struct rtmsg *)NLMSG_DATA(resp); // 取得nlmsghdr 之後的資料

	struct rtattr *route_attr = (struct rtattr *)RTM_RTA(route);
	int resp_len = RTM_PAYLOAD(resp);

	while (RTA_OK(route_attr, resp_len))
	{
		if (route_attr->rta_type == RTA_GATEWAY)
		{
			memcpy(&data->gateway, RTA_DATA(route_attr), sizeof(int));
		}
		else if (route_attr->rta_type == RTA_OIF)
		{
			memcpy(&data->interface_index, RTA_DATA(route_attr), sizeof(int));
		}
		route_attr = RTA_NEXT(route_attr, resp_len);
	}
}
int get_msg(int fd, int index)
{
	char *buffer[1024];
	int buffer_len = recv(fd, buffer, sizeof(buffer), 0);

	struct nlmsghdr *resp = (struct nlmsghdr *)buffer;
	while (NLMSG_OK(resp, buffer_len))
	{
		if (resp->nlmsg_type == NLMSG_ERROR)
		{
			printf("msg error\n");
		}
		struct gateway_data result;
		memset(&result, 0, sizeof(result));
		get_data(resp, &result);
		if (result.interface_index == index)
		{

			return result.gateway;
		}

		resp = NLMSG_NEXT(resp, buffer_len);
	}
	return -1;
}

void getGatewayAddr(struct opts *options)
{
	int target_index = options->interface_index;
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	struct route_req req;
	memset(&req, 0, sizeof(req));
	req.msghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.msghdr.nlmsg_type = RTM_GETROUTE;
	req.msghdr.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;

	req.msg.rtm_family = AF_INET;
	req.msg.rtm_type = RTN_UNICAST;

	struct sockaddr_nl addr;
	memset(&addr, 0, sizeof(struct sockaddr_nl));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = 0;
	addr.nl_groups = 0;

	if (sendto(fd, &req, sizeof(req), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		printf("send error\n");
		exit(EXIT_FAILURE);
	}

	options->gateway_ip = get_msg(fd, target_index);
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

	if (ioctl(options->sock_fd, SIOCGIFINDEX, &ifr) == -1)
	{
		printf("ioctl error\n");
		exit(EXIT_FAILURE);
	}
	memcpy((void *)&options->interface_index, (void *)&ifr.ifr_ifindex, sizeof(int));

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
}

int main(int argc, char *argv[])
{
	struct opts *options;
	memset((void *)options, 0, sizeof(options));
	getArgv(argc, argv, options);
	getSelfAddr(options);
	getGatewayAddr(options);
	getTargetAddr(options);
	
	// replyArp(0, options);
	// replyArp(1, options);

	return EXIT_SUCCESS;
}
//    uint32_t ip = options->gateway_ip;
//     struct in_addr ip_addr;
//     ip_addr.s_addr = ip;
//     printf("The IP address is %s\n", inet_ntoa(ip_addr));