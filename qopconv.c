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


typedef struct {
	qop_file *files;
	int len;
	int capacity;
	int size;
} qop_state;

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

unsigned int copy_into(const char *path, FILE *dest) {
	FILE *src = fopen(path, "r");
	error_if(!src, "Could not open file %s for reading", path);

	char buffer[BUFFER_SIZE];
	size_t bytes_read, bytes_written;
	unsigned int bytes_total = 0;

	while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src)) > 0) {
		bytes_written = fwrite(buffer, 1, bytes_read, dest);
		error_if(bytes_written != bytes_read, "write error");
		bytes_total += bytes_written;
	}

	error_if(ferror(src), "read error for file %s", path);
	fclose(src);
	return bytes_total;
}

void add_file(const char *path, FILE *dest, qop_state *state) {
	if (state->len >= state->capacity) {
		state->capacity *= 2;
		state->files = realloc(state->files, state->capacity * sizeof(qop_file));
	}

	qop_uint64_t hash = qop_hash(path);
	
	printf("%6d %016llx %s\n", state->len, hash, path);

	unsigned int offset = state->size;
	unsigned int size = copy_into(path, dest);

	state->files[state->len] = (qop_file){
		.hash = hash,
		.offset = offset,
		.size = size
	};
	state->size += size;
	state->len++;
}

void descent_dir(const char *path, FILE *dest, qop_state *state) {
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
			descent_dir(subpath, dest, state);
		}
		else if (file->d_type == DT_REG) {
			char subpath[MAX_PATH];
			snprintf(subpath, MAX_PATH, "%s/%s", path, file->d_name);
			add_file(subpath, dest, state);
		}
	}
}


int main(int argc, char **argv) {
	if (argc < 3) {
		puts("Usage: qopconv <indir/file> <outfile>");
		puts("Example:");
		puts("  qopconv file1 file2 output.qop");
		puts("  qopconv dir1 output.qop");
		exit(1);
	}

	FILE *dest = fopen(argv[argc-1], "w+");
	error_if(!dest, "Could not open file %s for writing", argv[argc-1]);

	qop_state state = {
		.files = malloc(sizeof(qop_file) * 1024),
		.len = 0,
		.capacity = 1024,
		.size = 0
	};

	for (int i = 1; i < argc-1; i++) {
		struct stat s;
		error_if(stat(argv[i], &s) != 0, "Could not stat file %s", argv[i]);

		if (S_ISDIR(s.st_mode)) {
			descent_dir(argv[i], dest, &state);
		}
		else if (S_ISREG(s.st_mode)) {
			add_file(argv[i], dest, &state);
		}
		else {
			die("Path %s is neither a directory nor a regular file", argv[i]);
		}
	}

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

	unsigned int total_size = state.size + QOP_HEADER_SIZE;
	for (int i = 0; i < index_len; i++) {
		write_64(index[i].hash, dest);
		write_32(index[i].offset, dest);
		write_32(index[i].size, dest);
		total_size += 16;
	}

	write_32(total_size, dest);
	write_32(index_bits, dest);
	write_32(QOP_MAGIC, dest);

	free(state.files);
	free(index);
	fclose(dest);

	printf("files: %d, index len: %d, size: %d bytes\n", state.len, index_len, total_size);

	return 0;
}
