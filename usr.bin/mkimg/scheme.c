/*-
 * Copyright (c) 2013,2014 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/linker_set.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "image.h"
#include "mkimg.h"
#include "scheme.h"

static struct {
	const char *name;
	enum alias alias;
} scheme_alias[] = {
	{ "ebr", ALIAS_EBR },
	{ "efi", ALIAS_EFI },
	{ "fat32", ALIAS_FAT32 },
	{ "freebsd", ALIAS_FREEBSD },
	{ "freebsd-boot", ALIAS_FREEBSD_BOOT },
	{ "freebsd-nandfs", ALIAS_FREEBSD_NANDFS },
	{ "freebsd-swap", ALIAS_FREEBSD_SWAP },
	{ "freebsd-ufs", ALIAS_FREEBSD_UFS },
	{ "freebsd-vinum", ALIAS_FREEBSD_VINUM },
	{ "freebsd-zfs", ALIAS_FREEBSD_ZFS },
	{ "mbr", ALIAS_MBR },
	{ NULL, ALIAS_NONE }		/* Keep last! */
};

static struct mkimg_scheme *scheme;
static void *bootcode;

static enum alias
scheme_parse_alias(const char *name)
{
	u_int idx;

	idx = 0;
	while (scheme_alias[idx].name != NULL) {
		if (strcasecmp(scheme_alias[idx].name, name) == 0)
			return (scheme_alias[idx].alias);
		idx++;
	}
	return (ALIAS_NONE);
}

int
scheme_select(const char *spec)
{
	struct mkimg_scheme *s, **iter;

	SET_FOREACH(iter, schemes) {
		s = *iter;
		if (strcasecmp(spec, s->name) == 0) {
			scheme = s;
			return (0);
		}
	}
	return (EINVAL);
}

struct mkimg_scheme *
scheme_selected(void)
{

	return (scheme);
}

int
scheme_bootcode(int fd)
{
	struct stat sb;

	if (scheme->bootcode == 0)
		return (ENXIO);

	if (fstat(fd, &sb) == -1)
		return (errno);
	if (sb.st_size > scheme->bootcode)
		return (EFBIG);

	bootcode = malloc(scheme->bootcode);
	if (bootcode == NULL)
		return (ENOMEM);
	memset(bootcode, 0, scheme->bootcode);
	if (read(fd, bootcode, sb.st_size) != sb.st_size) {
		free(bootcode);
		bootcode = NULL;
		return (errno);
	}
	return (0);
}

int
scheme_check_part(struct part *p)
{
	struct mkimg_alias *iter;
	enum alias alias;

	/* Check the partition type alias */
	alias = scheme_parse_alias(p->alias);
	if (alias == ALIAS_NONE)
		return (EINVAL);

	iter = scheme->aliases;
	while (iter->alias != ALIAS_NONE) {
		if (alias == iter->alias)
			break;
		iter++;
	}
	if (iter->alias == ALIAS_NONE)
		return (EINVAL);
	p->type = iter->type;

	/* Validate the optional label. */
	if (p->label != NULL) {
		if (strlen(p->label) > scheme->labellen)
			return (EINVAL);
	}

	return (0);
}

u_int
scheme_max_parts(void)
{

	return (scheme->nparts);
}

u_int
scheme_max_secsz(void)
{

	return (scheme->maxsecsz);
}

lba_t
scheme_metadata(u_int where, lba_t start)
{

	return (scheme->metadata(where, start));
}

int
scheme_write(lba_t end)
{
	int error;

	error = scheme->write(end, bootcode);
	return (error);
}
