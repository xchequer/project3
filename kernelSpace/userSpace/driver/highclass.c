#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define BLOCK_SIZE 512
#define NUM_BLOCKS 256

// Filesystem image file descriptor
int fs_fd;
uint8_t block_buf[BLOCK_SIZE];  // Buffer for reading and writing blocks

// Structure representing the superblock metadata for the filesystem.
typedef struct memefs_superblock {
    char signature[16];         // Filesystem signature
    uint8_t cleanly_unmounted;  // Flag for unmounted state
    uint8_t reseerved1[3];      // Reserved bytes
    uint32_t fs_version;        // Filesystem version
    uint8_t fs_ctime[8];        // Creation timestamp in BCD format
    uint16_t main_fat;          // Starting block for main FAT
    uint16_t main_fat_size;     // Size of the main FAT
    uint16_t backup_fat;        // Starting block for backup FAT
    uint16_t backup_fat_size;   // Size of the backup FAT
    uint16_t directory_start;   // Starting block for directory
    uint16_t directory_size;    // Directory size in blocks
    uint16_t num_user_blocks;   // Number of user data blocks
    uint16_t first_user_block;  // First user data block
    char volume_label[16];      // Volume label
    uint8_t unused[448];        // Unused space for alignment
} __attribute__((packed)) memefs_superblock_t;

// Helper function to read a block from the filesystem image
int read_block(uint16_t block_num) {
    lseek(fs_fd, block_num * BLOCK_SIZE, SEEK_SET);
    return read(fs_fd, block_buf, BLOCK_SIZE);
}

// Helper function to write a block to the filesystem image
int write_block(uint16_t block_num) {
    lseek(fs_fd, block_num * BLOCK_SIZE, SEEK_SET);
    return write(fs_fd, block_buf, BLOCK_SIZE);
}

// Helper function to find the first free block in the FAT
uint16_t find_free_block() {
    uint16_t *fat = (uint16_t *)block_buf;  // Assuming block_buf contains the FAT
    for (int i = 1; i < NUM_BLOCKS; i++) {  // Skip block 0 (reserved)
        if (fat[i] == 0xFFFF) {  // Check if the block is free (0xFFFF marks a free block)
            return i;
        }
    }
    return 0xFFFF;  // No free blocks found
}

// Helper function to get the superblock (at block 0 and 255)
memefs_superblock_t* get_superblock() {
    read_block(0);  // Read the first superblock
    return (memefs_superblock_t*)block_buf;
}

// Initialize the filesystem by reading the superblock and FAT
void init_fs(const char *image_path) {
    fs_fd = open(image_path, O_RDWR);
    if (fs_fd < 0) {
        perror("open filesystem image");
        exit(1);
    }
    // Read the superblock (you can check the signature here if needed)
    get_superblock();
}

// File Attributes (getattr)
static int memefs_getattr(const char *path, struct stat *stbuf) {
    memefs_superblock_t *sb = get_superblock();
    memset(stbuf, 0, sizeof(struct stat));
    
    if (strcmp(path, "/") == 0) {
        // Root directory
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;  // '.' and '..'
    } else {
        // Files
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = 0;  // Modify with actual file size logic if necessary
    }
    return 0;
}

// Directory Read (readdir)
static int memefs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    memefs_superblock_t *sb = get_superblock();
    
    if (strcmp(path, "/") != 0) {
        return -ENOENT;  // Only root directory supported in this example
    }

    // Return the names of files in the root directory (hardcoded for this example)
    filler(buf, "file1.txt", NULL, 0);
    filler(buf, "file2.txt", NULL, 0);
    return 0;
}

// Open file (open)
static int memefs_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/file1.txt") == 0 || strcmp(path, "/file2.txt") == 0) {
        return 0;  // File exists and can be opened
    }
    return -ENOENT;
}

// Read file data (read)
static int memefs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    uint16_t block_num = 1;  // For this example, we assume all files start at block 1
    if (strcmp(path, "/file1.txt") == 0 || strcmp(path, "/file2.txt") == 0) {
        // Read the file's data starting at block 1
        read_block(block_num);
        memcpy(buf, block_buf, size);
        return size;
    }
    return -ENOENT;
}

// File creation (create)
static int memefs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    uint16_t free_block = find_free_block();
    if (free_block == 0xFFFF) {
        return -ENOSPC;  // No free blocks available
    }
    
    // For this example, we're creating a new file with a hardcoded size of 512 bytes
    // You can extend this by writing metadata into the directory block and the FAT
    
    memset(block_buf, 0, BLOCK_SIZE);  // Clear the block buffer
    write_block(free_block);           // Write the new file's block
    return 0;
}

// Helper function to update the FAT after writing a file
static void update_fat(uint16_t block_num, uint16_t next_block) {
    uint16_t *fat = (uint16_t *)block_buf;
    fat[block_num] = htons(next_block);
    write_block(block_num);  // Update the FAT table
}

// Helper function to delete a file
static int memefs_unlink(const char *path) {
    uint16_t block_num = 1;  // For simplicity, assume files start at block 1

    // Deallocate the block in the FAT
    uint16_t *fat = (uint16_t *)block_buf;
    fat[block_num] = 0xFFFF;  // Mark the block as free
    write_block(block_num);

    return 0;
}

// Truncate file (truncate)
static int memefs_truncate(const char *path, off_t size) {
    // For this example, we'll truncate to the first block, this can be adjusted.
    uint16_t block_num = 1;  // We assume the file starts at block 1
    uint16_t *fat = (uint16_t *)block_buf;
    memset(block_buf, 0, BLOCK_SIZE);  // Clear the block

    if (size <= BLOCK_SIZE) {
        // If the requested size is smaller or equal to 512 bytes, truncate the file
        fat[block_num] = 0xFFFF;  // Mark the block as free
        write_block(block_num);   // Update the FAT
    } else {
        // If the size is larger, we would extend the file logic here.
        return -ENOSPC;
    }

    return 0;
}

// Create directory (mkdir)
static int memefs_mkdir(const char *path, mode_t mode) {
    // For this example, directories are not supported, so we return an error.
    return -ENOTSUP;
}

// File system operations structure
static struct fuse_operations memefs_oper = {
    .getattr    = memefs_getattr,
    .readdir    = memefs_readdir,
    .open       = memefs_open,
    .read       = memefs_read,
    .create     = memefs_create,
    .unlink     = memefs_unlink,
    .truncate   = memefs_truncate,
    .mkdir      = memefs_mkdir,
};
char *devfile = NULL;
// Main function to mount the filesystem
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <filesystem image> <mount point>\n", argv[0]);
        return 1;
    }
  int i;

  // get the device or image filename from arguments
  for (i = 1; i < argc && argv[i][0] == '-'; i++);
  if (i < argc) {
    devfile = realpath(argv[i], NULL);
    memcpy(&argv[i], &argv[i+1], (argc-i) * sizeof(argv[0]));
    argc--;
  }

    init_fs(devfile);  // Initialize the filesystem with the image file

    // Start the FUSE filesystem
    return fuse_main(argc, argv, &memefs_oper, NULL);
}

