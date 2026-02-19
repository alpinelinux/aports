#!/bin/sh
# Script to check that all apps in src/nextcloud/apps are covered by
# either _apps or provides variables in APKBUILD

set -e

APKBUILD="${1:-../../APKBUILD}"
APPS_DIR="${2:-./apps}"

# List of apps to ignore (never report as missing)
# These apps are intentionally not packaged or handled specially
IGNORE_APPS="
updatenotification
"

if [ ! -f "$APKBUILD" ]; then
    echo "Error: APKBUILD file not found at: $APKBUILD" >&2
    exit 1
fi

if [ ! -d "$APPS_DIR" ]; then
    echo "Error: Apps directory not found at: $APPS_DIR" >&2
    echo "Note: This directory is created during the build process." >&2
    echo "Run 'abuild unpack' first to extract the source." >&2
    exit 1
fi

# Extract app names from _apps variable in APKBUILD
# This extracts the multi-line _apps variable content
extract_apps_variable() {
    awk '
        /^_apps="/ {
            in_apps = 1
            # Extract content after _apps="
            line = $0
            sub(/^_apps="/, "", line)
            if (line ~ /"$/) {
                # Single line definition
                sub(/"$/, "", line)
                print line
                exit
            } else {
                print line
            }
            next
        }
        in_apps {
            if ($0 ~ /"$/) {
                # End of _apps variable
                line = $0
                sub(/"$/, "", line)
                print line
                exit
            } else {
                print $0
            }
        }
    ' "$APKBUILD" | grep -v '^[[:space:]]*$' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
}

# Extract app names from provides variable
# Looking for patterns like: $_pkgname-appname= or $pkgname-appname=
extract_provides_apps() {
    grep -E '\$(_pkgname|pkgname)-[a-z_0-9]+=' "$APKBUILD" | \
        sed -E 's/.*\$(_pkgname|pkgname)-([a-z_0-9]+)=.*/\2/' | \
        sort -u
}

# Get all apps from _apps variable
echo "==> Extracting apps from _apps variable..."
APPS_FROM_VARIABLE=$(extract_apps_variable)
echo "$APPS_FROM_VARIABLE" | sed 's/^/    /'

# Get all apps from provides
echo ""
echo "==> Extracting apps from provides variable..."
APPS_FROM_PROVIDES=$(extract_provides_apps)
echo "$APPS_FROM_PROVIDES" | sed 's/^/    /'

# Combine both lists
echo ""
echo "==> Combining _apps and provides lists..."
ALL_DECLARED_APPS=$(printf "%s\n%s\n" "$APPS_FROM_VARIABLE" "$APPS_FROM_PROVIDES" | sort -u | grep -v '^$')
echo "$ALL_DECLARED_APPS" | sed 's/^/    /'

# Get all directories from src/nextcloud/apps
echo ""
echo "==> Scanning directories in $APPS_DIR..."
ACTUAL_APPS=$(find "$APPS_DIR" -mindepth 1 -maxdepth 1 -type d -exec basename {} \; | sort)
echo "$ACTUAL_APPS" | sed 's/^/    /'

# Find apps in filesystem but not in APKBUILD
echo ""
echo "==> Checking for undeclared apps..."
MISSING_APPS=""
MISSING_COUNT=0

for app in $ACTUAL_APPS; do
    # Check if app is in ignore list
    if echo "$IGNORE_APPS" | grep -q "^${app}$"; then
        echo "    Ignoring: $app"
        continue
    fi

    if ! echo "$ALL_DECLARED_APPS" | grep -q "^${app}$"; then
        MISSING_APPS="${MISSING_APPS}${app}\n"
        MISSING_COUNT=$((MISSING_COUNT + 1))
    fi
done

if [ $MISSING_COUNT -gt 0 ]; then
    echo ""
    echo "ERROR: Found $MISSING_COUNT app(s) in $APPS_DIR that are NOT declared in APKBUILD:"
    echo ""
    printf "%s" "$MISSING_APPS" | sed 's/^/    /'
    echo ""
    echo "These apps should be added to either:"
    echo "  - The _apps variable (if they should be packaged as subpackages), or"
    echo "  - The provides variable (if they are core apps that should always be installed), or"
    echo "  - The IGNORE_APPS list in this script (if they should never be packaged)"
    echo ""
    exit 1
else
    echo "OK: All apps in $APPS_DIR are declared in APKBUILD"
    echo ""
    exit 0
fi
