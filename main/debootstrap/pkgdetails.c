#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1000
#define MAX_PKGS 100

static char *fieldcpy(char *dst, char *fld) {
    while (*fld && *fld != ':') 
        fld++;
    if (!*(fld++)) 
        return NULL;
    while (isspace(*fld)) fld++;
    return strcpy(dst, fld);
}

static void dogetdeps(char *pkgsfile, char **in_pkgs, int pkgc) {
    char buf[MAX_LINE];
    char cur_pkg[MAX_LINE];
    char cur_deps[MAX_LINE];
    char *pkgs[MAX_PKGS];
    int i;
    int skip;
    FILE *f;

    cur_pkg[0] = cur_deps[0] = '\0';

    for (i = 0; i < pkgc; i++) pkgs[i] = in_pkgs[i];

    f = fopen(pkgsfile, "r");
    if (f == NULL) {
        perror(pkgsfile);
        exit(1);
    }

    skip = 1;
    while (fgets(buf, sizeof(buf), f)) {
        if (*buf && buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';
        if (strncasecmp(buf, "Package:", 8) == 0) {
            skip = 1;
            fieldcpy(cur_pkg, buf);
	    int any = 0;
	    for (i = 0; i < pkgc; i++) {
		if (!pkgs[i]) continue;
		any = 1;
                if (strcmp(cur_pkg, pkgs[i]) == 0) {
                    skip = 0;
                    pkgs[i] = NULL;
                    break;
                }
            }
            if (!any) break;
        } else if (!skip && 
            (strncasecmp(buf, "Depends:", 8) == 0 || 
             strncasecmp(buf, "Pre-Depends:", 12) == 0)) 
        {
            char *pch;
            fieldcpy(cur_deps, buf);
            pch = cur_deps;
            while (1) {
                while (isspace(*pch)) pch++;
                if (!*pch) break;

                while (*pch && *pch != '(' && *pch != '|' && *pch != ','
                       && !isspace(*pch))
                {
                    fputc(*pch++, stdout);
                }
                fputc('\n', stdout);
                while (*pch && *pch++ != ',') (void)NULL;
            }
        }
    }
    fclose(f);
}

static void dopkgmirrorpkgs(int uniq, char *mirror, char *pkgsfile, 
        char *fieldname, char **in_pkgs, int pkgc) 
{
    char buf[MAX_LINE];
    char cur_field[MAX_LINE];
    char cur_pkg[MAX_LINE];
    char cur_ver[MAX_LINE];
    char cur_arch[MAX_LINE];
    char cur_size[MAX_LINE];
    char cur_md5[MAX_LINE];
    char cur_filename[MAX_LINE];
    char *pkgs[MAX_PKGS];
    int i;
    FILE *f;

    cur_pkg[0] = cur_ver[0] = cur_arch[0] = cur_filename[0] = '\0';

    for (i = 0; i < pkgc; i++) pkgs[i] = in_pkgs[i];

    f = fopen(pkgsfile, "r");
    if (f == NULL) {
        perror(pkgsfile);
        exit(1);
    }
    while (fgets(buf, sizeof(buf), f)) {
        if (*buf && buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';
        if (strncasecmp(buf, fieldname, strlen(fieldname)) == 0) {
            fieldcpy(cur_field, buf);
	}
        if (strncasecmp(buf, "Package:", 8) == 0) {
            fieldcpy(cur_pkg, buf);
        } else if (strncasecmp(buf, "Version:", 8) == 0) {
            fieldcpy(cur_ver, buf);
        } else if (strncasecmp(buf, "Architecture:", 13) == 0) {
            fieldcpy(cur_arch, buf);
        } else if (strncasecmp(buf, "Size:", 5) == 0) {
            fieldcpy(cur_size, buf);
        } else if (strncasecmp(buf, "MD5sum:", 7) == 0) {
            fieldcpy(cur_md5, buf);
        } else if (strncasecmp(buf, "Filename:", 9) == 0) {
            fieldcpy(cur_filename, buf);
        } else if (!*buf) {
	    int any = 0;
	    for (i = 0; i < pkgc; i++) {
		if (!pkgs[i]) continue;
		any = 1;
                if (strcmp(cur_field, pkgs[i]) == 0) {
                    printf("%s %s %s %s %s %s %s\n", cur_pkg, cur_ver, cur_arch, mirror, cur_filename, cur_md5, cur_size);
                    if (uniq) pkgs[i] = NULL;
		    break;
		}
            }
	    if (!any) break;
        }
    }
    fclose(f);

    /* any that weren't found are returned as "pkg -" */
    if (uniq) {
        for (i = 0; i < pkgc; i++) {
            if (pkgs[i]) {
                printf("%s -\n", pkgs[i]);
            }
        }
    }
}

static int dotranslatewgetpercent(int low, int high, int end, char *str) {
    int ch;
    int val, lastval;

    /* print out anything that looks like a % on its own line, appropriately
     * scaled */

    lastval = val = 0;
    while ( (ch = getchar()) != EOF ) {
        if (isdigit(ch)) {
	    val *= 10; val += ch - '0';
	} else if (ch == '%') {
	    float f = (float) val / 100.0 * (high - low) + low;
	    if (str) {
	    	printf("P: %d %d %s\n", (int) f, end, str);
	    } else {
	    	printf("P: %d %d\n", (int) f, end);
	    }
	    lastval = val;
	} else {
	    val = 0;
	}
    }
    return lastval == 100;
}

int main(int argc, char *argv[]) {
    if ((argc == 6 || argc == 5) && strcmp(argv[1], "WGET%") == 0) {
	if (dotranslatewgetpercent(atoi(argv[2]), atoi(argv[3]), 
	                           atoi(argv[4]), argc == 6 ? argv[5] : NULL))
	{
	    exit(0);
	} else {
	    exit(1);
	}
    } else if (argc >= 4 && strcmp(argv[1], "GETDEPS") == 0) {
        int i;
        for (i = 3; argc - i > MAX_PKGS; i += MAX_PKGS) {
	    dogetdeps(argv[2], argv+i, MAX_PKGS);
	}
	dogetdeps(argv[2], argv+i, argc-i);
	exit(0);
    } else if (argc >= 5 && strcmp(argv[1], "PKGS") == 0) {
        int i;
        for (i = 4; argc - i > MAX_PKGS; i += MAX_PKGS) {
	    dopkgmirrorpkgs(1, argv[2], argv[3], "Package:", argv+i, MAX_PKGS);
	}
	dopkgmirrorpkgs(1, argv[2], argv[3], "Package:", argv+i, argc-i);
	exit(0);
    } else if (argc >= 6 && strcmp(argv[1], "FIELD") == 0) {
        int i;
        for (i = 5; argc - i > MAX_PKGS; i += MAX_PKGS) {
	    dopkgmirrorpkgs(0, argv[3], argv[4], argv[2], argv+i, MAX_PKGS);
	}
	dopkgmirrorpkgs(0, argv[3], argv[4], argv[2], argv+i, argc-i);
	exit(0);
    } else {
        fprintf(stderr, "usage: %s PKGS mirror packagesfile pkgs..\n", argv[0]);
        fprintf(stderr, "   or: %s FIELD field mirror packagesfile pkgs..\n", 
                argv[0]);
        fprintf(stderr, "   or: %s GETDEPS packagesfile pkgs..\n", argv[0]);
	fprintf(stderr, "   or: %s WGET%% low high end reason\n", argv[0]);
        exit(1);
    }
}
