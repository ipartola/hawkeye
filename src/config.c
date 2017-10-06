#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "memory.h"
#include "utils.h"

#include "config.h"

static void set_config_item_value(struct config_item *c, char *value);
static void process_option(struct config *con, char key, char *value);
static void read_line(char *line, char **name, char **value);
static char get_key_by_name(struct config *con, char *name);

static void read_line(char *line, char **name, char **value) {
	int i, len;
    char c;

    short name_finished = 0;

    *name = *value = NULL;
    len = strlen(line);

    // Cut line at comment
    for (i = 0; i < len; i++) {
        if (line[i] == '#') {
            line[i] = '\0';
            break;
        }
    }

    // Recalculate line length in case it had a comment.
    len = strlen(line);

	for (i = 0; i < len; i++) {
        c = line[i];

		if (*name == NULL) {
            if (!is_blank(c)) {
                *name = &line[i]; // first non-blank char is the beginning of the name
            }

            continue;
        }

        if (*name != NULL && !name_finished) {
            if (is_blank(c) || c == '=') {
                line[i] = '\0';
                name_finished = 1;
            }

            continue;
        }

		if (*value == NULL) {
            if (is_blank(c) || c == '=') {
                line[i] = '\0';
                continue;
            }
            else {
                *value = &line[i];
                break;
            }
        }
	}

    if (*value != NULL) {
        trim(*value);
    }
}

static char get_key_by_name(struct config *con, char *name) {
    int i;
    struct config_item *c;

    for (i = 0; i < con->count; i++) {
        c = &con->items[i];

        if (strcmp(c->long_name, name) == 0) {
            return c->short_name;
        }
    }

    return '\0';
}

static void set_config_item_value(struct config_item *c, char *value) {
    switch (c->dst_type) {
        case CONFIG_BOOL:
            *(short *) c->dst = (value == NULL || atoi(value) != 0);
            break;
        case CONFIG_STR:
            (*c->dst != NULL) ? free(*c->dst) : 0;
            *c->dst = strdup(value);
            break;
        case CONFIG_INT:
            *(int *) c->dst = atoi(value);
            break;
    }
}

static void process_option(struct config *con, char key, char *value) {
    int i;
    short found = 0;
    struct config_item *c;

    c = NULL;

    for (i = 0; i < con->count; i++) {
        c = &con->items[i];

        if (c->short_name == key) {
            found = 1;
            break;
        }
    }

    if (!found) {
        user_panic("Uknown option: %c.", key);
    }

    set_config_item_value(c, value);
}

struct config *create_config() {
    struct config *conf;

    conf = malloc(sizeof(struct config));

    conf->items = NULL;
    conf->count = 0;

    return conf;
}

void add_config_item(struct config *con, signed char short_name, char *long_name, char dst_type, void *dst, char *default_value) {
    struct config_item *c;

    if (con->items == NULL) {
        con->items = malloc(sizeof(struct config_item));
    }
    else {
        con->items = realloc(con->items, (con->count+1) * sizeof(struct config_item));
    }

    c = &con->items[con->count];

    c->short_name = short_name;
    c->long_name = strdup(long_name);
    c->dst_type = dst_type;
    c->dst = dst;
    c->default_value = strdup(default_value);

    c->long_option.name = c->long_name;
    c->long_option.has_arg = (dst_type != CONFIG_BOOL);
    c->long_option.flag = NULL;
    c->long_option.val = c->short_name;

    if (dst_type == CONFIG_STR) {
        *c->dst = NULL;
    }

    con->count++;

    set_config_item_value(c, default_value);
}

void read_config_file(struct config *con, char *filename) {
	FILE *fp;
	char line[1024];
    char *name, *value;
    char key;

    name = value = NULL;

	if (NULL == (fp = fopen(filename, "r"))) {
		panic("Could not open config file");
	}

	while(NULL != fgets(line, sizeof(line), fp)) {
		read_line(line, &name, &value);

		if (name != NULL && value != NULL && strlen(name) > 0 && strlen(value) > 0) {
            key = get_key_by_name(con, name);

            if (key == '\0') {
                user_panic("Found unknown option in config file: '%s'.", name);
            }

			process_option(con, key, value);
		}
	}

	fclose(fp);
}

void read_command_line(struct config *con, int argc, char *argv[]) {
    signed char short_name;
    int i, opstring_pos;
    // 512 = 2*256 because it can only contain ASCII, and we potentially
    // need another ":" char for every option
    char optstring[512];
    struct config_item *c;
    struct option long_options[con->count + 1];
    struct option null_option;

    memset(optstring, 0, sizeof(optstring));
    opstring_pos = 0;

    for (i = 0; i < con->count; i++) {
        c = &con->items[i];

        optstring[opstring_pos++] = c->short_name;

        if (c->dst_type != CONFIG_BOOL) {
            optstring[opstring_pos++] = ':';
        }

        memcpy((void *) &long_options[i], &c->long_option, sizeof(struct option));
    }

    null_option.name = NULL;
    null_option.has_arg = 0;
    null_option.flag = NULL;
    null_option.val = 0;

    memcpy((void *) &long_options[con->count], &null_option, sizeof(struct option));

    int old_optind = optind;
    while ((short_name = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
        process_option(con, short_name, optarg);
    }
    optind = old_optind;
}

void destroy_config(struct config *conf) {
    int i;
    struct config_item *c;

    for (i = 0; i < conf->count; i++) {
        c = &conf->items[i];
        free(c->long_name);
        free(c->default_value);
    }

    free(conf->items);
    free(conf);
}

