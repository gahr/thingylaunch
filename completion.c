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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "completion.h"

#ifdef LINT
extern char *strdup(const char *);
#endif

static int comp_compare(const void *, const void *);

typedef struct {
	char  *pathname;
	char  *filename;
	char  *fullpath;
} exec_elem_t;

struct comp_ {
	char        **path_elem;
	exec_elem_t  *exec_elem;
	exec_elem_t  *curr_elem;
	char         *last_hint;
	size_t        last_hint_len;
	size_t        nof_path_elem;
	size_t        nof_exec_elem;
	uid_t         uid;
	gid_t         gid;
};


comp_t
comp_init(void)
{
	comp_t       comp;
	exec_elem_t *curr_elem;
	char        *curr_path;
	char        *path;
	char        *path_elem_end;
	size_t       path_len;
	size_t       curr_path_len;

	char           buffer[MAX_CMD_LEN+1];
	struct dirent *dp;
	struct stat    sb;
	DIR           *dirp = NULL;


	/*
	 * initialize structure
	 */
	if (!(comp = malloc(sizeof *comp)))
		goto error;

	/*
	 * initialize general members
	 */
	comp->path_elem = NULL;
	comp->exec_elem = NULL;
	comp->curr_elem = NULL;
	comp->last_hint = NULL;
	comp->last_hint_len = 0;
	comp->nof_path_elem = 0;
	comp->nof_exec_elem = 0;
	comp->uid = getuid();
	comp->gid = getgid();

	/*
	 * initialize path elements
	 */

	/* get PATH env */
	path = strdup(getenv("PATH"));
	if (!path) {
		goto error;
	}
	path_elem_end   = NULL;
	path_len = strlen(path);

	while ((curr_path = strsep(&path, ":")) != NULL) {

		comp->nof_path_elem++;
		curr_path_len = strlen(curr_path);

		/* allocate space for a new path pointer */
		if (!(comp->path_elem = realloc(comp->path_elem, comp->nof_path_elem * sizeof (char *)))) {
			goto error;
		}

		/* copy the new path string */
		comp->path_elem[comp->nof_path_elem-1] = strdup(curr_path);

		/*
		 * look for binaries in current path
		 */

		/* open the directory pointed to by path */
		if (!(dirp = opendir(curr_path))) {
			continue;
		}

		/* traverse directory */
		while ((dp = readdir(dirp))) {

			/* create a 'path/file' string and check whether we can access meta-information */
			snprintf(buffer, MAX_CMD_LEN, "%s/%s", curr_path, dp->d_name);
			if (stat(buffer, &sb) == -1) {
				continue;
			}

			/* a regular, executable file*/
			if (((sb.st_mode & S_IFREG) == S_IFREG) &&
					((sb.st_uid == comp->uid && (sb.st_mode & S_IXUSR) == S_IXUSR) ||
					 (sb.st_gid == comp->gid && (sb.st_mode & S_IXGRP) == S_IXGRP) ||
					 ((sb.st_mode & S_IXOTH) == S_IXOTH)))
			{
				/* allocate a new executable element */
				comp->nof_exec_elem++;
				if (!(comp->exec_elem = realloc(comp->exec_elem, comp->nof_exec_elem * sizeof (*comp->exec_elem)))) {
					goto error;
				}
				curr_elem = &comp->exec_elem[comp->nof_exec_elem-1];
				/* point this new executable path to the current path element */
				curr_elem->pathname = curr_path;
				/* copy the full path of the new executable element */
				if (!(curr_elem->fullpath = strdup(buffer))) {
					goto error;
				}
				/* set the filename of the new executable element */
				curr_elem->filename = &curr_elem->fullpath[curr_path_len+1];
			}
		}
		closedir(dirp);
		dirp = NULL;
	}

	/* sort exec elements */
	if (comp->exec_elem)
		qsort(comp->exec_elem, comp->nof_exec_elem, sizeof(*comp->exec_elem), comp_compare);

	free(path);

	return (comp);

error:
	if (comp)
		comp_cleanup(comp);
	if (dirp)
		closedir(dirp);
	return (NULL);
}

void
comp_cleanup(comp_t comp)
{
	size_t i;

	/* free exec elements */
	for (i=0; i<comp->nof_exec_elem; i++) {
		free(comp->exec_elem[i].fullpath);
	}
	free(comp->exec_elem);

	/* free path elements */
	for (i=0; i<comp->nof_path_elem; i++) {
		free(comp->path_elem[i]);
	}
	free(comp->path_elem);

	/* free hint */
	free(comp->last_hint);

	/* free main structure */
	free(comp);
}

void
comp_reset(comp_t comp)
{
	free(comp->last_hint);
	comp->last_hint = NULL;
}

char *
comp_next(comp_t comp, char *hint)
{
	size_t i;

	if (!hint)
		return (NULL);

	if (!comp->last_hint) {
		if(!(comp->last_hint = strdup(hint))) {
			return (NULL);
		}
		comp->last_hint_len = strlen(comp->last_hint);
	}

	/* if we're not at the end of the list and the next one matches, return it */
	if (comp->curr_elem != NULL &&
			comp->curr_elem != &comp->exec_elem[comp->nof_exec_elem-1] &&
			strncmp((++comp->curr_elem)->filename, comp->last_hint, comp->last_hint_len) == 0) {
		return (comp->curr_elem->fullpath);
	}

	/* start over */
	comp->curr_elem = NULL;
	for(i=0; i<comp->nof_exec_elem; i++) {
		if (!strncmp(comp->exec_elem[i].filename, comp->last_hint, comp->last_hint_len)) {
			comp->curr_elem = &comp->exec_elem[i];
			return (comp->curr_elem->fullpath);
		}
	}

	return (NULL);
}

void
comp_dump(comp_t comp)
{
	size_t i;

	puts("Dumping path elements:");
	for (i=0; i<comp->nof_path_elem; i++)
		printf("[%4zu] %s\n", i, comp->path_elem[i]);

	puts("Dumping binaries:");
	for (i=0; i<comp->nof_exec_elem; i++)
		printf("[%4zu] %s/%s\n", i, comp->exec_elem[i].pathname, comp->exec_elem[i].filename);
}

static int
comp_compare(const void *vexec_elem_1, const void *vexec_elem_2)
{
	const exec_elem_t *a = vexec_elem_1;
	const exec_elem_t *b = vexec_elem_2;

	return (strcmp(a->filename, b->filename));
}
