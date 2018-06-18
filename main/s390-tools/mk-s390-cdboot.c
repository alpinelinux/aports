/*
 * mk-s390-cdboot -- creates one big image using a kernel, a ramdisk and
 *                   a parmfile
 *
 * 2003-07-24 Volker Sameske <sameske@de.ibm.com>
 * 2008-09-22 Updated by David Cantrell <dcantrell@redhat.com>
 *
 * compile with:
 *     gcc -Wall -o mk-s390-cdboot mk-s390-cdboot.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <libgen.h>

#define BUFFER_LEN 1024
#define INITRD_START 0x0000000000800000LL
#define START_PSW_ADDRESS 0x80010000

static struct option getopt_long_options[]= {
    { "image", 1, 0, 'i'},
    { "ramdisk", 1, 0, 'r'},
    { "parmfile", 1, 0, 'p'},
    { "outfile", 1, 0, 'o'},
    { "help", 0, 0, 'h'},
    {0, 0, 0, 0}
};

static void usage(char *cmd) {
    printf("%s [-h] [-v] -i <kernel> -r <ramdisk> -p <parmfile> -o <outfile>\n", cmd);
}

int main (int argc, char **argv) {
    char *cmd = basename(argv[0]);
    FILE *fd1 = NULL;
    FILE *fd2 = NULL;
    FILE *fd3 = NULL;
    FILE *fd4 = NULL;
    char buffer[BUFFER_LEN];
    int wc, rc, oc, index;
    unsigned long long initrd_start = INITRD_START;
    unsigned long long initrd_size;
    char *image = NULL;
    char *ramdisk = NULL;
    char *parmfile = NULL;
    char *outfile = NULL;
    int image_specified = 0;
    int ramdisk_specified = 0;
    int parmfile_specified = 0;
    int outfile_specified = 0;
    int start_psw_address = START_PSW_ADDRESS;

    opterr = 0;
    while (1) {
        oc = getopt_long(argc, argv, "i:r:p:o:h?", getopt_long_options, &index);
        if (oc == -1) {
            break;
        }

        switch (oc) {
            case '?':
            case 'h':
                usage(cmd);
                exit(0);
            case 'i':
                image = strdup(optarg);
                image_specified = 1;
                break;
            case 'r':
                ramdisk = strdup(optarg);
                ramdisk_specified = 1;
                break;
            case 'p':
                parmfile = strdup(optarg);
                parmfile_specified = 1;
                break;
            case 'o':
                outfile = strdup(optarg);
                outfile_specified = 1;
                break;
            default:
                usage(cmd);
                exit(0);
        }
    }

    if (!image_specified || !ramdisk_specified ||
        !parmfile_specified || !outfile_specified) {
        usage(cmd);
        exit(0);
    }

    printf("Creating bootable CD-ROM image...\n");
    printf("kernel is  : %s\n", image);
    printf("ramdisk is : %s\n", ramdisk);
    printf("parmfile is: %s\n", parmfile);
    printf("outfile is : %s\n", outfile);

    if ((fd1 = fopen(outfile, "w")) == NULL) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    if ((fd2 = fopen(image, "r")) == NULL) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    if ((fd3 = fopen(ramdisk, "r")) == NULL) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    if ((fd4 = fopen(parmfile, "r")) == NULL) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    printf("writing kernel...\n");
    while (1) {
        rc = fread(buffer, 1, 1, fd2);

        if (rc == 0) {
            break;
        }

        if (feof(fd2) || ferror(fd2)) {
            fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
            abort();
        }

        wc = fwrite(buffer, 1, 1, fd1);
        if (feof(fd1) || ferror(fd1)) {
            fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
            abort();
        }

        if (wc != rc) {
          fprintf(stderr, "could only write %i of %i bytes of kernel\n",
                  wc, rc);
        }
    }

    printf("writing initrd...\n");
    fseek(fd1, initrd_start, SEEK_SET);
    while (1) {
        rc = fread(buffer, 1, 1, fd3);

        if (rc == 0) {
            break;
        }

        if (feof(fd3) || ferror(fd3)) {
            fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
            abort();
        }

        wc = fwrite(buffer, 1, 1, fd1);
        if (feof(fd1) || ferror(fd1)) {
            fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
            abort();
        }

        if (wc != rc) {
          fprintf(stderr, "could only write %i of %i bytes of initrd\n",
                  wc, rc);
        }
    }

    if (fseek(fd3, 0, SEEK_END) == -1) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    if ((initrd_size = ftell(fd3)) == -1) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    printf("changing start PSW address to 0x%08x...\n", start_psw_address);
    if (fseek(fd1, 0x4, SEEK_SET) == -1) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    wc = fwrite(&start_psw_address, 1, 4, fd1);
    if (feof(fd1) || ferror(fd1)) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    if (wc != 4) {
        fprintf(stderr, "could only write %i of %i bytes of PSW address\n",
                wc, 4);
    }

    printf("writing initrd address and size...\n");
    printf("INITRD start: 0x%016llx\n",  initrd_start);
    printf("INITRD size : 0x%016llx\n", initrd_size);

    if (fseek(fd1, 0x10408, SEEK_SET) == -1) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    wc = fwrite(&initrd_start, 1, 8, fd1);
    if (feof(fd1) || ferror(fd1)) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    if (wc != 8) {
        fprintf(stderr, "could only write %i of %i bytes of INITRD start\n",
                wc, 8);
    }

    if (fseek(fd1, 0x10410, SEEK_SET) == -1) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    wc = fwrite(&initrd_size, 1, 8, fd1);
    if (feof(fd1) || ferror(fd1)) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    if (wc != 8) {
        fprintf(stderr, "could only write %i of %i bytes of INITRD size\n",
                wc, 8);
    }

    printf("writing parmfile...\n");
    if (fseek(fd1, 0x10480, SEEK_SET) == -1) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
        abort();
    }

    while (1) {
        rc = fread(buffer, 1, 1, fd4);

        if (rc == 0) {
            break;
        }

        if (feof(fd4) || ferror(fd4)) {
            fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
            abort();
        }

        wc = fwrite(buffer, 1, 1, fd1);
        if (feof(fd1) || ferror(fd1)) {
            fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
            abort();
        }

        if (wc != 1) {
            fprintf(stderr, "could only write %i of %i bytes of parmfile\n",
                    wc, 1);
        }
    }

    if (fclose(fd1) == EOF) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
    }

    if (fclose(fd2) == EOF) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
    }

    if (fclose(fd3) == EOF) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
    }

    if (fclose(fd4) == EOF) {
        fprintf(stderr, "%s (%d): %s\n", __func__, __LINE__, strerror(errno));
    }

    return EXIT_SUCCESS;
}
