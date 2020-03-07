/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * file.c
 *
 * Functions that handle files and directories
 *
 * Updated on October 8, 2019,16:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>



// Creates a directory and sets permissions that allow creation of new files
gboolean make_directory(const char *dirname) {
  return !mkdir(dirname, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)
	 || (errno==EEXIST);
}


// Extracts the directory name from a full path name
const char *get_trunc_filename(const char *FileName) {
  char *pt= strrchr(FileName, (int)'/');
  if (pt != NULL)
    return pt+1;
  else
    return FileName;
}


// Returns the file length
uint64_t get_filesize(const char *FileName)
{
  struct stat file;
  if(!stat(FileName, &file))
  {
    return (uint64_t)file.st_size;
  }
  return 0;
}


// Returns a XOR HASH value for the contents of a file
uint32_t fhash(FILE *f) {
  assert(f != NULL);
  rewind(f);
  uint32_t sum= 0;
  uint32_t aux= 0;
  while (fread(&aux, 1, sizeof(aux), f) > 0)
      sum ^= aux;
  return sum;
}

