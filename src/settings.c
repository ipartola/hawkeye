#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <linux/videodev2.h>

#include "memory.h"
#include "logger.h"
#include "version.h"
#include "config.h"
#include "utils.h"

#include "settings.h"

struct settings settings;

static void normalize_path(char **path, const char *error) {
    char tmp_path[PATH_MAX];

    if (!strlen(*path)) {
        return;
    }

    if (NULL == realpath(*path, tmp_path)) {
        panic(error);
    }

    free(*path);
    *path = strdup(tmp_path);
}

void print_usage() {
    fprintf(stdout, "Usage: %s [-d] [-c config] [-H host] [-p port] [-w www-root] [-P pidfile]\n", program_name);
    fprintf(stdout, "       [-l logfile] [-u user] [-g group] [-F fps] [-D video-devices] [-W width]\n");
    fprintf(stdout, "       [-G height] [-j jpeg-quality] [-L log-level] [-f format] [-A user:pass]\n");
    fprintf(stdout, "       [-C cert-file] [-k key-file]\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Usage: %s [--daemon] [--config=path] [--host=host] [--port=port]\n", program_name);
    fprintf(stdout, "       [--www-root=path] [--pid=path] [--log=path] [--user=user] [--group=group]\n");
    fprintf(stdout, "       [--fps=fps][--devices=video-devices] [--width=width] [--height=height]\n");
    fprintf(stdout, "       [--quality=quality] [--log-level=log-level] [--format=format]\n");
    fprintf(stdout, "       [--auth=user:pass] [--cert=cert-file] [--key=key-file]\n");

    fprintf(stdout, "Usage: %s [-h]\n", program_name);
    fprintf(stdout, "Usage: %s [-v]\n", program_name);
    fprintf(stdout, "\n");
    fprintf(stdout, "devices is a : separated list of video devices, such as\n");
    fprintf(stdout, "for example \"/dev/video0:/dev/video1\".\n");
    fprintf(stdout, "log-level can be debug, info, warning, or error.\n");
    fprintf(stdout, "format can be mjpeg (recommended) or yuv.\n");
}

void init_settings(int argc, char *argv[]) {
    struct config *conf;
    char *log_level, *v4l2_format, *video_device_files;
    short display_version, display_usage;
    int i, video_devices_len;

    conf = create_config();

    add_config_item(conf, 'd', "daemon", CONFIG_BOOL, &settings.run_in_background, "0");
    add_config_item(conf, 'c', "config", CONFIG_STR, &settings.config_file, DEFAULT_CONFIG_FILE);
    add_config_item(conf, 'H', "host", CONFIG_STR, &settings.host, DEFAULT_HOST);
    add_config_item(conf, 'p', "port", CONFIG_INT, &settings.port, DEFAULT_PORT);
    add_config_item(conf, 'w', "www-root", CONFIG_STR, &settings.static_root, DEFAULT_STATIC_ROOT);
    add_config_item(conf, 'l', "log", CONFIG_STR, &settings.log_file, DEFAULT_LOG_FILE);
    add_config_item(conf, 'P', "pid", CONFIG_STR, &settings.pid_file, DEFAULT_PID_FILE);
    add_config_item(conf, 'u', "user", CONFIG_STR, &settings.user, DEFAULT_USER);
    add_config_item(conf, 'g', "group", CONFIG_STR, &settings.group, DEFAULT_GROUP);
    add_config_item(conf, 'F', "fps", CONFIG_INT, &settings.fps, DEFAULT_FPS);
    add_config_item(conf, 'W', "width", CONFIG_INT, &settings.width, DEFAULT_WIDTH);
    add_config_item(conf, 'G', "height", CONFIG_INT, &settings.height, DEFAULT_HEIGHT);
    add_config_item(conf, 'j', "quality", CONFIG_INT, &settings.jpeg_quality, DEFAULT_JPEG_QUALITY);
    add_config_item(conf, 'A', "auth", CONFIG_STR, &settings.auth, DEFAULT_AUTH);
    add_config_item(conf, 'C', "cert", CONFIG_STR, &settings.ssl_cert_file, DEFAULT_SSL_CERT_FILE);
    add_config_item(conf, 'k', "key", CONFIG_STR, &settings.ssl_key_file, DEFAULT_SSL_KEY_FILE);
    
    add_config_item(conf, 'L', "log-level", CONFIG_STR, &log_level, DEFAULT_LOG_LEVEL);
    add_config_item(conf, 'f', "format", CONFIG_STR, &v4l2_format, DEFAULT_V4L2_FORMAT);
    add_config_item(conf, 'D', "devices", CONFIG_STR, &video_device_files, DEFAULT_VIDEO_DEVICE_FILES);
    
    add_config_item(conf, 'h', "help", CONFIG_BOOL, &display_usage, "0");
    add_config_item(conf, 'v', "version", CONFIG_BOOL, &display_version, "0");

    // Read command line options to get the location of the config file
    read_command_line(conf, argc, argv);

    // Read config file
    if ((settings.config_file == NULL) || strlen(settings.config_file) == 0) {
        puts("You must specify a config file with the -c or --config option.\n");
        print_usage();
        exit(0);
    }
    read_config_file(conf, settings.config_file);

    // Read command line options again to overwrite config file values
    read_command_line(conf, argc, argv);

    destroy_config(conf);

    // Set log level
    settings.log_level = LOG_INFO;
    if (strcmp(log_level, "debug") == 0) {
        settings.log_level = LOG_DEBUG;
    }
    else if (strcmp(log_level, "info") == 0) {
        settings.log_level = LOG_INFO;
    }
    else if (strcmp(log_level, "warning") == 0) {
        settings.log_level = LOG_WARNING;
    }
    else if (strcmp(log_level, "error") == 0) {
        settings.log_level = LOG_ERROR;
    }

    // Set format
    settings.v4l2_format = V4L2_PIX_FMT_MJPEG;
    if (strcmp(v4l2_format, "yuv") == 0) {
        settings.v4l2_format = V4L2_PIX_FMT_YUYV;
    }
    
    // Parse video devices
    settings.video_device_count = 0;
    settings.video_device_files = malloc(32 * sizeof(char *));
    settings.video_device_files[settings.video_device_count++] = video_device_files;

    video_devices_len = strlen(video_device_files);
    for (i = 0; i < video_devices_len; i++) {
        if (video_device_files[i] == ':') {
            video_device_files[i] = '\0';
            if (i + 1 < video_devices_len) {
                settings.video_device_files[settings.video_device_count++] = &video_device_files[i + 1];
            }
        }
    }

    free(log_level);
    free(v4l2_format);

    settings.port = (unsigned short) abs(settings.port);
    settings.jpeg_quality = max(1, min(100, settings.jpeg_quality));
    settings.fps = max(1, min(50, settings.fps));

    normalize_path(&settings.static_root, "The www-root you specified does not exist");
    normalize_path(&settings.ssl_cert_file, "The SSL certificate file you specified does not exist");
    normalize_path(&settings.ssl_key_file, "The SSL private key file you specified does not exist");

    if (display_usage) {
        print_usage();
        exit(0);
    }
    
    if (display_version) {
        print_version();
        exit(0);
    }
}

void cleanup_settings() {
    free(settings.host);
    free(settings.config_file);
    free(settings.log_file);
    free(settings.pid_file);
    free(settings.user);
    free(settings.group);
    free(settings.video_device_files[0]);
    free(settings.video_device_files);
    free(settings.static_root);
    free(settings.auth);
    free(settings.ssl_cert_file);
    free(settings.ssl_key_file);
}

