#!/bin/sh
# A row that names a field of the wrong sort has to stop the build, and the only
# way to know that it does is to write one and watch it fail. Two are written
# below, one of each direction, and this refuses a compiler that accepts either.
#
# The guarantee is the whole reason a row carries functions rather than an
# offset, so it is checked here rather than described in a comment.
set -e
CXX="$1"
shift

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/good.cpp" <<'PROBE'
#include "coreapi/settings/settingsfield.h"
using namespace coreapi;
// What a row of the kind whose value a daemon holds is written with. Nothing
// calls them; the probe is about what compiles.
bool probeAsk(long &) { return false; }
bool probeTell(long) { return false; }
const Descriptor row[] = {
	{ "k", ValueType::Int, "s", "l", NULL, 0, 2000, NULL, 0, 450, NULL, false, false,
	  COREAPI_ALWAYS, COREAPI_NUMBER_FIELD(repeat_blocker) },
	{ "t", ValueType::String, "s", "l", NULL, 0, 0, NULL, 0, 0, "", false, false,
	  COREAPI_ALWAYS, COREAPI_TEXT_FIELD(language) },
	{ "i", ValueType::String, "s", "l", NULL, 0, 0, NULL, 0, 0, "0", false, false,
	  COREAPI_ALWAYS, COREAPI_CHANNEL_ID_FIELD(startchanneltv_id) },
	{ "b", ValueType::Bool, "s", "l", NULL, 0, 1, NULL, 0, 0, NULL, false, false,
	  COREAPI_ALWAYS, COREAPI_MASK_BIT_FIELD(recording_audio_pids_std,
						 recording_audio_pids_default, 1) },
	{ "d", ValueType::Int, "s", "l", NULL, 0, 99, NULL, 0, 0, NULL, false, false,
	  COREAPI_ALWAYS, COREAPI_SERVICE_FIELD(record_safety_time_before, probeAsk, probeTell) },
};
const Descriptor *first() { return row; }
PROBE

# A number where the field is text, and text where the field is a number.
sed 's/COREAPI_NUMBER_FIELD(repeat_blocker)/COREAPI_NUMBER_FIELD(language)/' "$tmp/good.cpp" > "$tmp/number_over_text.cpp"
sed 's/COREAPI_TEXT_FIELD(language)/COREAPI_TEXT_FIELD(repeat_blocker)/' "$tmp/good.cpp" > "$tmp/text_over_number.cpp"
# And a field wider than the long a value travels in, which is the one kind of
# truncation that would show on the box and on nothing else.
sed 's/COREAPI_NUMBER_FIELD(repeat_blocker)/COREAPI_NUMBER_FIELD(startchanneltv_id)/' "$tmp/good.cpp" > "$tmp/wider_than_a_long.cpp"
# An identifier is sixty four bits, and a row naming a narrower field as one
# would answer half of something that is not an identifier at all.
sed 's/COREAPI_CHANNEL_ID_FIELD(startchanneltv_id)/COREAPI_CHANNEL_ID_FIELD(repeat_blocker)/' "$tmp/good.cpp" > "$tmp/id_over_a_narrow_field.cpp"
# The three kinds whose row names a member the value is not in still have to
# name a member: the name is what every check outside the compiler reads, and
# one that is not a member holds to nothing.
sed 's/COREAPI_MASK_BIT_FIELD(recording_audio_pids_std,/COREAPI_MASK_BIT_FIELD(no_such_member_4711,/' "$tmp/good.cpp" > "$tmp/mask_bit_names_nothing.cpp"
sed 's/COREAPI_SERVICE_FIELD(record_safety_time_before,/COREAPI_SERVICE_FIELD(no_such_member_4711,/' "$tmp/good.cpp" > "$tmp/service_names_nothing.cpp"

if ! $CXX "$@" -c -o "$tmp/out.o" "$tmp/good.cpp" > "$tmp/log" 2>&1; then
	echo "a row naming its own kind of field does not compile:" >&2
	cat "$tmp/log" >&2
	exit 1
fi

for bad in number_over_text text_over_number wider_than_a_long \
	   id_over_a_narrow_field mask_bit_names_nothing service_names_nothing; do
	if $CXX "$@" -c -o "$tmp/out.o" "$tmp/$bad.cpp" > "$tmp/log" 2>&1; then
		echo "a row naming a field of the wrong sort compiled: $bad" >&2
		exit 1
	fi
done
exit 0
