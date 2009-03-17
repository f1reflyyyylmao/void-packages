/*-
 * Copyright (c) 2008-2009 Juan Romero Pardines.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/utsname.h>

#include <xbps_api.h>

static const char *rootdir;
static int flags;

int
xbps_check_file_hash(const char *path, const char *sha256)
{
	SHA256_CTX ctx;
	const char *res;
	uint8_t buf[BUFSIZ * 20], digest[SHA256_DIGEST_LENGTH * 2 + 1];
	ssize_t bytes;
	int fd, rv = 0;

	if ((fd = open(path, O_RDONLY)) == -1)
		return errno;

	SHA256_Init(&ctx);
	while ((bytes = read(fd, buf, sizeof(buf))) > 0)
		SHA256_Update(&ctx, buf, (size_t)bytes);
	res = SHA256_End(&ctx, digest);

	if (strcmp(sha256, res))
		rv = ERANGE;

	(void)close(fd);

	return rv;
}

int
xbps_check_pkg_file_hash(prop_dictionary_t pkgd, const char *repoloc)
{
	const char *sha256, *arch, *filename;
	char *binfile, *path;
	int rv = 0;

	assert(repoloc != NULL);

	if (!prop_dictionary_get_cstring_nocopy(pkgd, "filename", &filename))
		return EINVAL;

	if (!prop_dictionary_get_cstring_nocopy(pkgd, "filename-sha256",
	    &sha256))
		return EINVAL;

	if (!prop_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch))
		return EINVAL;

	path = xbps_append_full_path(false, repoloc, arch);
	if (path == NULL)
		return EINVAL;

	binfile = xbps_append_full_path(false, path, filename);
	if (binfile == NULL) {
		free(path);
		return EINVAL;
	}
	free(path);

	printf("Checking SHA256 for %s ... ", filename);
	(void)fflush(stdout);

	rv = xbps_check_file_hash(binfile, sha256);
	if (rv != 0 && rv != ERANGE)
		printf("failed (%s)\n", strerror(rv));
	else if (rv == ERANGE)
		printf("failed! aborting installation.\n");
	else if (rv == 0)
		printf("ok.\n");

	(void)fflush(stdout);

	free(binfile);
	return rv;
}

int
xbps_check_is_installed_pkg(const char *pkg)
{
	prop_dictionary_t dict;
	const char *reqver, *instver;
	char *pkgname;
	int rv = 0;

	assert(pkg != NULL);

	pkgname = xbps_get_pkg_name(pkg);
	reqver = xbps_get_pkg_version(pkg);

	dict = xbps_find_pkg_installed_from_plist(pkgname);
	if (dict == NULL) {
		free(pkgname);
		return -1; /* not installed */
	}

	/* Get version from installed package */
	prop_dictionary_get_cstring_nocopy(dict, "version", &instver);

	/* Compare installed and required version. */
	rv = xbps_cmpver_versions(instver, reqver);

	free(pkgname);
	prop_object_release(dict);

	return rv;
}

bool
xbps_check_is_installed_pkgname(const char *pkgname)
{
	prop_dictionary_t pkgd;

	assert(pkgname != NULL);

	pkgd = xbps_find_pkg_installed_from_plist(pkgname);
	if (pkgd) {
		prop_object_release(pkgd);
		return true;
	}

	return false;
}

const char *
xbps_get_pkg_version(const char *pkg)
{
	const char *tmp;

	assert(pkg != NULL);

	/* Get the required version */
	tmp = strrchr(pkg, '-');
	if (tmp == NULL)
		return NULL;

	return tmp + 1; /* skip first '-' */
}

char *
xbps_get_pkg_name(const char *pkg)
{
	const char *tmp;
	char *pkgname;
	size_t len = 0;

	assert(pkg != NULL);

	/* Get package name */
	tmp = strrchr(pkg, '-');
	if (tmp == NULL)
		return NULL;

	len = strlen(pkg) - strlen(tmp) + 1;

	pkgname = malloc(len);
	memcpy(pkgname, pkg, len - 1);
	pkgname[len - 1] = '\0';

	return pkgname;
}

char *
xbps_get_pkg_index_plist(const char *path)
{
	struct utsname un;
	char *plist, *p;

	assert(path != NULL);

	if (uname(&un) == -1)
		return NULL;

	p = xbps_append_full_path(false, path, un.machine);
	if (p == NULL)
		return NULL;

	plist = xbps_append_full_path(false, p, XBPS_PKGINDEX);
	if (plist == NULL) {
		free(p);
		return NULL;
	}
	free(p);

	return plist;
}

bool
xbps_pkg_has_rundeps(prop_dictionary_t pkg)
{
	prop_array_t array;

	assert(pkg != NULL);
	array = prop_dictionary_get(pkg, "run_depends");
	if (array && prop_array_count(array) > 0)
		return true;

	return false;
}

void
xbps_set_rootdir(const char *dir)
{
	assert(dir != NULL);
	rootdir = dir;
}

const char *
xbps_get_rootdir(void)
{
	return rootdir;
}

void
xbps_set_flags(int lflags)
{
	flags = lflags;
}

int
xbps_get_flags(void)
{
	return flags;
}

char *
xbps_append_full_path(bool use_rootdir, const char *basedir, const char *plist)
{
	const char *env;
	char *buf = NULL;
	size_t len = 0;

	assert(plist != NULL);

	if (basedir)
		env = basedir;
	else
		env = XBPS_META_PATH;

	if (rootdir && use_rootdir) {
		len = strlen(rootdir) + strlen(env) + strlen(plist) + 2;
		buf = malloc(len + 1);
		if (buf == NULL) {
			errno = ENOMEM;
			return NULL;
		}

		if (snprintf(buf, len + 1, "%s/%s/%s",
		    rootdir, env, plist) < 0) {
			errno = ENOSPC;
			return NULL;
		}
	} else {
		len = strlen(env) + strlen(plist) + 1;
		buf = malloc(len + 1);
		if (buf == NULL) {
			errno = ENOMEM;
			return NULL;
		}

		if (snprintf(buf, len + 1, "%s/%s", env, plist) < 0) {
			errno = ENOSPC;
			return NULL;
		}
	}

	return buf;
}
