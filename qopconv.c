/*

Copyright (c) 2024, Dominic Szablewski - https://phoboslab.org
SPDX-License-Identifier: MIT


Command line tool to create qop archives

*/

#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define QOP_IMPLEMENTATION
#include "qop.h"

#define MAX_PATH 1024
#define BUFFER_SIZE 4096

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define die(...) \
	printf("Abort at " TOSTRING(__FILE__) " line " TOSTRING(__LINE__) ": " __VA_ARGS__); \
	printf("\n"); \
	exit(1)

#define error_if(TEST, ...) \
	if (TEST) { \
		die(__VA_ARGS__); \
	}

// -----------------------------------------------------------------------------
// Unpack

int create_path(const char *path, const mode_t mode) {
	char tmp[MAX_PATH];
	char *p = NULL;
	struct stat sb;
	size_t len;

	// copy path
	len = strnlen(path, MAX_PATH);
	if (len == 0 || len == MAX_PATH) {
		return -1;
	}
	memcpy(tmp, path, len);
	tmp[len] = '\0';

	// remove file part
	char *last_slash = strrchr(tmp, '/');
	if (last_slash == NULL) {
		return 0;
	}
	*last_slash = '\0';

	// check if path exists and is a directory
	if (stat(tmp, &sb) == 0) {
		if (S_ISDIR(sb.st_mode)) {
			return 0;
		}
	}

	// recursive mkdir
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			if (stat(tmp, &sb) != 0) {
				if (mkdir(tmp, mode) < 0) {
					return -1;
				}
			}
			else if (!S_ISDIR(sb.st_mode)) {
				return -1;
			}
			*p = '/';
		}
	}
	if (stat(tmp, &sb) != 0) {
		if (mkdir(tmp, mode) < 0) {
			return -1;
		}
	}
	else if (!S_ISDIR(sb.st_mode)) {
		return -1;
	}
	return 0;
}

unsigned int copy_out(FILE *src, unsigned int offset, unsigned int size, const char *dest_path) {
	FILE *dest = fopen(dest_path, "w");
	error_if(!dest, "Could not open file %s for writing", dest_path);

	char buffer[BUFFER_SIZE];
	size_t bytes_read, bytes_written;
	unsigned int bytes_total = 0;
	unsigned int read_size = size < BUFFER_SIZE ? size : BUFFER_SIZE;

	fseek(src, offset, SEEK_SET);
	while (read_size > 0 && (bytes_read = fread(buffer, 1, read_size, src)) > 0) {
		bytes_written = fwrite(buffer, 1, bytes_read, dest);
		error_if(bytes_written != bytes_read, "Write error");
		bytes_total += bytes_written;
		if (bytes_total >= size) {
			break;
		}
		if (size - bytes_total < read_size) {
			read_size = size - bytes_total;
		}
	}

	error_if(ferror(src), "read error for file %s", dest_path);
	fclose(dest);
	return bytes_total;
}

void unpack(const char *archive_path, int list_only) {
	qop_desc qop;
	int archive_size = qop_open(archive_path, &qop);
	error_if(archive_size == 0, "Could not open archive %s", archive_path);

	// Read the archive index
	void *index_buffer = malloc(qop.index_size);
	int index_len = qop_read_index(&qop, index_buffer);
	error_if(index_len == 0, "Could not read index from archive %s", archive_path);
	
	// Extract all files
	for (int i = 0; i < index_len; i++) {
		qop_file *file = &qop.index[i];
		if (file->size == 0) {
			continue;
		}
		error_if(file->path_len >= MAX_PATH, "Path for file %016llx exceeds %d", file->hash, MAX_PATH);
		char path[MAX_PATH];
		qop_read_path(&qop, file, path);

		qop_file *tf = qop_find(&qop, path);
		error_if(!tf, "could not find %s", path);

		printf("%6d %016llx %10d %s\n", i, file->hash, file->size, path);

		if (!list_only) {
			error_if(create_path(path, 0755) != 0, "Could not create path %s", path);
			copy_out(qop.fh, qop.files_offset + file->offset + file->path_len, file->size, path);
		}
	}

	qop_close(&qop);
	free(index_buffer);
}


// -----------------------------------------------------------------------------
// Pack

typedef struct {
	qop_file *files;
	int len;
	int capacity;
	int size;
} pack_state;

void write_16(unsigned int v, FILE *fh) {
	unsigned char b[sizeof(unsigned short)];
	b[0] = 0xff & (v      );
	b[1] = 0xff & (v >>  8);
	int written = fwrite(b, sizeof(unsigned short), 1, fh);
	error_if(!written, "Write error");
}

void write_32(unsigned int v, FILE *fh) {
	unsigned char b[sizeof(unsigned int)];
	b[0] = 0xff & (v      );
	b[1] = 0xff & (v >>  8);
	b[2] = 0xff & (v >> 16);
	b[3] = 0xff & (v >> 24);
	int written = fwrite(b, sizeof(unsigned int), 1, fh);
	error_if(!written, "Write error");
}

void write_64(qop_uint64_t v, FILE *fh) {
	unsigned char b[sizeof(qop_uint64_t)];
	b[0] = 0xff & (v      );
	b[1] = 0xff & (v >>  8);
	b[2] = 0xff & (v >> 16);
	b[3] = 0xff & (v >> 24);
	b[4] = 0xff & (v >> 32);
	b[5] = 0xff & (v >> 40);
	b[6] = 0xff & (v >> 48);
	b[7] = 0xff & (v >> 56);
	int written = fwrite(b, sizeof(qop_uint64_t), 1, fh);
	error_if(!written, "Write error");
}

unsigned int copy_into(const char *src_path, FILE *dest) {
	FILE *src = fopen(src_path, "r");
	error_if(!src, "Could not open file %s for reading", src_path);

	char buffer[BUFFER_SIZE];
	size_t bytes_read, bytes_written;
	unsigned int bytes_total = 0;

	while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src)) > 0) {
		bytes_written = fwrite(buffer, 1, bytes_read, dest);
		error_if(bytes_written != bytes_read, "Write error");
		bytes_total += bytes_written;
	}

	error_if(ferror(src), "read error for file %s", src_path);
	fclose(src);
	return bytes_total;
}

void add_file(const char *path, FILE *dest, pack_state *state) {
	if (state->len >= state->capacity) {
		state->capacity *= 2;
		state->files = realloc(state->files, state->capacity * sizeof(qop_file));
	}

	qop_uint64_t hash = qop_hash(path);
	

	// Write the path into the archive
	int path_len = strlen(path) + 1;
	int path_written = fwrite(path, sizeof(char), path_len, dest);
	error_if(path_written != path_len, "Write error");

	// Copy the file into the archive
	unsigned int size = copy_into(path, dest);

	printf("%6d %016llx %10d %s\n", state->len, hash, size, path);

	// Collect file info for the index
	state->files[state->len] = (qop_file){
		.hash = hash,
		.offset = state->size,
		.size = size,
		.path_len = path_len,
		.flags = QOP_FLAG_NONE
	};
	state->size += size + path_len;
	state->len++;
}

void add_dir(const char *path, FILE *dest, pack_state *state) {
	DIR *dir = opendir(path);
	error_if(!dir, "Could not open directory %s for reading", path);

	struct dirent *file;
	for (int i = 0; (file = readdir(dir)) != NULL; i++) {
		if (
			file->d_type & DT_DIR &&
			strcmp(file->d_name, ".") != 0 &&
			strcmp(file->d_name, "..") != 0
		) {
			char subpath[MAX_PATH];
			snprintf(subpath, MAX_PATH, "%s/%s", path, file->d_name);
			add_dir(subpath, dest, state);
		}
		else if (file->d_type == DT_REG) {
			char subpath[MAX_PATH];
			snprintf(subpath, MAX_PATH, "%s/%s", path, file->d_name);
			add_file(subpath, dest, state);
		}
	}
}

void pack(char **sources, int sources_len, const char *archive_path) {
	FILE *dest = fopen(archive_path, "w+");
	error_if(!dest, "Could not open file %s for writing", archive_path);

	pack_state state = {
		.files = malloc(sizeof(qop_file) * 1024),
		.len = 0,
		.capacity = 1024,
		.size = 0
	};

	// Add files/directories
	for (int i = 0; i < sources_len; i++) {
		struct stat s;
		error_if(stat(sources[i], &s) != 0, "Could not stat file %s", sources[i]);

		if (S_ISDIR(s.st_mode)) {
			add_dir(sources[i], dest, &state);
		}
		else if (S_ISREG(s.st_mode)) {
			add_file(sources[i], dest, &state);
		}
		else {
			die("Path %s is neither a directory nor a regular file", sources[i]);
		}
	}

	// Create Index
	int index_len = 0;
	int index_bits = 1;
	for (; index_bits < 24; index_bits++) {
		index_len = 1 << index_bits;
		if (index_len > state.len * 1.5) {
			break;
		}
	}

	qop_file *index = malloc(sizeof(qop_file) * index_len);
	memset(index, 0, sizeof(qop_file) * index_len);

	int mask = index_len - 1;
	for (int i = 0; i < state.len; i++) {
		int idx = state.files[i].hash & mask;
		while (index[idx & mask].size > 0) {
			idx++;
		}
		index[idx] = state.files[i];
	}

	// Write index and header
	unsigned int total_size = state.size + QOP_HEADER_SIZE;
	for (int i = 0; i < index_len; i++) {
		write_64(index[i].hash, dest);
		write_32(index[i].offset, dest);
		write_32(index[i].size, dest);
		write_16(index[i].path_len, dest);
		write_16(index[i].flags, dest);
		total_size += 20;
	}

	write_32(total_size, dest);
	write_32(index_bits, dest);
	write_32(QOP_MAGIC, dest);

	free(state.files);
	free(index);
	fclose(dest);

	printf("files: %d, index len: %d, size: %d bytes\n", state.len, index_len, total_size);
}



int main(int argc, char **argv) {
	if (argc < 3) {
		puts("Usage: qopconv [-ul] <infiles/dirs> <outfile.qop>");
		puts("Examples:");
		puts("  qopconv dir1 out.qop");
		puts("  qopconv file1 file2 dir1 out.qop");
		puts("Unpack archive:");
		puts("  qopconv -u archive.qop");
		puts("List archive contents:");
		puts("  qopconv -l archive.qop");
		exit(1);
	}

	// Unpack
	if (strcmp(argv[1], "-u") == 0) {
		unpack(argv[2], 0);	
	}
	else if (strcmp(argv[1], "-l") == 0) {
		unpack(argv[2], 1);
	}
	else {
		pack(argv + 1, argc - 2, argv[argc-1]);
	}
	return 0;
}
