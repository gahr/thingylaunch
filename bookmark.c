/*-
  Copyright (C) 2014 Pietro Cerutti <gahr@gahr.ch>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bookmark.h"
#include "completion.h"

typedef struct book_elem_ * book_elem_t;

struct book_elem_ {
	book_elem_t   next;
	char        * command;
	char          letter;
};

/* head of the linked list */
struct book_ {
	book_elem_t head;
};

book_t
book_init(void)
{
	book_t        book;
	book_elem_t   curr;
	char          book_file[MAX_CMD_LEN];
	FILE        * fd = NULL;
	int           rc, nof_chars, c;
	char          letter;
	char          command[MAX_CMD_LEN+1];

	/*
	 * initialize structure
	 */
	if (!(book = malloc(sizeof *book)))
		return NULL;

	book->head = NULL;

	snprintf(book_file, MAX_CMD_LEN, "%s/%s", getenv("HOME"), "/.thingylaunch.bookmarks");

	if (!(fd = fopen(book_file, "r"))) {
		return (book);
	}

	while ((rc = fscanf(fd, "%c %" XSTRING(MAX_CMD_LEN) "[^\n]%n", &letter, command, &nof_chars)) != EOF) {
		if ((c = fgetc(fd)) != '\n') {
			goto error;
		}
		if (rc == 0) {
			continue;
		}
		command[nof_chars] = '\0';
		/* allocate new element */
		if (!(curr = malloc(sizeof *curr))) {
			goto error;
		}
		if (!(curr->command = strdup(command))) {
			free(curr);
			goto error;
		}
		curr->letter = letter;
		curr->next = book->head;
		book->head = curr;
	}
	fclose(fd);
	fd = NULL;

	return (book);

error:
	if (book)
		book_cleanup(book);
	if (fd)
		fclose(fd);
	return (NULL);
}

void
book_cleanup(book_t book)
{
	book_elem_t curr = book->head;
	book_elem_t next;

	while (curr != NULL) {
		next = curr->next;
		free(curr->command);
		free(curr);
		curr = next;
	}
	free(book);
}

char *
book_lookup(book_t book, char letter)
{
	book_elem_t curr = book->head;

	while (curr != NULL) {
		if (curr->letter == letter)
			return (curr->command);
		curr = curr->next;
	}
	return (NULL);
}
