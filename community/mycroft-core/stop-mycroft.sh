#!/bin/sh

script=${0}
script=${script##*/}

help() {
	echo "${script}:  Mycroft service stopper"
	echo "usage: ${script} [service]"
	echo
	echo "Service:"
	echo "  all       ends core services: bus, audio, skills, voice"
	echo "  (none)    same as \"all\""
	echo "  bus       stop the Mycroft messagebus service"
	echo "  audio     stop the audio playback service"
	echo "  skills    stop the skill service"
	echo "  voice     stop voice capture service"
	echo "  enclosure stop enclosure (hardware/gui interface) service"
	echo
	echo "Examples:"
	echo "  ${script}"
	echo "  ${script} audio"

	exit 0
}

process_running() {
	if [ "$( pgrep -f "python3 ${1}" )" ]; then
		return 0
	else
		return 1
	fi
}

end_process() {
	if process_running "$1"; then
		# Find the process by name, only returning the oldest if it has children
		pid=$( pgrep -o -f "python3 ${1}" )
		echo "Stopping $1 (${pid})..."
		kill -SIGINT "${pid}"

		# Wait up to 5 seconds (50 * 0.1) for process to stop
		c=1
		while [ $c -le 50 ]; do
			if process_running "$1"; then
				sleep 0.1
				c=$((c + 1))
			else
				c=999 # end loop
			fi
		done

		if process_running "$1"; then
			echo "Failed to stop."
			pid=$( pgrep -o -f "python3 ${1}" )
			echo "  Killing $1 (${pid})..."
			kill -9 "${pid}"
			echo "Killed."
			result=120
		else
			echo "Stopped."
			if [ $result -eq 0 ] ; then
				result=100
			fi
		fi
	fi
}

result=0 # default, no change

OPT=$1
shift

case ${OPT} in
	"all"|"")
		echo "Stopping all mycroft-core services"
		end_process "/usr/bin/mycroft-skills"
		end_process "/usr/bin/mycroft-audio"
		end_process "/usr/bin/mycroft-speech-client"
		end_process "/usr/bin/mycroft-enclosure-client"
		end_process "/usr/bin/mycroft-messagebus"
		;;
	"bus")
		end_process "/usr/bin/mycroft-messagebus"
		;;
	"audio")
		end_process "/usr/bin/mycroft-audio"
		;;
	"skills")
		end_process "/usr/bin/mycroft-speech"
		;;
	"voice")
		end_process "/usr/bin/mycroft-speech-client"
		;;
	"enclosure")
		end_process "/usr/bin/mycroft-enclosure-client"
		;;
	*)
		help
		;;
esac

# Exit codes:
#	0   if nothing changed (e.g. --help or no process was running)
#	100 at least one process was stopped
#	120 if any process had to be killed
exit $result
