#ifndef lint
static const char *rcsid = "$Id: file.c,v 1.16 1995/08/01 09:49:27 jkh Exp $";
#endif

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Miscellaneous file access utilities.
 *
 */

#include "lib.h"
#include "ftp.h"
#include <pwd.h>
#include <time.h>

/* Quick check to see if a file exists */
Boolean
fexists(char *fname)
{
    struct stat dummy;
    if (!lstat(fname, &dummy))
	return TRUE;
    return FALSE;
}

/* Quick check to see if something is a directory */
Boolean
isdir(char *fname)
{
    struct stat sb;

    if (stat(fname, &sb) != FAIL && S_ISDIR(sb.st_mode))
	return TRUE;
    else
	return FALSE;
}

/* Check to see if file is a dir, and is empty */
Boolean
isemptydir(char *fname)
{
    if (isdir(fname)) {
	DIR *dirp;
	struct dirent *dp;

	dirp = opendir(fname);
	if (!dirp)
	    return FALSE;	/* no perms, leave it alone */
	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	    if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
		closedir(dirp);
		return FALSE;
	    }
	}
	(void)closedir(dirp);
	return TRUE;
    }
    return FALSE;
}

Boolean
isfile(char *fname)
{
    struct stat sb;
    if (stat(fname, &sb) != FAIL && S_ISREG(sb.st_mode))
	return TRUE;
    return FALSE;
}

/* Check to see if file is a file and is empty. If nonexistent or not
   a file, say "it's empty", otherwise return TRUE if zero sized. */
Boolean
isemptyfile(char *fname)
{
    struct stat sb;
    if (stat(fname, &sb) != FAIL && S_ISREG(sb.st_mode)) {
	if (sb.st_size != 0)
	    return FALSE;
    }
    return TRUE;
}

/* Returns TRUE if file is a URL specification */
Boolean
isURL(char *fname)
{
    /*
     * I'm sure there are other types of URL specifications that I could
     * also be looking for here, but for now I'll just be happy to get ftp
     * working.
     */
    while (isspace(*fname))
	++fname;
    if (!strncmp(fname, "ftp://", 6))
	return TRUE;
    return FALSE;
}

/* Returns the host part of a URL */
char *
fileURLHost(char *fname, char *where, int max)
{
    char *ret;

    while (isspace(*fname))
	++fname;
    /* Don't ever call this on a bad URL! */
    fname += strlen("ftp://");
    /* Do we have a place to stick our work? */
    if (ret = where) {
	while (*fname && *fname != '/' && max--)
	    *where++ = *fname++;
	*where = '\0';
	return ret;
    }
    /* If not, they must really want us to stomp the original string */
    ret = fname;
    while (*fname && *fname != '/')
	++fname;
    *fname = '\0';
    return ret;
}

/* Returns the filename part of a URL */
char *
fileURLFilename(char *fname, char *where, int max)
{
    char *ret;

    while (isspace(*fname))
	++fname;
    /* Don't ever call this on a bad URL! */
    fname += strlen("ftp://");
    /* Do we have a place to stick our work? */
    if (ret = where) {
	while (*fname && *fname != '/')
	    ++fname;
	if (*fname == '/') {
	    while (*fname && max--)
		*where++ = *fname++;
	}
	*where = '\0';
	return ret;
    }
    /* If not, they must really want us to stomp the original string */
    while (*fname && *fname != '/')
	++fname;
    return fname;
}

#define HOSTNAME_MAX	64
/*
 * Try and fetch a file by URL, returning the fd of open
 * file if fetched successfully.
 */
char *
fileGetURL(char *fname)
{
    char host[HOSTNAME_MAX], file[FILENAME_MAX], dir[FILENAME_MAX];
    char pword[HOSTNAME_MAX + 40], *uname, *cp;
    static char tmpl[40];
    struct passwd *pw;
    FTP_t ftp;
    int fd, fd2, i, len = 0;
    char ch;
    time_t start, stop;

    if (!isURL(fname))
	return NULL;

    ftp = FtpInit();
    cp = fileURLHost(fname, host, HOSTNAME_MAX);
    if (!*cp) {
	whinge("URL `%s' has bad host part!", fname);
	return NULL;
    }

    cp = fileURLFilename(fname, file, FILENAME_MAX);
    if (!*cp) {
	whinge("URL `%s' has bad filename part!", fname);
	return NULL;
    }

    /* Maybe change to ftp if this doesn't work */
    uname = "anonymous";

    /* Make up a convincing "password" */
    pw = getpwuid(getuid());
    if (!pw) {
	whinge("Can't get user name for ID %d\n.", getuid());
	strcpy(pword, "joe@");
    }
    else
	snprintf(pword, HOSTNAME_MAX + 40, "%s@%s", pw->pw_name, host);
    if (Verbose)
	printf("Trying to fetch %s from %s.\n", file, host);

    FtpOpen(ftp, host, uname, pword);
    if (getenv("FTP_PASSIVE_MODE"))
	FtpPassive(ftp, TRUE);

    strcpy(dir, file);
    for (i = strlen(dir); i && dir[i] != '/'; i--);
    dir[i] = '\0';

    if (dir[0]) {
	if (Verbose) printf("FTP: chdir to %s\n", dir);
	FtpChdir(ftp, dir);
    }
    FtpBinary(ftp, TRUE);
    if (Verbose) printf("FTP: trying to get %s\n", basename_of(file));
    fd = FtpGet(ftp, basename_of(file));
    if (fd < 0)
	return NULL;
    if ((cp = getenv("PKG_TMPDIR")) != NULL)
	sprintf(tmpl, "%s/instpkg-XXXXXX.tgz", cp);
    else
	strcpy(tmpl, "/var/tmp/instpkg-XXXXXX.tgz");
    fd2 = mkstemp(tmpl);
    if (fd2 < 0) {
	whinge("Unable to create a temporary file for ftp: %s", tmpl);
	return NULL;
    }
    if (Verbose) printf("FTP: Trying to copy from ftp connection to temporary: %s\n", tmpl);
    (void)time(&start);
    while (read(fd, &ch, 1) == 1) {
	++len;
	write(fd2, &ch, 1);
    }
    (void)time(&stop);
    if (Verbose) printf("FTP: Read %d bytes from connection, %d elapsed seconds.\n%FTP: Average transfer rate: %d bytes/second.\n", len, stop - start, len / ((stop - start) + 1));
    FtpEOF(ftp);
    FtpClose(ftp);
    return tmpl;
}

char *
fileFindByPath(char *fname)
{
    static char tmp[FILENAME_MAX];
    char *cp;

    if (fexists(fname) && isfile(fname)) {
	strcpy(tmp, fname);
	return tmp;
    }
    cp = getenv("PKG_PATH");
    while (cp) {
	char *cp2 = strsep(&cp, ":");

	snprintf(tmp, FILENAME_MAX, "%s/%s.tgz", cp2 ? cp2 : cp, fname);
	if (fexists(tmp) && isfile(fname))
	    return tmp;
    }
    return NULL;
}

char *
fileGetContents(char *fname)
{
    char *contents;
    struct stat sb;
    int fd;

    if (stat(fname, &sb) == FAIL)
	barf("Can't stat '%s'.", fname);

    contents = (char *)malloc(sb.st_size + 1);
    fd = open(fname, O_RDONLY, 0);
    if (fd == FAIL)
	barf("Unable to open '%s' for reading.", fname);
    if (read(fd, contents, sb.st_size) != sb.st_size)
	barf("Short read on '%s' - did not get %qd bytes.", fname, sb.st_size);
    close(fd);
    contents[sb.st_size] = '\0';
    return contents;
}

/* Write the contents of "str" to a file */
void
write_file(char *name, char *str)
{
    FILE *fp;
    int len;

    fp = fopen(name, "w");
    if (!fp)
	barf("Can't fopen '%s' for writing.", name);
    len = strlen(str);
    if (fwrite(str, 1, len, fp) != len)
	barf("Short fwrite on '%s', tried to write %d bytes.", name, len);
    if (fclose(fp))
	barf("failure to fclose '%s'.", name);
}

void
copy_file(char *dir, char *fname, char *to)
{
    char cmd[FILENAME_MAX];

    if (fname[0] == '/')
	snprintf(cmd, FILENAME_MAX, "cp -p -r %s %s", fname, to);
    else
	snprintf(cmd, FILENAME_MAX, "cp -p -r %s/%s %s", dir, fname, to);
    if (vsystem(cmd))
	barf("Couldn't perform '%s'", cmd);
}

void
move_file(char *dir, char *fname, char *to)
{
    char cmd[FILENAME_MAX];

    if (fname[0] == '/')
	snprintf(cmd, FILENAME_MAX, "mv %s %s", fname, to);
    else
	snprintf(cmd, FILENAME_MAX, "mv %s/%s %s", dir, fname, to);
    if (vsystem(cmd))
	barf("Couldn't perform '%s'", cmd);
}

/*
 * Copy a hierarchy (possibly from dir) to the current directory, or
 * if "to" is TRUE, from the current directory to a location someplace
 * else.
 *
 * Though slower, using tar to copy preserves symlinks and everything
 * without me having to write some big hairy routine to do it.
 */
void
copy_hierarchy(char *dir, char *fname, Boolean to)
{
    char cmd[FILENAME_MAX * 3];

    if (!to) {
	/* If absolute path, use it */
	if (*fname == '/')
	    dir = "/";
	snprintf(cmd, FILENAME_MAX * 3, "tar cf - -C %s %s | tar xpf -",
 		 dir, fname);
    }
    else
	snprintf(cmd, FILENAME_MAX * 3, "tar cf - %s | tar xpf - -C %s",
 		 fname, dir);
#ifdef DEBUG
    printf("Using '%s' to copy trees.\n", cmd);
#endif
    if (system(cmd))
	barf("copy_file: Couldn't perform '%s'", cmd);
}

/* Unpack a tar file */
int
unpack(char *pkg, char *flist)
{
    char args[10], suffix[80], *cp;

    /*
     * Figure out by a crude heuristic whether this or not this is probably
     * compressed.
     */
    cp = rindex(pkg, '.');
    if (cp) {
	strcpy(suffix, cp + 1);
	if (index(suffix, 'z') || index(suffix, 'Z'))
	    strcpy(args, "-z");
    }
    strcat(args, "xpf");
    if (vsystem("tar %s %s %s", args, pkg, flist ? flist : "")) {
	whinge("Tar extract of %s failed!", pkg);
	return 1;
    }
    return 0;
}

/* Using fmt, replace all instances of:
 *
 * %F	With the parameter "name"
 * %D	With the parameter "dir"
 * %B	Return the directory part ("base") of %D/%F
 * %f	Return the filename part of %D/%F
 *
 * Does not check for overflow - caution!
 *
 */
void
format_cmd(char *buf, char *fmt, char *dir, char *name)
{
    char *cp, scratch[FILENAME_MAX * 2];

    while (*fmt) {
	if (*fmt == '%') {
	    switch (*++fmt) {
	    case 'F':
		strcpy(buf, name);
		buf += strlen(name);
		break;

	    case 'D':
		strcpy(buf, dir);
		buf += strlen(dir);
		break;

	    case 'B':
		sprintf(scratch, "%s/%s", dir, name);
		cp = &scratch[strlen(scratch) - 1];
		while (cp != scratch && *cp != '/')
		    --cp;
		*cp = '\0';
		strcpy(buf, scratch);
		buf += strlen(scratch);
		break;

	    case 'f':
		sprintf(scratch, "%s/%s", dir, name);
		cp = &scratch[strlen(scratch) - 1];
		while (cp != scratch && *(cp - 1) != '/')
		    --cp;
		strcpy(buf, cp);
		buf += strlen(cp);
		break;

	    default:
		*buf++ = *fmt;
		break;
	    }
	    ++fmt;
	}
	else
	    *buf++ = *fmt++;
    }
    *buf = '\0';
}
