#include <stdlib.h>
#include <assert.h>
#include <tls.h>

int
main(int argc, const char *argv[])
{
	assert(tls_init() == 0);

	return EXIT_SUCCESS;
}
