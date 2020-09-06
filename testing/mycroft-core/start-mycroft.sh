#!/bin/sh

script=${0}
script=${script##*/}

help() {
	echo "${script}:  Mycroft command/service launcher"
	echo "usage: ${script} [COMMAND] [restart] [params]"
	echo
	echo "Services COMMANDs:"
	echo "  all                      runs core services: bus, audio, skills, voice"
	echo "  audio                    the audio playback service"
	echo "  bus                      the messagebus service"
	echo "  skills                   the skill service"
	echo "  voice                    voice capture service"
	echo "  enclosure                mark_1 enclosure service"
	echo
	echo "Options:"
	echo "  restart                  (optional) Force the service to restart if running"
	echo
	echo "Examples:"
	echo "  ${script} all"
	echo "  ${script} all restart"
	echo "  ${script} bus"
	echo "  ${script} voice"

	exit 1
}

name_to_script_path() {
	case ${1} in
		"bus")		_module="/usr/bin/mycroft-messagebus" ;;
		"skills")	_module="/usr/bin/mycroft-skills" ;;
		"audio")	_module="/usr/bin/mycroft-audio" ;;
		"voice")	_module="/usr/bin/mycroft-speech-client" ;;
		"enclosure")	_module="/usr/bin/mycroft-enclosure-client" ;;

		*)
			echo "Error: Unknown name '${1}'"
			exit 1
	esac
}

launch_background() {
	name_to_script_path "${1}"

	if pgrep -f "python3 ${_module}" > /dev/null; then
		if ($_force_restart); then
			echo "Restarting: ${1}"
			stop-mycroft "${1}"
		else
			# Already running, no need to restart
			return
		fi
	else
		echo "Starting background service $1"
	fi

	# Security warning/reminder for the user
	if [ "${1}" = "bus" ] ; then
		echo "CAUTION: The Mycroft bus is an open websocket with no built-in security"
		echo "         measures.  You are responsible for protecting the local port"
		echo "         8181 with a firewall as appropriate."
	fi

	# Launch process in background
	# Send logs to XDG Base Directories cache location
	if [ ! -z ${XDG_CACHE_HOME+x} ]; then
		logdir="$XDG_CACHE_HOME/mycroft/${1}.log"
	else
		logdir="$HOME/.cache/mycroft/${1}.log"
	fi

	${_module} "$_params" >> "$logdir" 2>&1 &
}

launch_all() {
	echo "Starting all mycroft-core services"
	launch_background bus
	launch_background skills
	launch_background audio
	launch_background voice
	launch_background enclosure
}

_opt=$1
_force_restart=false
shift
if [ "${1}" = "restart" ] || [ "${_opt}" = "restart" ]; then
	_force_restart=true
	if [ "${_opt}" = "restart" ]; then
		# Support "start-mycroft restart all" as well as "start-mycroft all restart"
		_opt=$1
	fi
	shift
fi
_params=$*

case ${_opt} in
	"all")
		launch_all
		;;
	"bus")
		launch_background "${_opt}"
		;;
	"audio")
		launch_background "${_opt}"
		;;
	"skills")
		launch_background "${_opt}"
		;;
	"voice")
		launch_background "${_opt}"
		;;
	"enclosure")
		launch-background "${_opt}"
		;;
	*)
		help
		;;
esac
