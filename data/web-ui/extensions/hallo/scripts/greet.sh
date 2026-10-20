#!/bin/sh
# greet.sh - the script of the Hello extension: says hello to whom it is given
#
# Run by the box over POST /api/v1/webui/extensions/hallo/scripts/greet,
# with the arguments the manifest's action names, each as one argument and
# without a shell in between. What it prints is what the page shows.
name=${1:-world}
printf 'Hello, %s! This is %s.\n' "$name" "$(hostname)"
