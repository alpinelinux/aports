https://patchwork.freedesktop.org/patch/112884/

error.h is a gnu extension and not available in other
popular libcs like musl. This patch provides a replacement.

Signed-off-by: Murray Calavera <murray.calavera@gmail.com>
---
 libweston/weston-launch.c | 20 +++++++++++++++++++-
 1 file changed, 19 insertions(+), 1 deletion(-)

diff --git a/libweston/weston-launch.c b/libweston/weston-launch.c
index 140fde1..84f7d60 100644
--- a/libweston/weston-launch.c
+++ b/libweston/weston-launch.c
@@ -33,7 +33,6 @@
 #include <poll.h>
 #include <errno.h>
 
-#include <error.h>
 #include <getopt.h>
 
 #include <sys/types.h>
@@ -112,6 +111,25 @@ struct weston_launch {
 
 union cmsg_data { unsigned char b[4]; int fd; };
 
+static void
+error(int status, int errnum, const char *msg, ...)
+{
+	va_list args;
+
+	fputs("weston-launch: ", stderr);
+	va_start(args, msg);
+	vfprintf(stderr, msg, args);
+	va_end(args);
+
+	if (errnum)
+		fprintf(stderr, ": %s\n", strerror(errnum));
+	else
+		fputc('\n', stderr);
+
+	if (status)
+		exit(status);
+}
+
 static gid_t *
 read_groups(void)
 {
