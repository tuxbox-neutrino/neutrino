export LD_LIBRARY_PATH=/var/lib
export PATH=${PATH}:/var/bin:/var/plugins

# Simple Neutrino start script

# Follow-up action after Neutrino exits (power off / reboot / restart).
#
# Neutrino writes the requested action as a single token into an action file.
# Older builds instead encode it in the exit status (1=poweroff, 2=reboot,
# 3=restart); we fall back to that when no action file is present, so any mix
# of old/new binary and old/new start script keeps working.

ACTION_FILE="${NEUTRINO_EXIT_ACTION_FILE:-/tmp/neutrino.exit-action}"

cd /tmp

while true; do
	# Drop a stale action file from a crashed earlier run before starting.
	rm -f "$ACTION_FILE"

	echo "Starting Neutrino"
	/bin/neutrino >/dev/null 2>&1; RET=$?
	sync
	echo "Neutrino exited with exit code $RET"

	ACTION=""
	if [ -r "$ACTION_FILE" ]; then
		read -r ACTION < "$ACTION_FILE"
		rm -f "$ACTION_FILE"
	fi

	if [ -z "$ACTION" ]; then
		# Legacy fallback: the exit status carries the action.
		case "$RET" in
			0) ACTION=none ;;
			1) ACTION=poweroff ;;
			2) ACTION=reboot ;;
			3) ACTION=restart ;;
			*) ACTION=panic ;;
		esac
	fi

	case "$ACTION" in
		none)     break ;;
		poweroff) poweroff; break ;;
		reboot)   reboot; break ;;
		restart)  continue ;;
		panic)    echo "Neutrino died, rebooting"; reboot -f; break ;;
		*)        echo "unknown exit action '$ACTION', ignoring"; break ;;
	esac
done
