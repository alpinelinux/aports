
#colors
if [ -n "$USE_COLORS" ]; then
	NORMAL="\033[1;0m"
	STRONG="\033[1;1m"
	RED="\033[1;31m"
	GREEN="\033[1;32m"
	YELLOW="\033[1;33m"
	BLUE="\033[1;34m"
fi	
	

# functions
msg() {
	local prompt="$GREEN>>>${NORMAL}"
	local fake="${FAKEROOTKEY:+${BLUE}*${NORMAL}}"
	local name="${STRONG}${subpkgname:-$pkgname}${NORMAL}"
	[ -z "$quiet" ] && printf "${prompt} ${name}${fake}: $@\n" >&2
}

warning() {
	local prompt="${YELLOW}>>> WARNING:${NORMAL}"
	local fake="${FAKEROOTKEY:+${BLUE}*${NORMAL}}"
	local name="${STRONG}${subpkgname:-$pkgname}${NORMAL}"
	printf "${prompt} ${name}${fake}: $@\n" >&2
}

error() {
	local prompt="${RED}>>> ERROR:${NORMAL}"
	local fake="${FAKEROOTKEY:+${BLUE}*${NORMAL}}"
	local name="${STRONG}${subpkgname:-$pkgname}${NORMAL}"
	printf "${prompt} ${name}${fake}: $@\n" >&2
}

