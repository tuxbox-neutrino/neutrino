#!/bin/bash
# usage: cut -d' ' -f1 english.locale | LC_ALL=C sort | uniq | tr [:lower:] [:upper:] | tr \. \_  | tr \- \_ | tr -d \? | ./helpers/create-locals.h
cat > locals.h <<EOH
#ifndef __locals__
#define __locals__

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

typedef enum
{
EOH
printf "\tNONEXISTANT_LOCALE" >> locals.h

while read id; do
	printf ",\n\tLOCALE_$id" >> locals.h;
done

cat >> locals.h <<EOF

} neutrino_locale_t;
#endif
EOF
