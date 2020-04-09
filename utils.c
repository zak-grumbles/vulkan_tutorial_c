#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

uint32_t* read_file(const char* filename, size_t* length) {

    FILE* f = fopen(filename, "rb");

    char* contents = NULL;
    if(f != NULL) {
        fseek(f, 0L, SEEK_END);

        long file_size = ftell(f);

        fseek(f, 0L, SEEK_SET);

        contents = (char*)malloc(file_size * sizeof(char));

        size_t read = fread(contents, sizeof(char), file_size, f);
        *length = read;

        if(read != file_size) {
            fprintf(stderr, "Unable to read file %s\n", filename);
        }

        fclose(f);
    }
    else {
        fprintf(stderr, "Unable to open file: \"%s\"\n", filename);
    }

    return (uint32_t*)contents;
}
