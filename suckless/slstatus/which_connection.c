#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void set_interface_state(const char *ifname, int enable) {
    struct nl_sock *sock;
    struct nl_msg *msg;
    struct ifinfomsg *ifi;

    sock = nl_socket_alloc();
    nl_connect(sock, NETLINK_ROUTE);

    // Allocate a new message
    msg = nlmsg_alloc();
    ifi = (struct ifinfomsg *)nlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, RTM_SETLINK, 0, NLM_F_REQUEST);

    // Set the interface index
    ifi->ifi_index = if_nametoindex(ifname);
    if (ifi->ifi_index == 0) {
        perror("Invalid interface name");
        nlmsg_free(msg);
        nl_socket_free(sock);
        return;
    }

    // Set the state (up or down)
    if (enable) {
        ifi->ifi_flags |= IFF_UP;  // Enable interface
    } else {
        ifi->ifi_flags &= ~IFF_UP; // Disable interface
    }

    // Send the message
    nl_send_auto(sock, msg);
    nlmsg_free(msg);
    nl_socket_free(sock);
}

int is_interface_connected(const char *ifname) {
    struct nl_sock *sock;
    struct nl_msg *msg;
    struct nlmsghdr *hdr;
    struct ifinfomsg *ifi;
    char buf[IFNAMSIZ];
    int connected = 0;

    sock = nl_socket_alloc();
    nl_connect(sock, NETLINK_ROUTE);

    // Request the interface list
    msg = nlmsg_alloc();
    nlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, RTM_GETLINK, 0, NLM_F_REQUEST);
    nl_send_auto(sock, msg);
    nlmsg_free(msg);

    // Receive messages
    while (1) {
        nl_recvmsgs(sock);

        // Process received messages
        hdr = nlmsg_hdr(sock);
        if (hdr->nlmsg_type == RTM_NEWLINK) {
            ifi = (struct ifinfomsg *)nlmsg_data(hdr);
            strncpy(buf, if_indextoname(ifi->ifi_index, buf), IFNAMSIZ);

            if (strcmp(buf, ifname) == 0) {
                if (ifi->ifi_flags & IFF_RUNNING) {
                    connected = 1; // Interface is connected
                }
                break;
            }
        }

        // Break the loop if no more messages
        if (hdr->nlmsg_type == NLMSG_DONE) {
            break;
        }
    }

    nl_socket_free(sock);
    return connected;
}

int main() {
    const char *wifi_ifname = "wlp0s20f3";   // Change this to your Wi-Fi interface name
    const char *ethernet_ifname = "enp0s31f6"; // Change this to your Ethernet interface name

    // Check Ethernet connection
    int ethernet_connected = is_interface_connected(ethernet_ifname);
    printf("Ethernet (%s) is %s\n", ethernet_ifname, ethernet_connected ? "connected" : "not connected");

    if (ethernet_connected) {
        printf("Disabling Wi-Fi (%s)\n", wifi_ifname);
        set_interface_state(wifi_ifname, 0); // Disable Wi-Fi
    } else {
        printf("Enabling Wi-Fi (%s)\n", wifi_ifname);
        set_interface_state(wifi_ifname, 1); // Enable Wi-Fi
    }

    return 0;
}
