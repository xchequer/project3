#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// Structure representing a file entry (simplified)

typedef struct {
    char name[32];
    uint16_t start_block;
    uint32_t size;
    uint16_t permissions;
} FileEntry;

// Hardcoded test files (replace with parsing from your image file)
FileEntry files[] = {
    {"jumanji.txt", 1, 1024, 0644},
    {"voltron.txt", 2, 2048, 0644}
};
int file_count = 2;

// Open the image file globally (for simplicity)
static int img_fd;

// Finds the first free block in the FAT (returning 0xFFFF if no free block is found)
uint16_t find_free_block() {
    uint16_t *fat = FileEntry->start_block;  // Assuming block_buf is already mapped to the FAT

    // Scan FAT for a free block
    for (int i = 1; i < 256; i++) {  // Skip block 0, as it's reserved
        if (fat[i] == 0xFFFF) {
            return i;  // Return the first free block
        }
    }

    return 0xFFFF;  // No free blocks available
}

// Updates the FAT to mark the block as in use
void update_fat(uint16_t block) {
    uint16_t *fat = FileEntry->start_block;

    // Mark the block as allocated in the FAT
    fat[block] = 0xFFFF;  // Mark this block as used (can be modified for chaining if needed)

    // Write the updated FAT to the image file (assuming you're writing to the image at block locations)
    // For simplicity, this writes the entire FAT to the image
    int fd = open("filesystem.img", O_RDWR);
    lseek(fd, block * 512, SEEK_SET);  // Positioning at the FAT block location
    write(fd, FileEntry->start_block, 512);         // Write the updated FAT block
    close(fd);
}



// Read file attributes (stat)
static int memefs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    for (int i = 0; i < file_count; i++) {
        if (strcmp(path + 1, files[i].name) == 0) {
            stbuf->st_mode = S_IFREG | files[i].permissions;
            stbuf->st_nlink = 1;
            stbuf->st_size = files[i].size;
            return 0;
        }
    }
    return -ENOENT;
}

// Read directory contents
static int memefs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    for (int i = 0; i < file_count; i++) {
        filler(buf, files[i].name, NULL, 0);
    }
    return 0;
}

// Open file
static int memefs_open(const char *path, struct fuse_file_info *fi) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(path + 1, files[i].name) == 0)
            return 0;
    }
    return -ENOENT;
}

// Read data from file
static int memefs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Placeholder: Replace with actual block reading logic
    strcpy(buf, "Hello from the filesystem!\n"); // For testing
    return strlen(buf);
}

static int memefs_mkdir(const char *path, mode_t mode) {
    // For simplicity, just check if the directory already exists
    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }
    return 0;
}

static int memefs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    // We will create files in the root directory (i.e., "/" in this case)
    // In this simple example, we're not handling directories, so files are created directly in the root.
    
    // Check if the file already exists
    for (int i = 0; i < file_count; i++) {
        if (strcmp(path + 1, files[i].name) == 0) {
            return -EEXIST;  // File already exists
        }
    }

    // Find a free block in the FAT to start the file (this would be a real FAT check in a full implementation)
    uint16_t start_block = find_free_block();  // Function to find a free block in the FAT
    if (start_block == 0xFFFF) {
        return -ENOSPC;  // No space left in the filesystem
    }

    // Reserve the block in the FAT and initialize the file entry
    update_fat(start_block);  // Mark the block as used in the FAT (write this to the image file)
    
    // Add the file entry in the directory (here, we're just adding it to a simple array)
    if (file_count < MAX_FILES) {
        strncpy(files[file_count].name, path + 1, MAX_FILENAME_LENGTH);  // Remove leading '/' in the path
        files[file_count].start_block = start_block;  // Assign the start block to the file
        files[file_count].size = 0;  // Initially the file size is 0
        file_count++;
    } else {
        return -ENFILE;  // Too many files (simple check)
    }

    // Optionally, initialize the file data (e.g., writing 0s to the data block)
    // This could be done in a real implementation by writing a block to the image file.

    return 0;  // Return 0 on success (file created)
}



static int memefs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Find the file entry
    for (int i = 0; i < file_count; i++) {
        if (strcmp(path + 1, files[i].name) == 0) {
            // Simple example: writing to the file (you'd need to update the image file with actual data blocks)
            // For simplicity, assume the file data is being stored in the buffer and can be written directly.

            // Here you would normally manage the FAT entries and write the data to the appropriate blocks in the image file
            lseek(img_fd, files[i].start_block * 512 + offset, SEEK_SET);
            write(img_fd, buf, size);

            // Update the file's size (in a real filesystem, you'd also update the FAT table)
            files[i].size = size + offset;

            return size;
        }
    }
    return -ENOENT;
}

static int memefs_truncate(const char *path, off_t size) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(path + 1, files[i].name) == 0) {
            if (size < files[i].size) {
                // If shrinking, we would update the FAT table to mark unused blocks as free
                // (not implemented in this simplified version)
                files[i].size = size;
                return 0;
            }
            // If the size is increasing, we'd need to allocate more blocks in the FAT (not implemented)
            files[i].size = size;
            return 0;
        }
    }
    return -ENOENT;
}

// Define FUSE operations
static struct fuse_operations memefs_ops = {
    .getattr = memefs_getattr,
    .readdir = memefs_readdir,
    .open    = memefs_open,
    .read    = memefs_read,
    .mkdir   = memefs_mkdir,
    .create  = memefs_create,
    .write   = memefs_write,
    .truncate= memefs_truncate,
};

// Main function
int main(int argc, char *argv[]) {
    //img_fd = open(argv[1], O_RDONLY); // Open the image file
    //if (img_fd < 0) {
    //    perror("Failed to open image file");
    //    return 1;
   // }
  int i;

  // get the device or image filename from arguments
  for (i = 1; i < argc && argv[i][0] == '-'; i++);
  if (i < argc) {
    img_fd = realpath(argv[i], NULL);
    memcpy(&argv[i], &argv[i+1], (argc-i) * sizeof(argv[0]));
    argc--;
  }
	
    return fuse_main(argc, argv, &memefs_ops, NULL);
}

