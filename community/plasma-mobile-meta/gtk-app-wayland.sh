#!/bin/sh

# Most pure GTK3 apps use wayland by default, but some,
# like Firefox, need the backend to be explicitely selected.
export GDK_BACKEND=wayland
