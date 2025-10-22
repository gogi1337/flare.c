#include "common.h"

typedef struct {
    char route[MAX_ROUTE_LEN];
    char forward[MAX_FORWARD_LEN];
} Route;

typedef struct {
    char addr[64];
    int disable_domain_not_configured_warns;
    int disable_failed_to_reach_warns;
    Route routes[MAX_ROUTES];
    int route_count;
} Config;

void log_info(const char *format, ...);
void log_warn(const char *format, ...);
void log_debug(const char *format, ...);

char* trim(char *str);
int parse_config(const char *path, Config *config);
Route* find_route(Config *config, const char *host);