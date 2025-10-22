#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

Config global_config;

void parse_host_port(const char *addr, char *host, int *port) {
    char *colon = strchr(addr, ':');
    if (colon) {
        int host_len = colon - addr;
        strncpy(host, addr, host_len);
        host[host_len] = '\0';
        *port = atoi(colon + 1);
    }
}

char* extract_host_header(const char *request) {
    char *host_line = strstr(request, "Host: ");
    if (!host_line) return NULL;

    host_line += 6;
    char *end = strstr(host_line, "\r\n");
    if (!end) return NULL;

    int len = end - host_line;
    char *host = malloc(len + 1);
    strncpy(host, host_line, len);
    host[len] = '\0';

    return host;
}

int connect_to_server(const char *forward_url) {
    char host[256];
    int port;
    
    // Parse forward URL (assuming http://host:port format)
    if (strncmp(forward_url, "http://", 7) == 0) {
        parse_host_port(forward_url + 7, host, &port);
    } else {
        parse_host_port(forward_url, host, &port);
    }

    struct sockaddr_in server_addr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    struct hostent *server = gethostbyname(host);
    if (!server) {
        close(sock);
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    server_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

void* handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[MAX_BUFFER];
    int bytes_read = recv(client_sock, buffer, MAX_BUFFER - 1, 0);
    if (bytes_read <= 0) {
        close(client_sock);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    char *host = extract_host_header(buffer);
    if (!host) {
        const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 19\r\n\r\nHost header missing";
        send(client_sock, response, strlen(response), 0);
        close(client_sock);
        return NULL;
    }

    Route *route = find_route(&global_config, host);
    if (!route) {
        if (!global_config.disable_domain_not_configured_warns) {
            log_warn("%s (Domain not configured)", host);
        }
        const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 22\r\n\r\nDomain not configured";
        send(client_sock, response, strlen(response), 0);
        free(host);
        close(client_sock);
        return NULL;
    }

    int target_sock = connect_to_server(route->forward);
    if (target_sock < 0) {
        if (!global_config.disable_failed_to_reach_warns) {
            log_warn("%s -> %s (Failed to reach)", route->route, route->forward);
        }
        const char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 24\r\n\r\nFailed to proxy request";
        send(client_sock, response, strlen(response), 0);
        free(host);
        close(client_sock);
        return NULL;
    }

    log_debug("%s -> %s", route->route, route->forward);

    // Forward request
    send(target_sock, buffer, bytes_read, 0);

    // Forward response
    while ((bytes_read = recv(target_sock, buffer, MAX_BUFFER, 0)) > 0) {
        send(client_sock, buffer, bytes_read, 0);
    }

    free(host);
    close(target_sock);
    close(client_sock);
    return NULL;
}

int main() {
    if (parse_config("./config.ini", &global_config) < 0) {
        fprintf(stderr, "Failed to load config\n");
        return 1;
    }

    char host[256];
    int port;
    parse_host_port(global_config.addr, host, &port);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(host);
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        close(server_sock);
        return 1;
    }

    log_info("Flare running on %s:%d", host, port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

        if (*client_sock < 0) {
            free(client_sock);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_sock);
        pthread_detach(thread);
    }

    close(server_sock);
    return 0;
}
