#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>

struct route_req
{
    struct nlmsghdr msghdr;
    struct rtmsg msg;
};

void setReq(struct route_req *req)
{
    memset(req, 0, sizeof(req));
    req->msghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req->msghdr.nlmsg_type = RTM_GETROUTE;
    req->msghdr.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;

    req->msg.rtm_family = AF_INET;
    req->msg.rtm_type = RTN_UNICAST;
}
void setAddr(struct sockaddr *addr)
{
    struct sockaddr_nl *_addr = (struct sockaddr_nl *)&addr;
    _addr->nl_family = AF_NETLINK;
    _addr->nl_pid = 0;
    _addr->nl_groups = 0;
}

void get_data(struct nlmsghdr *resp)
{
    struct rtmsg *route = (struct rtmsg *)NLMSG_DATA(resp); // 取得nlmsghdr 之後的資料

    struct rtattr *route_attr = (struct rtattr *)RTM_RTA(route);
    int resp_len = RTM_PAYLOAD(resp);

    // unsigned char *test = RTA_DATA(&route_attr[RTA_GATEWAY]);
    // static char buf[256];
    // printf("%s\n", inet_ntop(2, test, buf, 256));

    while (RTA_OK(route_attr, resp_len))
    {
        if (route_attr->rta_type == RTA_GATEWAY)
        {
            char *gateway[4];
            char gateway_string[256];
            memcpy(gateway, RTA_DATA(route_attr), sizeof(gateway));
            printf("%s\n", inet_ntop(AF_INET, gateway, gateway_string, sizeof(gateway_string)));
        }
        route_attr = RTA_NEXT(route_attr, resp_len);
    }
}
void get_msg(int fd)
{
    char *buffer[1024];
    // buffer = malloc(1024);
    int buffer_len = recv(fd, buffer, sizeof(buffer), 0);

    struct nlmsghdr *resp = (struct nlmsghdr *)buffer;
    while (NLMSG_OK(resp, buffer_len))
    {
        if (resp->nlmsg_type == NLMSG_ERROR)
        {
            printf("msg error\n");
        }
        get_data(resp);
        resp = NLMSG_NEXT(resp, buffer_len);
    }
}
int main(int argc, char *argv[])
{

    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

    struct route_req req;
    setReq(&req);

    struct sockaddr *addr;
    setAddr(addr);

    if (sendto(fd, &req, sizeof(req), 0, addr, sizeof(addr)) < 0)
    {
        printf("send error\n");
        exit(EXIT_FAILURE);
    }

    get_msg(fd);

    return EXIT_SUCCESS;
}
