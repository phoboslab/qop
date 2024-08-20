#include <assert.h>
#include <stdlib.h>


#define QOP_IMPLEMENTATION
#include "qop.h"

int main(int argc, char *argv[]) {
	assert(argc > 0);

	// Open the archive from argv[0] - the executable
	qop_desc qop;
	int archive_size = qop_open(argv[0], &qop);
	assert(archive_size > 0);

	// Read the archive index
	int index_len = qop_read_index(&qop, malloc(qop.hashmap_size));
	assert(index_len > 0);

	// Find a file
	qop_file *file = qop_find(&qop, "qop.h");
	assert(file);

	// Load the file contents
	unsigned char *contents = malloc(file->size);
	qop_read(&qop, file, contents);

	printf("%.*s\n", (int)file->size, contents);
	free(contents);

	free(qop.hashmap);
	qop_close(&qop);
}