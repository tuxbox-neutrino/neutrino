#!/bin/sh
# coreapi is consumed by the GUI, the HTTP layer and Lua, so it must not
# depend on any of them.
set -e
DIR="${1:-.}"
BAD=$(find "$DIR" -name "*.h" -o -name "*.hpp" -o -name "*.cpp" -o -name "*.c" \
	| xargs grep -nE "#[[:space:]]*include" 2>/dev/null \
	| grep -E "[<\"]([^\">]*/)?(gui|httpd|nhttpd)/|[<\"]([^\">]*/)?(lua|lualib|luaconf|lauxlib|luajit|luainstance)\.(h|hpp)|[<\"]([^\">]*/)?lua(jit)?[-0-9.]*/" \
	|| true)
if [ -n "$BAD" ]; then
	echo "layering violation in coreapi:" >&2
	echo "$BAD" >&2
	exit 1
fi
exit 0
