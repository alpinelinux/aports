upstream report: https://github.com/Leont/crypt-rijndael/issues/17

diff --git a/rijndael.h b/rijndael.h
index 8601122..83cec2d 100644
--- a/rijndael.h
+++ b/rijndael.h
@@ -34,6 +34,11 @@
 	#undef _CRYPT_RIJNDAEL_H_TYPES
 #endif
 
+#include <stdint.h>
+#define _CRYPT_RIJNDAEL_H_TYPES
+typedef uint32_t UINT32;
+typedef uint8_t  UINT8;
+
 /* Irix. We could include stdint.h and use uint8_t but that also
  * requires that we specifically drive the compiler in C99 mode.
  * Defining UINT8 as unsigned char is, ultimately, what stdint.h
