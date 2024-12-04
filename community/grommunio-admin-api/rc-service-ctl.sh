#!/bin/ash
# 
# This script is a wrapper for Alpine Linux openrc tools, i.e. rc-service, rc-update, rc-status
# It allows to control multiple services at once using systemd-like commands
# NOTE: The info command is compatible with the systemctl output expected by grommunio-admin-api
# 
# Copyright 2024 Miguel Da Silva, Contauro AG
# Distributed under the terms of the GNU General Public License, v2 or later
# 
# Version: 0.3 - 2024-05-20
# Last change: Fixed info command
#

usage() {
    echo >&2 "Wrapper for openrc tools to control multiple services at once"
    echo >&2
    echo >&2 "Usage: $(basename "$0") [options] <command> <service>"
    echo >&2
    echo >&2 "Commands: start | stop | reload | restart | | enable | disable | status | info |"
    echo >&2 "          try-restart | reload-or-restart | try-reload-or-restart"
    echo >&2 "Service: one or multiple services separate by blanks"
    echo >&2
    echo >&2 "Options:"
    echo >&2 " -q, --quiet			Run quietly"
    exit ${1:-0}
}

# Show usage by default
[ $# -eq 0 ] && usage

# Retrieve options and command
prog=""; cmd=""; opt=""; combined=0; try=0
while [ -z "$cmd" ]; do
    case "$1" in
        "start")   prog="rc-service"; cmd="start" ;;
        "stop")    prog="rc-service"; cmd="stop" ;;
        "reload")  prog="rc-service"; cmd="reload" ;;
        "restart") prog="rc-service"; cmd="restart" ;;
        "enable")  prog="rc-update";  cmd="add" ;;
        "disable") prog="rc-update";  cmd="del" ;;
        "status")  prog="rc-service"; cmd="status" ;;
        "info")    prog="internal";   cmd="info" ;;
        "try-restart")           prog="rc-service"; cmd="restart" try=1 ;;
        "reload-or-restart")     prog="rc-service"; cmd="reload-or-restart"; combined=1 ;;
        "try-reload-or-restart") prog="rc-service"; cmd="reload-or-restart"; combined=1; try=1 ;;
        -h|--help) usage ;;
        -q|--quiet) opt="-q"; shift ;;
        *) echo >&2 "ERROR: Unknown command."; exit 1 ;;
    esac
done

# Verify service input
if [ -z "$2" ] && [ "$cmd" != "status" ]; then
    echo >&2 "ERROR: Specify one or mulitple services as argument."
    exit 1
fi


# status command is allowed without service
[ $# -eq 1 ] && [ "$cmd" = "status" ] && rc-status && exit 0


# Loop over services
while [ -n "$2" ]; do

    # Remove suffix '.service' if available
    service=${2%%.service}

    # Retrieve initscript. If not found, skip this service.
    initscript=$(rc-service -r $service)
    if [ -z "$initscript" ]; then
        [ "$cmd" != "info" ] && echo -e >&2 "\e[1;31m * \e[0m$service: unknown service"
	shift
	continue
    fi

    # Retrieve service state
    servicestate=$(rc-service $service status | awk '{print $3}')

    # No actions if try-flag is set and the service is stopped
    [ $try -eq 1 ] && [ "$servicestate" = "stopped" ] && shift && continue


    ## Invoke external commands
    if [ $combined -eq 0 ]; then
        # Fix status output for multiple services
        statusfix=""
        [ "$cmd" = "status" ] && statusfix="| sed 's/status/$service/'"

        [ "$prog" = "rc-service" ] && eval $prog $opt $service $cmd $statusfix
        [ "$prog" = "rc-update" ]  && eval $prog $opt $cmd $service
    else
       # combined commands reload/restart
       if [ "$prog" = "rc-service" ] && [ "$cmd" = "reload-or-restart" ]; then
           eval $prog $opt $service "reload"
           [ $? -ne 0 ] && eval $prog $opt $service "restart"
       fi
    fi


    ## Process info command
    if [ "$prog" = "internal" ] && [ "$cmd" = "info" ]; then
        # Retrieve description from init.d file
        description=$(grep '^description=' $initscript | cut -d'=' -f2 | tr -d "\"")

        # Retrieve service unit state (enabled/disabled)
        unitstate="disabled"
        rc-update show | grep -q "$service |" && unitstate="enabled"

        # Translate service state to systemd-like activestate and substate
        case "$servicestate" in
            "started") activestate="active";   substate="running" ;;
            "stopped") activestate="inactive"; substate="dead" ;;
            "crashed") activestate="failed";   substate="failed" ;;
            *)         activestate="unknown";  substate="unknown" ;;
        esac

        # Print out required variables. Print a newline between items
        printf "Names=$service.service\n"
        printf "Description=$description\n"
        printf "ActiveState=$activestate\n"
        printf "SubState=$substate\n"
        printf "UnitFileState=$unitstate\n"
        printf "ActiveEnterTimestampMonotonic=0\n"
        printf "InactiveEnterTimestampMonotonic=0\n"
        [ -n "$3" ] &&  printf "\n"
    fi
    shift;
done

