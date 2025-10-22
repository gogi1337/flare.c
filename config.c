#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("[INFO] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("[WARN] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("[DEBUG] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 512

// Trim whitespace from both ends of a string
char* trim(char *str) {
    char *end;
    
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    end[1] = '\0';
    return str;
}

int parse_config(const char *path, Config *config) {
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Failed to open config file");
        return -1;
    }

    char line[MAX_LINE];
    int in_routes = 0;
    int current_route = -1;
    
    config->route_count = 0;
    config->disable_domain_not_configured_warns = 0;
    config->disable_failed_to_reach_warns = 0;
    memset(config->addr, 0, sizeof(config->addr));

    while (fgets(line, sizeof(line), file)) {
        char *trimmed = trim(line);
        
        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        if (trimmed[0] == '[') {
            char *end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                char *section_name = trim(trimmed + 1);
                
                if (strcmp(section_name, "routes") == 0) {
                    in_routes = 1;
                } else if (strncmp(section_name, "route", 5) == 0) {
                    current_route = config->route_count++;
                    memset(config->routes[current_route].route, 0, MAX_ROUTE_LEN);
                    memset(config->routes[current_route].forward, 0, MAX_FORWARD_LEN);
                } else {
                    in_routes = 0;
                    current_route = -1;
                }
            }
            continue;
        }

        // Parse key = value pairs
        char *equals = strchr(trimmed, '=');
        if (equals) {
            *equals = '\0';
            char *key = trim(trimmed);
            char *value = trim(equals + 1);

            // Remove quotes if present
            if (value[0] == '"' || value[0] == '\'') {
                value++;
                int len = strlen(value);
                if (len > 0 && (value[len-1] == '"' || value[len-1] == '\'')) {
                    value[len-1] = '\0';
                }
            }

            if (current_route >= 0) {
                // Inside a [route] section
                if (strcmp(key, "route") == 0) {
                    strncpy(config->routes[current_route].route, value, MAX_ROUTE_LEN - 1);
                } else if (strcmp(key, "forward") == 0) {
                    strncpy(config->routes[current_route].forward, value, MAX_FORWARD_LEN - 1);
                }
            } else {
                // Global configuration
                if (strcmp(key, "addr") == 0) {
                    strncpy(config->addr, value, 63);
                } else if (strcmp(key, "disable_domain_not_configured_warns") == 0) {
                    config->disable_domain_not_configured_warns = 
                        (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "disable_failed_to_reach_warns") == 0) {
                    config->disable_failed_to_reach_warns = 
                        (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                }
            }
        }
    }

    fclose(file);
    return 0;
}

Route* find_route(Config *config, const char *host) {
    for (int i = 0; i < config->route_count; i++) {
        if (strcmp(config->routes[i].route, host) == 0) {
            return &config->routes[i];
        }
    }
    return NULL;
}