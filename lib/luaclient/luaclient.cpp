/*
	(C)2014 by martii

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <basicclient.h>

#include <luaclient.h>


class CLuaClient : private CBasicClient
{
	private:
		unsigned char   getVersion   () const { return LUACLIENT_VERSION; };
		const    char * getSocketName() const { return LUACLIENT_UDS_NAME; };
	public:
		bool Send(const char *data, const size_t size) { return send(0, data, size);}
		bool Recv(char *data, const size_t size) { return receive_data(data, size, true); }
		~CLuaClient() { close_connection(); }
};

int main(int argc, char** argv)
{
	char *cmd = strrchr(argv[0], '/');
	if (cmd)
		cmd++;
	else
		cmd = argv[0];
	if (!strcmp(cmd, "luaclient"))
		argv++, argc--;

	if (!*argv) {
		fprintf(stderr,
			"Usage: luaclient [command [arguments ...]]\n"
			"   or: command [arguments ...] (with command being a link to luaclient)\n");
		exit(-1);
	}

	size_t len[argc];
	size_t size = 0;
	for (int i = 0; i < argc; i++) {
		len[i] = strlen(argv[i]) + 1;
		size += len[i];
	}

	char data[size + sizeof(size)];
	char *b = data;
	memcpy(b, &size, sizeof(size));
	size += sizeof(size);
	b += sizeof(size);
	for (int i = 0; i < argc; i++) {
		memcpy(b, argv[i], len[i]);
		b += len[i];
	}

	CLuaClient client;
	int res = -1;
	const char *fun = NULL;
	char *resp = NULL;
	char *result = NULL;

        if (!client.Send(data, size)) {
		fun = "Send failed";
		goto fail;
	}
        if (!client.Recv((char *)&size, sizeof(size))) {
		fun = "Recv (1) failed";
		goto fail;
	}
		if(size)
			result = (char*) malloc(size);

        if (result && !client.Recv(result, size)) {
		fun = "Recv (2) failed";
		goto fail;
	}

	if (result[size - 1]) {
		fun = "unterminated result";
		goto fail;
	}
	res = atoi(result);
	resp = result + strlen(result) + 1;
	if (resp < result + size)
		printf("%s", resp);
	resp += strlen(resp) + 1;
	if (resp < result + size)
		fprintf(stderr, "%s", resp);
	if(result)
		free(result);
	exit(res);
fail:
	if(result)
		free(result);
	if (fun)
		fprintf(stderr, "luaclient: %s.\n", fun);
	exit(-1);
}
