#!/bin/bash
# usage: cut -d' ' -f1 english.locale | LC_ALL=C sort | uniq | ./helpers/create-locals_intern.h
cat > locals_intern.h <<EOH
#ifndef __locals_intern__
#define __locals_intern__

/*
 * \$Id\$
 *
 * (C) 2004 by thegoodguy <thegoodguy@berlios.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

const char * locale_real_names[] =
{
EOH
printf "\t\"INTERNAL ERROR - PLEASE REPORT\"" >> locals_intern.h

while read id; do
	printf ",\n\t\"$id\"" >> locals_intern.h
done

cat >> locals_intern.h <<EOF

};
#endif
EOF
