/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * file.h
 *
 * Header file of functions that handle files and directories
 *
 * Updated on October 8, 2019,16:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/
#ifndef FILE_INC_
#define FILE_INC_
#include <inttypes.h>

// Creates a directory and sets permissions that allow creation of new files
gboolean make_directory(const char *dirname);

// Extracts the directory name from a full path name
const char *get_trunc_filename(const char *FileName);

// Returns the file length
uint64_t get_filesize(const char *FileName);

// Returns a XOR HASH value for the contents of a file
uint32_t fhash(FILE *f);

#endif
