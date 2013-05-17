/*-
  Copyright (C) 2009-2013 Pietro Cerutti <gahr@gahr.ch>

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

#include "history.h"
#include "completion.h"

#define XSTRING(x) STRING(x)
#define STRING(x) #x

struct hist_ {
	char    **hist_elem;
	size_t    curr_elem;
	size_t    nof_hist_elem;
};

hist_t
hist_init(void)
{
	hist_t  hist;
	char    history_file[MAX_CMD_LEN];
	char    buffer[MAX_CMD_LEN+1];
	FILE   *fd = NULL;
	int     rc, nof_chars, c;

	/*
	 * initialize structure
	 */
	if (!(hist = malloc(sizeof *hist)))
		goto error;

	/*
	 * initialize general members
	 */
	hist->hist_elem = NULL;
	hist->curr_elem = 0;
	hist->nof_hist_elem = 0;

	snprintf(history_file, MAX_CMD_LEN, "%s/%s", getenv("HOME"), "/.thingylaunch.history");

	if (!(fd = fopen(history_file, "r"))) {
		return (hist);
	}

	while ((rc = fscanf(fd, "%" XSTRING(MAX_CMD_LEN) "[^\n]%n", buffer, &nof_chars)) != EOF) {
		if ((c = fgetc(fd)) != '\n') {
			goto error;
		}
		if (rc == 0) {
			continue;
		}
		buffer[nof_chars] = '\0';
		hist->nof_hist_elem++;
		/* allocate space for new history member pointer */
		if (!(hist->hist_elem  = realloc(hist->hist_elem, hist->nof_hist_elem * sizeof (char *)))) {
			goto error;
		}
		/* allocate space for new history member */
		if (!(hist->hist_elem[hist->nof_hist_elem-1] = malloc(nof_chars+1))) {
			goto error;
		}
		strncpy(hist->hist_elem[hist->nof_hist_elem-1], buffer, nof_chars);
		hist->hist_elem[hist->nof_hist_elem-1][nof_chars] = '\0';
	}
	fclose(fd);
	fd = NULL;

	/* allocate space for the last dummy element */
	hist->nof_hist_elem++;
	if (!(hist->hist_elem = realloc(hist->hist_elem, hist->nof_hist_elem * sizeof (char *)))) {
		goto error;
	}
	hist->hist_elem[hist->nof_hist_elem-1] = NULL;
	hist->curr_elem = hist->nof_hist_elem-1;

	return (hist);

error:
	if (hist)
		hist_cleanup(hist);
	if (fd)
		fclose(fd);
	return (NULL);
}

void
hist_cleanup(hist_t hist)
{
	size_t i;

	for (i=0; i<hist->nof_hist_elem; i++) {
		free(hist->hist_elem[i]);
	}
	free(hist->hist_elem);
	free(hist);
}

char *
hist_next(hist_t hist)
{
	if (hist->nof_hist_elem == 0) {
		return (NULL);
	}
	return (hist->hist_elem[(++hist->curr_elem) % hist->nof_hist_elem]);
}

char *
hist_prev(hist_t hist)
{
	if (hist->nof_hist_elem == 0) {
		return (NULL);
	}
	return (hist->hist_elem[(--hist->curr_elem) % hist->nof_hist_elem]);
}

int
hist_save(hist_t hist, const char *cmd)
{
	size_t  i = 0;
	char    history_file[MAX_CMD_LEN];
	FILE   *fd;

	snprintf(history_file, MAX_CMD_LEN, "%s/%s", getenv("HOME"), "/.thingylaunch.history");

	if (!(fd = fopen(history_file, "w"))) {
		return (1);
	}

	if (hist->nof_hist_elem > MAX_HISTORY_LINES-2) {
		for (i=0; i<hist->nof_hist_elem - MAX_HISTORY_LINES; i++) {
			free(hist->hist_elem[i]);
		}
	}
	hist->hist_elem += i;
	hist->nof_hist_elem -= i;

	for (i=0; i<hist->nof_hist_elem; i++) {
		if (!hist->hist_elem[i]) { continue; }
		fprintf(fd, "%s\n", hist->hist_elem[i]);
	}
	fprintf(fd, "%s\n", cmd);

	fclose(fd);

	return (0);
}

void
hist_dump(hist_t hist)
{
	size_t i;

	puts("Dumping history elements");
	for (i=0; i<hist->nof_hist_elem; i++) {
		printf("[%4zu] %1s %s\n", i, hist->curr_elem == i ? "*" : " ",hist->hist_elem[i]);
	}
}
