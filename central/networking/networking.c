#include "networking.h"
#include "utils.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/udp.h>


int create_raw_udp_socket() {
    const int fd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (fd < 0)
    {
        perror("socket");
        return -1;
    }

    const int yes = 1;
    if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &yes, sizeof(yes)) < 0)
    {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    return fd;
}

char * get_local_interface_name() {
    struct sockaddr_in address;
    address.sin_family = AF_INET;

    // Get list of network interfaces
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        fprintf(stderr, "Error getting network interfaces\n");
        return NULL;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if (strncmp(ifa->ifa_name, "wlo", 3) == 0) {
            freeifaddrs(ifaddr);
            return ifa->ifa_name;
        }
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if (strcmp(ifa->ifa_name, "lo") != 0) {
            freeifaddrs(ifaddr);
            return ifa->ifa_name;
        }
    }

    freeifaddrs(ifaddr);
    return NULL;
}

int bind_raw_socket(const int socket_fd, char * ip_address) {
    //Bind to interface
    if ((setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, ip_address, strlen(ip_address)+1)) < 0) {
        printf("Failed to bind socket to interface device %s!\n", ip_address);
        return -1;
    }

    return 0;
}

char * get_local_address() {
    struct sockaddr_in address;
    address.sin_family = AF_INET;

    // Get list of network interfaces
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        fprintf(stderr, "Error getting network interfaces\n");
        return NULL;
    }

    bool found = false;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if (strncmp(ifa->ifa_name, "wlo", 3) == 0) {
            const struct sockaddr_in * addr_in = (struct sockaddr_in*) ifa->ifa_addr;
            address.sin_addr = addr_in->sin_addr;
            printf("Binding to %s: %s\n",  ifa->ifa_name, inet_ntoa(addr_in->sin_addr));
            found = true;
            break;
        }
    }

    if (!found) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }

            if (strcmp(ifa->ifa_name, "lo") != 0) {
                const struct sockaddr_in * addr_in = (struct sockaddr_in*) ifa->ifa_addr;
                address.sin_addr = addr_in->sin_addr;
                printf("Binding to %s: %s\n",  ifa->ifa_name, inet_ntoa(addr_in->sin_addr));
                found = true;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);

    if (!found) {
        printf("No suitable network interface found");
        return NULL;
    }
    const struct sockaddr_in * addr_in = (struct sockaddr_in*) ifa->ifa_addr;
    address.sin_addr = addr_in->sin_addr;
    return inet_ntoa(addr_in->sin_addr);
}

void create_ip_header(struct iphdr * ip, const uint32_t source_ip, const char * dest_ip, const uint16_t payload_len) {
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + payload_len);
    ip->id = htons(0x1234);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = IPPROTO_UDP;
    ip->saddr = htonl(source_ip);
    ip->daddr = inet_addr(dest_ip);
    ip->check = checksum(ip, sizeof(struct iphdr));
}

void create_udp_header(struct udphdr * udp, const int port, const uint16_t payload_len) {
    udp->source = htons(port);
    udp->dest   = htons(port);
    udp->len    = htons(sizeof(struct udphdr) + payload_len);
    udp->check  = 0;
}

void send_packet(const int socket_fd, const char * packet, const struct iphdr * ip, const struct udphdr * udp, const int payload_len) {
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = udp->dest;
    dest.sin_addr.s_addr = ip->daddr;

    const ssize_t sent = sendto(socket_fd, packet, sizeof(struct iphdr) + sizeof(struct udphdr) + payload_len, 0, (struct sockaddr *)&dest, sizeof(dest));
    if (sent < 0) {
        perror("sendto");
    }
    else {
        printf("Sent raw UDP packet\n");
    }
}

void send_command(const int socket_fd, const char * dest_ip, const int port, const enum command_codes command) {
    char packet[4096] = {0};
    const uint16_t payload_len = generate_random_length(PACKET_LENGTH_MAX);
    char * random_string = generate_random_string(payload_len);
    uint32_t src = generate_random_length(IP_MAX) << 24 | generate_random_length(IP_MAX) << 16 | 0 << 8 | 0;

    struct iphdr * ip = (struct iphdr *)packet;
    struct udphdr * udp = (struct udphdr *)(packet + sizeof(struct iphdr));
    char * payload = packet + sizeof(struct iphdr) + sizeof(struct udphdr);

    const uint8_t command_byte = command;
    src = (src & 0xFFFF0000) | (command_byte << 8) | command_byte;

    create_ip_header(ip, src, dest_ip, payload_len);
    create_udp_header(udp, port, payload_len);
    strcpy(payload, random_string);

    send_packet(socket_fd, packet, ip, udp, payload_len);
    send_packet(socket_fd, packet, ip, udp, payload_len);

    free(random_string);
}

void send_message(const int socket_fd, const char * dest_ip, const int port, const char * message) {
    char packet[4096] = {0};
    uint16_t payload_len = generate_random_length(PACKET_LENGTH_MAX);
    uint32_t src = generate_random_length(IP_MAX) << 24 | generate_random_length(IP_MAX) << 16 | generate_random_length(IP_MAX) << 8 | generate_random_length(IP_MAX);

    struct iphdr * ip = (struct iphdr *)packet;
    struct udphdr * udp = (struct udphdr *)(packet + sizeof(struct iphdr));
    char * payload = packet + sizeof(struct iphdr) + sizeof(struct udphdr);


    if (message == NULL) {
        char * random_string = generate_random_string(payload_len);
        create_ip_header(ip, src, dest_ip, payload_len);
        create_udp_header(udp, port, payload_len);
        strcpy(payload, random_string);
        send_packet(socket_fd, packet, ip, udp, payload_len);

        free(random_string);
        return;
    }
    const size_t length = strlen(message);
    for (int index = 0; index < length; index += 2) {
        // Only 1 byte remaining
        if (index + 2 > length) {
            src = (src & 0xFFFF0000) | (unsigned char) message[index];
            char * random_string = generate_random_string(payload_len);

            create_ip_header(ip, src, dest_ip, payload_len);
            create_udp_header(udp, port, payload_len);
            strcpy(payload, random_string);
            send_packet(socket_fd, packet, ip, udp, payload_len);

            free(random_string);
            return;
        }

        // Two bytes operation
        const uint8_t first_byte = message[index];
        const uint8_t second_byte = message[index + 1];
        src = (src & 0xFFFF0000) | (first_byte << 8) | second_byte;
        char * random_string = generate_random_string(payload_len);

        create_ip_header(ip, src, dest_ip, payload_len);
        create_udp_header(udp, port, payload_len);
        strcpy(payload, random_string);
        send_packet(socket_fd, packet, ip, udp, payload_len);

        free(random_string);
        payload_len = generate_random_length(PACKET_LENGTH_MAX); // rand length per packet
    }
}

uint16_t parse_raw_packet(const char * buffer, const ssize_t n) {
    if (n < (ssize_t)(sizeof(struct iphdr) + sizeof(struct udphdr))) {
        return 0;
    }

    const struct iphdr * ip = (struct iphdr *)buffer;

    if (ip->protocol != IPPROTO_UDP) {
        return 0;
    }

    const struct udphdr * udp = (struct udphdr *)(buffer + (ip->ihl * 4));

    if (ntohs(udp->dest) != 8080) {
        return 0;
    }

    const unsigned long payload_len = n - (ip->ihl * 4) - sizeof(struct udphdr);

    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &(ip->saddr), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip->daddr), dst_ip, INET_ADDRSTRLEN);

    // printf("IP: %s:%d -> %s:%d\n", src_ip, ntohs(udp->source), dst_ip, ntohs(udp->dest));
    // printf("Total Length: %d bytes, Payload: %lu bytes\n", ntohs(ip->tot_len), payload_len);
    // printf("UDP Length %d\n", ntohs(udp->len) - 8);

    fflush(stdout);

    return ntohs(udp->len) - 8;
}

