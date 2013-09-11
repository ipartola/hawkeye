#ifndef __SETTINGS_H
#define __SETTINGS_H

// These should all be strings to be able to pass them through the option handler function
#define DEFAULT_LOG_LEVEL "info"
#define DEFAULT_PORT "8000"
#define DEFAULT_FPS "5"
#define DEFAULT_WIDTH "640"
#define DEFAULT_HEIGHT "480"
#define DEFAULT_JPEG_QUALITY "80"
#define DEFAULT_HOST "localhost"
#define DEFAULT_CONFIG_FILE	""
#define DEFAULT_LOG_FILE ""
#define DEFAULT_PID_FILE ""
#define DEFAULT_VIDEO_DEVICE_FILES "/dev/video0"
#define DEFAULT_V4L2_FORMAT "mjpeg"
#define DEFAULT_USER ""
#define DEFAULT_GROUP ""
#define DEFAULT_STATIC_ROOT ""
#define DEFAULT_AUTH ""
#define DEFAULT_SSL_CERT_FILE ""
#define DEFAULT_SSL_KEY_FILE ""

struct settings {
	short run_in_background;
	int port;
	int fps;
	char *host;
	char *config_file;
	char *static_root;
	char *log_file;
	char *pid_file;
	char *user;
	char *group;
	char *auth;
	char *ssl_cert_file;
	char *ssl_key_file;
	int width;
	int height;
	int jpeg_quality;
	
    int log_level;
	int v4l2_format;
	int video_device_count;
	char **video_device_files;
};

void init_settings(int argc, char *argv[]);
void cleanup_settings();
extern struct settings settings;

#endif
