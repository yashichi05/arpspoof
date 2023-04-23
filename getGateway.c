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
struct gateway_data
{
    int gateway;
    int interface_index;
};

struct gateway_data *data;
int interface_index;

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
    struct sockaddr_nl *_addr = (struct sockaddr_nl *)addr;
    memset(_addr, 0, sizeof(struct sockaddr));
    _addr->nl_family = AF_NETLINK;
    _addr->nl_pid = 0;
    _addr->nl_groups = 0;
}

void get_data(struct nlmsghdr *resp)
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
void get_msg(int fd)
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

        memset(data, 0, sizeof(struct gateway_data));
        get_data(resp);
        if (data->interface_index == interface_index)
        {
            break;
        }

        resp = NLMSG_NEXT(resp, buffer_len);
    }
}
int getGateway(int index)
{
    interface_index = index;
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
    return data->interface_index;
}
