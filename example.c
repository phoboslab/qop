#include <assert.h>
#include <stdlib.h>


#define QOP_IMPLEMENTATION
#include "qop.h"

#if defined(__linux__)
	#include <unistd.h>
	#include <stdio.h>
#elif defined(__APPLE__)
	#include <mach-o/dyld.h>
#elif defined(_WIN32)
	#include <windows.h>
#endif

int get_executable_path(char *buffer, int buffer_size) {
	#if defined(__linux__)
		ssize_t len = readlink("/proc/self/exe", buffer, buffer_size - 1);
		if (len == -1) {
			return 0;
		}
		buffer[len] = '\0';
		return len;
	#elif defined(__APPLE__)
		uint32_t size = sizeof(path);
		if (_NSGetExecutablePath(buffer, &buffer_size) == 0) {
			return buffer_size;
		}
	#elif defined(_WIN32)
		return GetModuleFileName(NULL, buffer, buffer_size);
	#endif

	return 0;
}

int main(int, char *[]) {
	// Find the path to the current executable
	char exe_path[1024];
	int exe_path_len = get_executable_path(exe_path, sizeof(exe_path));
	assert(exe_path_len > 0);

	// Open the archive appended to this executable
	qop_desc qop;
	int archive_size = qop_open(exe_path, &qop);
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