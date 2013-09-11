#ifndef __CONFIG_H
#define __CONFIG_H

#include <getopt.h>

#define CONFIG_BOOL 'b'
#define CONFIG_INT 'i'
#define CONFIG_STR 's'

struct config_item {
    signed char short_name;
    char *long_name;
    char dst_type;  // b for bool, s for char[], i for int.
    void **dst;
    void *default_value;
    struct option long_option;
};

struct config {
    struct config_item *items; // Array of config_items
    size_t count;
};

struct config *create_config();
void destroy_config(struct config *conf);
void add_config_item(struct config *con, signed char short_name, char *long_name, char dst_type, void *dst, char *default_value);
void read_command_line(struct config *conf, int argc, char *argv[]);
void read_config_file(struct config *conf, char *filename);

#endif
