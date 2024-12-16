#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>


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
		filler( buffer, "testfile1", NULL, 0 );
		filler( buffer, "testfile2", NULL, 0 );
	}
	
	return 0;
}

int memefs_open(const char *path, struct fuse_file_info *fi)
{
	if (strcmp(path+1, "testfile1") != 0)
		return -ENOENT;

	else if (strcmp(path+1, "testfile2") != 0)
		return -ENOENT;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;

	return 0;
}

int memefs_read(const char *path, char *buffer, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	char testfile1Text[] = "Hello World From testfile1!";
	char testfile2Text[] = "Hello World From testfile2!";
	char *selectedText = NULL;

	if ( strcmp( path, "/testfile1" ) == 0 )
		selectedText = testfile1Text;
	else if ( strcmp( path, "/testfile2" ) == 0 )
		selectedText = testfile2Text;
	else
		return -1;

	memcpy( buffer, selectedText + offset, size );
		
	return strlen( selectedText ) - offset;
}

int memefs_mknod(const char *path, mode_t mode, dev_t rdev) {
    // Check if the parent directory exists
    struct stat sb;
    if (memefs_getattr(path, &sb) < 0) {
        return -ENOENT;
    }

    // Create the file/directory/symlink as appropriate
    if (S_ISREG(mode)) {
        // Regular file
        int fd = open(path, O_CREAT | O_WRONLY, mode);
        if (fd < 0) {
            return -errno;
        }
        close(fd);
    } else if (S_ISDIR(mode)) {
        // Directory
        mkdir(path, mode);
    } else if (S_ISLNK(mode)) {
        // Symbolic link
        symlink(path, path);
    }

    return 0;
}

int memefs_unlink(const char *path) {
    // Check if the file exists
    struct stat sb;
    if (memefs_getattr(path, &sb) < 0) {
        return -ENOENT;
    }

    // Unlink the file
    if (unlink(path) < 0) {
        return -errno;
    }

    return 0;
}

int memefs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Check if the file exists and is writable
    struct stat sb;
    if (memefs_getattr(path, &sb) < 0 || !S_ISREG(sb.st_mode)) {
        return -ENOENT;
    }

    // Open the file for writing
    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) {
        return -errno;
    }

    // Write the data
    if (pwrite(fd, buf, size, offset) < size) {
        return -errno;
    }

    close(fd);
    return size;
}

int memefs_truncate(const char *path, off_t size) {
    // Check if the file exists and is a regular file
    struct stat sb;
    if (memefs_getattr(path, &sb) < 0 || !S_ISREG(sb.st_mode)) {
        return -ENOENT;
    }

    // Truncate the file
    if (ftruncate(open(path, O_WRONLY), size) < 0) {
        return -errno;
    }

    return 0;
}

static struct fuse_operations memefs_ops = {
   .getattr = memefs_getattr,
   .readdir = memefs_readdir,
   .open    = memefs_open,
   .read    = memefs_read,
   .mknod  = memefs_mknod,
   .unlink  = memefs_unlink,
   .write   = memefs_write,
   .truncate = memefs_truncate,
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
