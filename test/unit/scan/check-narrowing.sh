#!/bin/sh
# The router's two narrowing guards, run at the ABI the boxes have.
#
# Not part of make check and deliberately so. Those guards compare a value with
# itself narrowed to a long, and where a long is eight bytes the comparison is
# between a value and itself: the compiler removes it, the object is byte identical
# whether the guard is written or not, and no case anywhere can be made to fail over
# it. The suite runs on such a machine. The boxes do not, and neither does the image
# this project keeps for the oldest compiler it supports, which is armv7l with a four
# byte long, size_t and time_t.
#
# So this is run by hand, in that image, over this tree:
#
#   docker run --rm -v <the build volume>:/build -v <the workspace>:/src \
#       ni-gcc49:jessie sh /src/ni-neutrino/test/unit/scan/check-narrowing.sh
#
# It refuses to run where a long is eight bytes rather than printing a pass. A check
# that answers the same on a machine that cannot hold the fault is not a check, and
# this project has been bitten by that shape eleven times.
set -e
LC_ALL=C
export LC_ALL

CXX=${CXX:-g++}
HERE=`dirname "$0"`
SRC=`cd "$HERE/../../.." && pwd`
OUT=${OUT:-/tmp/narrowing}

mkdir -p "$OUT"

cat > "$OUT/width.cpp" <<'WIDTH'
#include <cstdio>
int main() { std::printf("%u\n", (unsigned) sizeof(long)); return 0; }
WIDTH

$CXX -o "$OUT/width" "$OUT/width.cpp"
width=`"$OUT/width"`
if [ "$width" != "4" ]; then
	echo "check-narrowing.sh: this compiler's long is $width bytes and the guards" >&2
	echo "  it drives are removed by the compiler where it is eight, so a run here" >&2
	echo "  would report a pass about code that is not in the binary" >&2
	exit 1
fi

# The router and what it links against, and nothing else. allRoutes and the
# refusal sentence come from files this does not need, so they are written here:
# what is being driven is one table handed in by the caller.
cat > "$OUT/stubs.cpp" <<'STUBS'
#include "httpd/endpoint.h"
#include "httpd/router.h"

namespace httpd
{

const RouteTable *const *allRoutes(size_t *table_count)
{
	if (table_count != NULL)
		*table_count = 0;
	return NULL;
}

void setRoutesForTest(const RouteTable *)
{
}

const char *notPermittedDetail()
{
	return "this endpoint is not open to this caller";
}

} // namespace httpd
STUBS

FLAGS="-I$SRC/src/httpd -I$SRC/src -I$SRC -std=c++11 -fno-rtti -fexceptions -O2 -g -Wall -Wextra"

$CXX $FLAGS -o "$OUT/narrow-probe" \
	"$SRC/test/unit/support/narrow-probe.cpp" \
	"$OUT/stubs.cpp" \
	"$SRC/src/httpd/router.cpp" \
	"$SRC/src/httpd/request.cpp" \
	"$SRC/src/httpd/status.cpp" \
	"$SRC/src/httpd/json.cpp" \
	"$SRC/src/httpd/http.cpp"

"$OUT/narrow-probe"
