#!/usr/bin/env sh

set -e

# This is always set in the Dart entrypoint script provided by Flutter,
# but not for standalone Dart. Emulating this behavior by only exporting
# if flutter is installed.
FLUTTER_ROOT=/usr/lib/flutter
if [ -f "$FLUTTER_ROOT/version" ]; then
	export FLUTTER_ROOT
fi

# Test if running as superuser – but don't warn if running within Docker or CI.
if [ "$(id -u)" = "0" ] && ! [ -f /.dockerenv ] && [ "$CI" != "true" ] && [ "$BOT" != "true" ] && [ "$CONTINUOUS_INTEGRATION" != "true" ]; then
  >&2 echo "   Woah! You appear to be trying to run dart as root."
  >&2 echo "   We strongly recommend running dart without superuser privileges."
  >&2 echo "  /"
  >&2 echo "📎"
fi

exec /usr/lib/dart/bin/dart "$@"
