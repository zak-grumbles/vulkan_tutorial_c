#include <stdint.h>

/**
 * Utility function for reading contents of file.
 * 
 * Params:
 *   filename - path to file
 * 
 * Returns:
 *   string containing file contents.
 */
uint32_t* read_file(const char* filename, size_t* length);