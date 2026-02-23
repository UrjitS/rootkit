#include "networking.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <ifaddrs.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include "protocol.h"

int listen_port_knock(const struct client_options * client_options) {
    bool received_knock = false;
    const int fd = create_udp_socket();
    if (fd == -1) {
        fprintf(stderr, "Failed to create udp socket\n");
        return -1;
    }

    bind_socket(fd, client_options);

    printf("Listening for UDP on %s:%d\n", client_options->host, client_options->port);

    unsigned char *buf = malloc((size_t)client_options->max_bytes);
    if (!buf)
    {
        fprintf(stderr, "malloc failed\n");
        close(fd);
        return -1;
    }

    fflush(stdout);

    while (!exit_flag && !received_knock)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        struct timeval tv;
        tv.tv_sec = client_options->poll_ms / 1000;
        tv.tv_usec = (client_options->poll_ms % 1000) * 1000;

        const int r = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;

            perror("select");
            break;
        }

        if (r == 0)
        {
            // timeout: wake up, check g_stop, continue
            continue;
        }

        if (FD_ISSET(fd, &rfds))
        {
            struct sockaddr_in src;
            socklen_t srclen = sizeof(src);

            const ssize_t n = recvfrom(fd,
                                 buf,
                                 (size_t)client_options->max_bytes,
                                 0,
                                 (struct sockaddr *)&src,
                                 &srclen);

            if (n < 0)
            {
                if (errno == EINTR)
                    continue;

                perror("recvfrom");
                break;
            }

            char ts[32];
            time_t t = time(NULL);
            struct tm tm;

            localtime_r(&t, &tm);
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

            char src_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET,
                      &src.sin_addr,
                      src_ip,
                      sizeof(src_ip));

            printf("%s knock dst_port=%d src=%s:%u bytes=%zd\n",
                   ts,
                   client_options->port,
                   src_ip,
                   (unsigned)ntohs(src.sin_port),
                   n);

            if (client_options->knock_source_ip) {
                strncpy(client_options->knock_source_ip, src_ip, INET_ADDRSTRLEN);
            }

            received_knock = true;
            fflush(stdout);
        }
    }


    free(buf);
    close(fd);

    return received_knock ? 0 : -1;
}

int create_udp_socket() {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        perror("socket");
        return -1;
    }

    const int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR)");
        close(fd);
        return -1;
    }

    return fd;
}

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

int bind_socket(const int socket_fd, const struct client_options * client_options) {
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)client_options->port);

    if (inet_pton(AF_INET, client_options->host, &address.sin_addr) != 1)
    {
        fprintf(stderr, "invalid --host address: %s\n", client_options->host);
        close(socket_fd);
        return 2;
    }

    const int ret = bind(socket_fd, (struct sockaddr *) &address, sizeof(address));
    if (ret == -1) {
        fprintf(stderr, "Error binding socket");
        return -1;
    }
    return ret;
}

void create_ip_header(struct iphdr * ip, const uint32_t source_ip, const char * dest_ip, const uint16_t payload_len) {
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + payload_len);
    ip->id = htons(sequence_number);
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
        sequence_number++;
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
            src = (src & 0xFFFFFF00) | (unsigned char) message[index];
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

struct packet_data * parse_raw_packet(const char * buffer, const ssize_t n) {
    if (n < (ssize_t)(sizeof(struct iphdr) + sizeof(struct udphdr))) {
        return NULL;
    }

    const struct iphdr * ip = (struct iphdr *)buffer;

    if (ip->protocol != IPPROTO_UDP) {
        return NULL;
    }

    const struct udphdr * udp = (struct udphdr *)(buffer + (ip->ihl * 4));

    if (ntohs(udp->dest) != 8080) {
        return NULL;
    }

    const unsigned long payload_len = n - (ip->ihl * 4) - sizeof(struct udphdr);

    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &(ip->saddr), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip->daddr), dst_ip, INET_ADDRSTRLEN);

    const uint32_t host_src = ntohl(ip->saddr);

    // log_message("IP: %u:%d -> %s:%d\n", ntohl(ip->saddr), ntohs(udp->source), dst_ip, ntohs(udp->dest));
    // log_message("Total Length: %d bytes, Payload: %lu bytes\n", ntohs(ip->tot_len), payload_len);
    // log_message("UDP Length %d\n", ntohs(udp->len) - 8);

    fflush(stdout);

    struct packet_data * packet_data = malloc(sizeof(struct packet_data));;
    packet_data->sequence_number = (uint16_t) ip->id;
    packet_data->data = (uint16_t) host_src;

    return packet_data;
}


