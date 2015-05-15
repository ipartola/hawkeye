#!/bin/sh
### BEGIN INIT INFO
# Provides:          hawkeye
# Required-Start:    $network $local_fs $remote_fs
# Required-Stop:     $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: hawkeye daemon
# Description:       hawkeye USB webcam streaming service
### END INIT INFO

# Author: Igor Partola <igor@igorpartola.com>

# PATH should only include /usr/* if it runs after the mountnfs.sh script
PATH=/sbin:/usr/sbin:/bin:/usr/bin
DESC='hawkeye daemon'                    # Introduce a short description here
NAME=hawkeye                            # Introduce the short server's name here
DAEMON=/usr/bin/hawkeye
CONFIG="/etc/hawkeye/hawkeye.conf"
PIDFILE="/var/run/$NAME/$NAME.pid"
SCRIPTNAME="/etc/init.d/$NAME"
DAEMON_ARGS="-d -c $CONFIG" # Arguments to run the daemon with
STARTUP_TIMEOUT=1

# Exit if the package is not installed
[ -x $DAEMON ] || exit 0

# Load the VERBOSE setting and other rcS variables
. /lib/init/vars.sh

# Define LSB log_* functions.
# Depend on lsb-base (>= 3.0-6) to ensure that this file is present.
. /lib/lsb/init-functions

#
# Function that starts the daemon/service
#
do_start()
{
	# Return
	#   0 if daemon has been started
	#   1 if daemon was already running
	#   2 if daemon could not be started
	start-stop-daemon --start --quiet --pidfile $PIDFILE --exec $DAEMON --test > /dev/null \
		|| return 1

    # If the line above did not return it means that the process isn't running
    # so it is safe to delete this PID file (aside from obvious race conditions)
    rm -f "$PIDFILE"

	start-stop-daemon --start --quiet --pidfile $PIDFILE --exec $DAEMON -- \
		$DAEMON_ARGS \
		|| return 2
	# Add code here, if necessary, that waits for the process to be ready
	# to handle requests from services started subsequently which depend
	# on this one.  As a last resort, sleep for some time.
    sleep $STARTUP_TIMEOUT
    if [ -e "$PIDFILE" ]; then
        if ! kill -0 `cat "$PIDFILE"` > /dev/null 2>&1; then
            return 2
        fi
    else
        return 2
    fi
}

#
# Function that stops the daemon/service
#
do_stop()
{
	# Return
	#   0 if daemon has been stopped
	#   1 if daemon was already stopped
	#   2 if daemon could not be stopped
	#   other if a failure occurred
	start-stop-daemon --stop --quiet --retry=TERM/5/KILL/1 --pidfile $PIDFILE --name `basename "$DAEMON"`
	RETVAL="$?"
	return "$RETVAL"
}

#
# Function that sends a SIGHUP to the daemon/service
#
do_reload() {
	#
	# If the daemon can reload its configuration without
	# restarting (for example, when it is sent a SIGHUP),
	# then implement that here.
	#
	start-stop-daemon --stop --signal 1 --quiet --pidfile $PIDFILE --name `basename "$DAEMON"` 
	return 0
}

case "$1" in
  start)
    log_daemon_msg "Starting $DESC " "$NAME"
    do_start
    case "$?" in
		0|1)
            log_end_msg 0
        ;;
		2)
            log_end_msg 1
            exit 2
        ;;
	esac
  ;;
  soft-start)
    log_daemon_msg "Soft starting $DESC " "$NAME"
    do_start
    case "$?" in
		0|1)
            log_end_msg 0
        ;;
		2)
            log_end_msg 1
            exit 0
        ;;
	esac
  ;;
  stop)
	log_daemon_msg "Stopping $DESC" "$NAME"
	do_stop
	case "$?" in
		0|1) log_end_msg 0 ;;
		2) log_end_msg 1 ;;
	esac
	;;
  status)
       status_of_proc "$DAEMON" "$NAME" && exit 0 || exit $?
       ;;
  reload)
	#
	# If do_reload() is not implemented then leave this commented out
	# and leave 'force-reload' as an alias for 'restart'.
	#
	log_daemon_msg "Reloading $DESC" "$NAME"
	do_reload
	log_end_msg $?
	;;
  restart|force-reload)
	#
	# If the "reload" option is implemented then remove the
	# 'force-reload' alias
	#
	log_daemon_msg "Restarting $DESC" "$NAME"
	do_stop
	case "$?" in
	  0|1)
		do_start
		case "$?" in
			0) log_end_msg 0 ;;
			1) # Old process is still running
                log_end_msg 1 
                exit 1
            ;;
			*) # Failed to start
                log_end_msg 1
                exit 1
            ;;
		esac
		;;
	  *)
	  	# Failed to stop
		log_end_msg 1
        exit 1
		;;
	esac
	;;
  *)
	echo "Usage: $SCRIPTNAME {start|stop|status|restart|reload|force-reload}" >&2
	exit 3
	;;
esac

:
