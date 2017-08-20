#include "common.h"
#include "id.h"
#include "fs.h"

int direxists(const char *dir)
{
	return fileexists(dir);
}

int fileexists(const char *file)
{
	struct stat statbuf;

	if (stat(file, &statbuf)!=0) {
		if (errno==ENOENT) {
			return 0;
		}
		if (debug)
			printf("Error: stat() \"%s\": %s\n", file, strerror(errno));
	}
	return 1;
}

int mkpath(const char *dir, const mode_t mode)
{
	int i = 0, len = 0, ret = 1;
	char *tmp = NULL;

	if (!strlen(dir)) {
		if (debug)
			printf("Error: mkpath(), no directory given\n");
		return 0;
	}

	if (direxists(dir)) {
		if (debug)
			printf("already exists: %s\n", dir);
		return 1;
	}

	if (!cfg.createdirs) {
		return 0;
	}

	tmp = strdup(dir);
	if (tmp == NULL) {
		return 0;
	}

	len = strlen(tmp);
	if (tmp[len-1] == '/') {
		tmp[len-1] = '\0';
	}

	if (tmp[0] == '/') {
		i++;
	}

	for (; i<len; i++) {
		if (tmp[i] == '/') {
			tmp[i] = '\0';
			if (!direxists(tmp)) {
				if (mkdir(tmp, mode)!=0) {
					if (debug)
						printf("Error: mkdir() \"%s\": %s\n", tmp, strerror(errno));
					ret = 0;
					break;
				}
			}
			tmp[i] = '/';
		}
	}
	if (ret) {
		if (mkdir(tmp, mode)!=0) {
			if (debug)
				printf("Error: mkdir() \"%s\": %s\n", tmp, strerror(errno));
			ret = 0;
		} else if (debug) {
			printf("created: %s\n", tmp);
		}
	}

	free(tmp);
	return ret;
}

void preparevnstatdir(const char *file, const char *user, const char *group)
{
	int len, i, lastslash=0;
	char *path, *base;

	if (file == NULL) {
		return;
	}

	len = strlen(file);
	if (len<2) {
		return;
	}

	if (file[len-1] == '/') {
		return;
	}

	path = strdup(file);
	if (path == NULL) {
		return;
	}

	/* verify that path ends with vnstat or vnstatd */
	base = basename(dirname(path));
	if (strcmp(base, "vnstat")!=0 && strcmp(base, "vnstatd")!=0) {
		free(path);
		return;
	}
	free(path);

	path = strdup(file);
	if (path == NULL) {
		return;
	}

	/* extract path */
	for (i=0; i<len; i++) {
		if (path[i] == '/') {
			lastslash = i;
		}
	}
	if (lastslash == 0) {
		free(path);
		return;
	}
	path[lastslash] = '\0';

	/* create & chmod if needed */
	if (mkpath(path, 0775)) {
		updatedirowner(path, user, group);
	}
	free(path);
}

void updatedirowner(const char *dir, const char *user, const char *group)
{
	uid_t uid;
	gid_t gid;

	if (!cfg.updatefileowner) {
		return;
	}

	if (!hasroot()) {
		if (debug)
			printf("user not root, skipping chmod\n");
		return;
	}

	uid = getuser(user);
	gid = getgroup(group);

	updatedirownerid(dir, uid, gid);
}

void updatedirownerid(const char *dir, const uid_t uid, const gid_t gid)
{
	DIR *d;
	struct dirent *di;
	struct stat statbuf;
	char entryname[512];

	if (!cfg.updatefileowner) {
		return;
	}

	if (!hasroot()) {
		if (debug)
			printf("user not root, skipping chmod\n");
		return;
	}

	if (stat(dir, &statbuf)!=0) {
		return;
	}

	if (statbuf.st_uid != uid || statbuf.st_gid != gid) {
		if (chown(dir, uid, gid) != 0) {
			if (debug)
				printf("Error: updatedirowner() chown() \"%s\": %s\n", dir, strerror(errno));
			return;
		} else {
			if (debug)
				printf("\"%s\" chown completed\n", dir);
		}
	}

	if ((d=opendir(dir))==NULL) {
		if (debug)
			printf("Error: updatedirowner() diropen() \"%s\": %s\n", dir, strerror(errno));
		return;
	}

	while ((di=readdir(d))) {
		if (di->d_type != DT_REG) {
			continue;
		}
		snprintf(entryname, 512, "%s/%s", dir, di->d_name);
		if (stat(entryname, &statbuf)!=0) {
			continue;
		}
		if (statbuf.st_uid != uid || statbuf.st_gid != gid) {
			if (chown(entryname, uid, gid) != 0) {
				if (debug)
					printf("Error: chown() \"%s\": %s\n", entryname, strerror(errno));
			} else {
				if (debug)
					printf("\"%s\" chown completed\n", entryname);
			}
		}
	}

	closedir(d);
}

void matchdbownerwithdirowner(const char *dir)
{
	uid_t uid;
	gid_t gid;

	if (!cfg.updatefileowner) {
		if (debug)
			printf("db owner: UpdateFileOwner disabled\n");
		return;
	}

	if (!direxists(dir)) {
		return;
	}

	if (!hasroot()) {
		if (debug)
			printf("db owner: user not root, skipping chmod\n");
		return;
	}

	if (!getdirowner(dir, &uid, &gid)) {
		return;
	}

	if (debug)
		printf("db owner: \"%s\" user/group: %d/%d\n", dir, (int)uid, (int)gid);

	updatedirownerid(dir, uid, gid);
}

int getdirowner(const char *dir, uid_t *uid, gid_t *gid)
{
	struct stat statbuf;

	if (!direxists(dir)) {
		return 0;
	}

	if (stat(dir, &statbuf)!=0) {
		if (debug)
			printf("Error: stat() \"%s\": %s\n", dir, strerror(errno));
		return 0;
	}

	*uid = statbuf.st_uid;
	*gid = statbuf.st_gid;

	return 1;
}
