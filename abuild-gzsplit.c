#include <stdio.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <zlib.h>

int main(void)
{
	char obuf[8*1024], ibuf[8*1024];
	z_stream zs;
	int r = Z_OK, rc = 1, fd = -1;
	size_t len;
	size_t offset;

	if (inflateInit2(&zs, 15+32) != Z_OK)
		goto err;

	zs.avail_out = sizeof obuf;
	zs.next_out  = (Bytef *)obuf;

	while (1) {
		const char *fn = NULL;

		if (zs.avail_in == 0) {
			zs.avail_in = read(STDIN_FILENO, ibuf, sizeof ibuf);
			zs.next_in = (Bytef *)ibuf;
			if (zs.avail_in < 0) {
				warn("Failed to read input");
				goto err;
			}
			if (zs.avail_in == 0 && r == Z_STREAM_END) goto ok;
		}

		r = inflate(&zs, Z_NO_FLUSH);
		if (r != Z_OK && r != Z_STREAM_END) {
			warnx("Failed to inflate input");
			goto err;
		}

		len = sizeof obuf - zs.avail_out;
		if (len) {
			if (fd < 0) {
				const char *fn;

				if (strncmp(obuf, "PaxHeader/", 10) == 0)
					offset = 10;
				else
					offset = 0;

				if (strncmp(obuf + offset, ".SIGN.", 6) == 0)
					fn = "signatures.tar.gz";
				else if (strncmp(obuf + offset, ".PKGINFO", 8) == 0)
					fn = "control.tar.gz";
				else if (rc == 1)
					fn = "data.tar.gz", rc = 2;
				else {
					warnx("Failed to find .PKGINFO section");
					goto err;
				}
				fd = open(fn, O_CREAT|O_TRUNC|O_WRONLY, 0777);
				if (fd < 0) {
					warn("Failed to open %s", fn);
					goto err;
				}
			}
			zs.next_out = (Bytef *)obuf;
			zs.avail_out = sizeof obuf;
		}

		if (zs.avail_in == 0 || r == Z_STREAM_END) {
			len = (void *)zs.next_in - (void *)ibuf;
			if (write(fd, ibuf, len) != len) {
				warn("Failed to write to %s", fn);
				goto err;
			}
			memmove(ibuf, zs.next_in, zs.avail_in);
			zs.next_in = (Bytef *)ibuf;
		}

		if (r == Z_STREAM_END) {
			if (fd >= 0) {
				close(fd);
				fd = -1;
			}
			inflateEnd(&zs);
			if (inflateInit2(&zs, 15+32) != Z_OK) {
				warnx("inflateInit2 failed");
				goto err;
			}
		}
	}
ok:
	rc = 0;
err:
	if (fd >= 0) close(fd);
	inflateEnd(&zs);
	return rc;
}

