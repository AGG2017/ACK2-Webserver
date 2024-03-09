#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "httpd.h"

// read the configuration file by providing file path
// return last element pointer or NULL (empty file, or cannot read the file)
config_option_t read_config_file(char *path) {
    FILE *fp;

    if ((fp = fopen(path, "r")) == NULL) {
        if (debug)
            fprintf(stderr, "Error: cannot read the config file\n");
        return NULL;
    }

    config_option_t last_co_addr = NULL;

    while (1) {
        config_option_t co = NULL;
        if ((co = calloc(1, sizeof(config_option))) == NULL)
            break;
        memset(co, 0, sizeof(config_option));
        co->prev = last_co_addr;

        if (fscanf(fp, "%s = %s", &co->key[0], &co->value[0]) != 2) {
            if (feof(fp)) {
                // EOF reached, free mem
                free(co);
                break;
            }
            if (co->key[0] == '#') {
                while (fgetc(fp) != '\n') {
                    // Do nothing (to move the cursor to the end of the line).
                }
                free(co);
                continue;
            }
            free(co);
            continue;
        }
        // printf("Key: %s\nValue: %s\n", co->key, co->value);
        last_co_addr = co;
    }
    fclose(fp);
    return last_co_addr;
}

// write the configuration file by providing file path and last element pointer
int write_config_file(char *path, config_option_t conf_opt) {
    FILE *fp;

    if ((fp = fopen(path, "w")) == NULL) {
        if (debug)
            fprintf(stderr, "Error: cannot write the config file\n");
        return 1;
    }

    config_option_t co = conf_opt;

    while (1) {
        if (!co)
            break;
        fprintf(fp, "%s = %s\n", co->key, co->value);
        if (co->prev != NULL) {
            co = co->prev;
        } else {
            break;
        }
    }
    fclose(fp);
    return 0;
}

// free the memory used by the config file
// by providing the last element pointer
void free_config_file(config_option_t conf_opt) {
    config_option_t co = conf_opt;
    config_option_t co_prev;
    if (!co)
        return;
    while (1) {
        co_prev = co->prev;
        free(co);
        co = co_prev;
        if (!co)
            return;
    }
}

// set a key=value, update existing or create a new element
// provide last element pointer, key and value
// return 0 if success, 1 if failed
config_option_t set_key_value(config_option_t conf_opt, char *key, char *value) {
    config_option_t co = conf_opt;

    if (!key)
        return conf_opt;
    if (!value)
        return conf_opt;

    while (1) {
        if (!co)
            break;
        // co->key, co->value
        if (!strcmp(co->key, key)) {
            strncpy(co->value, value, CONFIG_VALUE_MAX_BYTES - 1);
            return conf_opt;
        }
        if (co->prev != NULL) {
            co = co->prev;
        } else {
            break;
        }
    }
    // key not found, add it
    if (!conf_opt) {
        // config is empty, create element
        if ((co = calloc(1, sizeof(config_option))) == NULL)
            return conf_opt;
        memset(co, 0, sizeof(config_option));
        co->prev = NULL;
        strncpy(co->key, key, CONFIG_KEY_MAX_BYTES - 1);
        strncpy(co->value, value, CONFIG_VALUE_MAX_BYTES - 1);
    } else {
        // config is not empty, create element
        if ((co = calloc(1, sizeof(config_option))) == NULL)
            return conf_opt;
        memset(co, 0, sizeof(config_option));
        co->prev = conf_opt;
        strncpy(co->key, key, CONFIG_KEY_MAX_BYTES - 1);
        strncpy(co->value, value, CONFIG_VALUE_MAX_BYTES - 1);
    }
    return co;
}

// read the value by selected key
// provide the last element pointer and the key
// return the pointer to the value or default for missing key
char *get_key_value(config_option_t conf_opt, char *key, char *def_value) {
    config_option_t co = conf_opt;
    if (!co)
        return def_value;
    while (1) {
        if (!strcmp(co->key, key)) {
            return co->value;
        }
        if (co->prev != NULL) {
            co = co->prev;
        } else {
            return def_value;
        }
    }
}

// create new config file from get request parameters
config_option_t read_config_file_from_get_request(char *parameters) {
    config_option_t last_co_addr = NULL;
    config_option_t co = NULL;
    char k_or_v, more;
    char *p;
    char *k;
    char *v;

    // set the initial state
    p = parameters;
    k = p;
    k_or_v = 0;
    v = NULL;

    // check for empty parameter list
    if (!p[0])
        return last_co_addr;

    // create new element
    if ((co = calloc(1, sizeof(config_option))) == NULL)
        return last_co_addr;
    memset(co, 0, sizeof(config_option));

    while (1) {
        if (p[0] == '=') {
            // end of the key
            p[0] = 0;   // fix the end of the key
            v = p + 1;  // set the value begin
            k_or_v = 1;
        } else {
            if ((p[0] == '&') || (!p[0])) {
                more = p[0];
                // end of this parameter or end of all parameters
                p[0] = 0;  // fix the end of the string (key or value)
                int k_len = strlen(k);
                if (k_len > 0) {
                    // set current element for a valid key
                    co->prev = last_co_addr;
                    strncpy(co->key, k, CONFIG_KEY_MAX_BYTES - 1);
                    if (v)
                        strncpy(co->value, v, CONFIG_VALUE_MAX_BYTES - 1);
                    last_co_addr = co;
                    co = NULL;
                }
                if (more == '&') {
                    if (!co) {
                        // create new element if the last element was used
                        if ((co = calloc(1, sizeof(config_option))) == NULL)
                            return last_co_addr;
                        memset(co, 0, sizeof(config_option));
                    }
                    k = p + 1;
                    k_or_v = 0;
                    v = NULL;
                } else {
                    if (co)
                        free(co);
                    return last_co_addr;
                }
            }
        }
        p++;
    }
}
