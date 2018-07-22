/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2018
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "comm.h"

struct comm {
	int socket;
	int connection;

	struct comm_packet *current;
	uint8_t *cursor;
	size_t remaining;
};

/*
 * From the gnu docs
 * https://www.gnu.org/software/libc/manual/html_node/Local-Socket-Example.html
 */
static int make_named_socket (const char *filename)
{
	struct sockaddr_un name;
	int sock;
	size_t size;

	/* Create the socket. */
	sock = socket (PF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (sock < 0)
	{
		perror ("socket");
		exit (EXIT_FAILURE);
	}

	/* Bind a name to the socket. */
	name.sun_family = AF_LOCAL;
	strncpy (name.sun_path, filename, sizeof (name.sun_path));
	name.sun_path[sizeof (name.sun_path) - 1] = '\0';

	/* The size of the address is
	   the offset of the start of the filename,
	   plus its length (not including the terminating null byte).
	   Alternatively you can just do:
	   size = SUN_LEN (&name);
	   */
	size = (offsetof (struct sockaddr_un, sun_path)
			+ strlen (name.sun_path));
	if (bind (sock, (struct sockaddr *) &name, size) < 0)
	{
		perror ("bind");
		exit (EXIT_FAILURE);
	}

	return sock;
}

struct comm *comm_init(const char *socket)
{
	int ret;
	struct comm *comm = calloc(1, sizeof(*comm));
	if (!comm) {
		return NULL;
	}
	comm->socket = -1;
	comm->connection = -1;

	if (!access(socket, F_OK)) {
		fprintf(stderr, "Socket '%s' already exists, removing.\n", socket);
		ret = unlink(socket);
		if (ret) {
			perror("Unlink failed:");
			goto fail;
		}
	}

	ret = make_named_socket(socket);
	if (ret < 0) {
		goto fail;
	}
	comm->socket = ret;

	fprintf(stderr, "Listening on socket %s (%d)\n", socket, comm->socket);
	listen(comm->socket, 10);

	return comm;

fail:
	if (comm->socket >= 0) {
		close(comm->socket);
	}
	return NULL;
}

int comm_poll(struct comm *comm, struct comm_packet ***recv)
{
	int ret, npkts = 0;
	struct comm_packet **pkts = NULL;

	/* Try to open a connection if we don't have one */
	if (comm->connection < 0) {
		ret = accept4(comm->socket, NULL, NULL, SOCK_NONBLOCK);
		if (ret == -1) {
			if (errno != EWOULDBLOCK && errno != EAGAIN) {
				perror("Unexpected error in accept():");
				goto error;
			}
			return 0;
		}
		comm->connection = ret;

	}

	while (1) {
		/* Allocate a new packet if needed */
		if (!comm->current) {
			comm->current = calloc(1, sizeof(*comm->current));
			if (!comm->current) {
				return -ENOMEM;
			}
			comm->cursor = (uint8_t *)comm->current;
			comm->remaining = sizeof(*comm->current);
		}

		ret = read(comm->connection, comm->cursor, comm->remaining);
		if (ret == 0) {
			/* Client went away - clean up, but no error */
			close(comm->connection);
			comm->connection = -1;

			/* Discard current state */
			free(comm->current);
			comm->current = NULL;
			break;
		} else if (ret == -1) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				perror("Unexpected read error:");
				goto error;
			}
			break;
		} else if (ret == comm->remaining) {
			if (comm->cursor == (uint8_t *)comm->current) {
				/* We just received the header */
				comm->current = realloc(comm->current, sizeof(*comm->current) + comm->current->length);
				// FIXME: Handle alloc failure??

				comm->cursor = comm->current->data;
				comm->remaining = comm->current->length;
			} else {
				/* Packet finished */
				pkts = realloc(pkts, (npkts + 1) * sizeof(*pkts));
				// FIXME: Handle alloc failure??

				pkts[npkts] = comm->current;
				/* The alloc path will re-initialise everything */
				comm->current = NULL;

				npkts++;
			}
			/* Go around again. */
		} else {
			/* Short read */
			comm->remaining -= ret;
			comm->cursor += ret;
			break;
		}
	}

	*recv = pkts;

	return npkts;

error:
	if (comm->connection >= 0) {
		close(comm->connection);
		comm->connection = -1;
	}
	return -1;

}
