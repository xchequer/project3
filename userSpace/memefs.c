#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int memefs_getattr(const char *path, struct stat *st)
{
  if (!strcmp(path, "/")) {
    // it's the root directory (just an example, you probably have more directories)
    st->st_mode = S_IFDIR | 0755; // access rights and directory type
    st->st_nlink = 2;             // number of hard links, for directories this is at least 2
    st->st_size = 4096;           // file size
  } else {
    st->st_mode = S_IFREG | 0644; // access rights and regular file type
    st->st_nlink = 2;             // number of hard links
    st->st_size = 4096;           // file size
  }
  // user and group. we use the user's id who is executing the FUSE driver
  st->st_uid = getuid();
  st->st_gid = getgid();
  return 0;
}

int memefs_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	printf( "--> Getting The List of Files of %s\n", path );
	
	filler( buffer, ".", NULL, 0 ); // Current Directory
	filler( buffer, "..", NULL, 0 ); // Parent Directory
	
	if ( strcmp( path, "/" ) == 0 ) // If the user is trying to show the files/directories of the root directory show the following
	{
		filler( buffer, "file54", NULL, 0 );
		filler( buffer, "file349", NULL, 0 );
	}
	
	return 0;
}

static struct fuse_operations memefs_ops = {
   .getattr = memefs_getattr,
   .readdir = memefs_readdir,
};

char *devfile = NULL;

int main(int argc, char **argv)
{
  int i;

  // get the device or image filename from arguments
  for (i = 1; i < argc && argv[i][0] == '-'; i++);
  if (i < argc) {
    devfile = realpath(argv[i], NULL);
    memcpy(&argv[i], &argv[i+1], (argc-i) * sizeof(argv[0]));
    argc--;
  }
  // leave the rest to FUSE
  return fuse_main(argc, argv, &memefs_ops, NULL);
}
