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
#include <stdint.h>

#define BLOCK_SIZE 512
#define NUM_BLOCKS 256
#define NAME_MAX_LENGTH 32  // Maximum file name length (including null terminator)
#define DIRECTORY_ENTRIES ((14 * 512) / sizeof(file_entry_t))

typedef struct {
    char name[NAME_MAX_LENGTH];  // File name (null-terminated)
    uint16_t start_block;        // Starting block of the file
    uint32_t size;               // File size in bytes
    uint16_t mode;               // Permissions and type (e.g., S_IFREG)
} file_entry_t;

file_entry_t directory[DIRECTORY_ENTRIES];

// Filesystem image file descriptor and filename
int fs_fd;
char *devfile = NULL;
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

// Helper function to get the superblock (at block 0 and 255)
memefs_superblock_t* get_superblock() {
    if (read_block(0) < 0) {  // Read the first superblock
        perror("Failed to read superblock");
        exit(1);
    }

    // Cast block buffer to superblock structure
    memefs_superblock_t *sb = (memefs_superblock_t *)block_buf;

    // Convert fields from network to host byte order
    sb->main_fat = ntohs(sb->main_fat);
    sb->main_fat_size = ntohs(sb->main_fat_size);
    sb->backup_fat = ntohs(sb->backup_fat);
    sb->backup_fat_size = ntohs(sb->backup_fat_size);
    sb->directory_start = ntohs(sb->directory_start);
    sb->directory_size = ntohs(sb->directory_size);
    sb->num_user_blocks = ntohs(sb->num_user_blocks);
    sb->first_user_block = ntohs(sb->first_user_block);
    sb->fs_version = ntohl(sb->fs_version);  // 32-bit field

    return sb;
}

// Helper function to find the first free block in the FAT
uint16_t find_free_block() {
    // Read the FAT into block_buf
    if (read_block(ntohs(get_superblock()->main_fat)) < 0) {
        perror("Failed to read FAT");
        return 0xFFFF;  // Error indicator
    }

    // Interpret block_buf as the FAT (array of uint16_t)
    uint16_t *fat = (uint16_t *)block_buf;

    // Iterate through the FAT to find a free block (marked as 0xFFFF)
    for (int i = 1; i < NUM_BLOCKS; i++) {  // Skip block 0 (reserved)
        if (ntohs(fat[i]) == 0xFFFF) {  // Convert FAT entry to host byte order
            return i;  // Return the index of the free block
        }
    }

    return 0xFFFF;  // No free blocks found
}


// Initialize the filesystem by opening the image file
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
    printf("getattr called for path: %s\n", path);

    memset(stbuf, 0, sizeof(struct stat));
    
    if (strcmp(path, "/") == 0) {
        // Root directory
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;  // '.' and '..'
	stbuf->st_blocks = 0; // No blocks allocated
	printf("Returning root directory attributes\n");
	return 0;
    }
    else if (strcmp(path, "/file1.txt") == 0 || strcmp(path, "/file2.txt") == 0) {
        // File attributes for file1.txt and file2.txt
        stbuf->st_mode = S_IFREG | 0644;  // Regular file with rw-r--r-- permissions
        stbuf->st_nlink = 1;  // One hard link
        stbuf->st_size = 1024;  // Example file size (in bytes)
	stbuf->st_blocks = (stbuf->st_size + 511) / 512;  // Calculate number of 512-byte blocks
	printf("Returning attributes for %s: size = %ld, blocks = %ld\n", path, stbuf->st_size, 
        stbuf->st_blocks);

        return 0;
    } 

        else{
	stbuf->st_mode = S_IFREG | 0644;  // Regular file with rw-r--r-- permissions
        stbuf->st_nlink = 1;  // One hard link
        stbuf->st_size = 1024;  // Example file size (in bytes)
        stbuf->st_blocks = (stbuf->st_size + 511) / 512;  // Calculate number of 512-byte blocks
        printf("Returning attributes for %s: size = %ld, blocks = %ld\n", path, stbuf->st_size, 
        stbuf->st_blocks);

        return 0;
    }  	
    return -ENOENT; // File or directory not found
}
////////////////////////////////////
// Directory Read (readdir)
static int memefs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/") != 0) {
        return -ENOENT;  // Only root directory supported
    }
    // Example hardcoded files
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, "file1.txt", NULL, 0);
    filler(buf, "file2.txt", NULL, 0);
    filler(buf, "extra1.txt", NULL, 0);
    filler(buf, "extra2.txt", NULL, 0);    
    return 0;
}

// Open file (open)
static int memefs_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/file1.txt") == 0 || strcmp(path, "/file2.txt") == 0) {
        return 0;  // File exists
    }
    return -ENOENT;
}

// Read file data (read)
static int memefs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/file1.txt") == 0 || strcmp(path, "/file2.txt") == 0) {
        // Example hardcoded data
        const char *data = "Hello, FUSE Filesystem!";
        size_t len = strlen(data);
        if (offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        memcpy(buf, data + offset, size);
        return size;
    }
    return -ENOENT;
}



static int find_free_directory_entry() {
    for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
        if (directory[i].name[0] == '\0') {  // Empty slot
            return i;
        }
    }
    return -1;  // No free slots
}

static file_entry_t *find_file_in_directory(const char *path) {
    for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
        if (strcmp(directory[i].name, path + 1) == 0) {  // Skip leading '/'
            return &directory[i];
        }
    }
    return NULL;  // File not found
}


static int add_file_to_directory(file_entry_t *new_file) {
    int index = find_free_directory_entry();
    if (index < 0) {
        return -1;  // No space left in the directory
    }

    memcpy(&directory[index], new_file, sizeof(file_entry_t));
    return 0;  // Success
}

//void load_directory() {
//    for (int i = 0; i < 14; i++) {  // 14 blocks allocated for the directory
//        if (read_block(get_superblock()->directory_start + i) < 0) {
//            perror("Failed to read directory block");
//            exit(1);
//        }

        // Copy block data into the corresponding section of the directory array
//        memcpy((uint8_t *)directory + (i * BLOCK_SIZE), block_buf, BLOCK_SIZE);
//    }
//}

//void save_directory() {
//    for (int i = 0; i < 14; i++) {  // 14 blocks allocated for the directory
//        memcpy(block_buf, (uint8_t *)directory + (i * BLOCK_SIZE), BLOCK_SIZE);
//
//        if (write_block(get_superblock()->directory_start + i) < 0) {
//            perror("Failed to write directory block");
//            exit(1);
//        }
//    }
//}

void load_directory() {
    int directory_start = ntohs(get_superblock()->directory_start);
    int directory_blocks = ntohs(get_superblock()->directory_size);

    for (int i = 0; i < directory_blocks; i++) {
        if (read_block(directory_start + i) < 0) {
            perror("Failed to read directory block");
            exit(1);
        }

        // Copy block data into the corresponding section of the directory array
        memcpy((uint8_t *)directory + (i * BLOCK_SIZE), block_buf, BLOCK_SIZE);
    }

    // Convert fields to host byte order
    for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
        if (directory[i].name[0] != '\0') {  // Skip empty entries
            directory[i].start_block = ntohs(directory[i].start_block);
            directory[i].size = ntohl(directory[i].size);
            directory[i].mode = ntohs(directory[i].mode);
        }
    }
}

void save_directory() {
    int directory_start = ntohs(get_superblock()->directory_start);
    int directory_blocks = ntohs(get_superblock()->directory_size);

    // Convert fields to network byte order before saving
    for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
        if (directory[i].name[0] != '\0') {  // Skip empty entries
            directory[i].start_block = htons(directory[i].start_block);
            directory[i].size = htonl(directory[i].size);
            directory[i].mode = htons(directory[i].mode);
        }
    }

    for (int i = 0; i < directory_blocks; i++) {
        memcpy(block_buf, (uint8_t *)directory + (i * BLOCK_SIZE), BLOCK_SIZE);

        if (write_block(directory_start + i) < 0) {
            perror("Failed to write directory block");
            exit(1);
        }
    }

    // Restore fields to host byte order after saving
    for (int i = 0; i < DIRECTORY_ENTRIES; i++) {
        if (directory[i].name[0] != '\0') {  // Skip empty entries
            directory[i].start_block = ntohs(directory[i].start_block);
            directory[i].size = ntohl(directory[i].size);
            directory[i].mode = ntohs(directory[i].mode);
        }
    }
}



// Create file (create)
static int memefs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    printf("create called for path: %s, mode: %o\n", path, mode);

    // Load the directory into memory
    load_directory();

    // Extract file name from path
    const char *filename = path + 1;  // Skip leading '/'
    if (strlen(filename) >= NAME_MAX_LENGTH) {
        return -ENAMETOOLONG;  // File name too long
    }

    // Check if the file already exists
    if (find_file_in_directory(path) != NULL) {
        return -EEXIST;  // File already exists
    }

    // Find a free block
    uint16_t free_block = find_free_block();
    if (free_block == 0xFFFF) {
        return -ENOSPC;  // No space left
    }

    // Prepare the file entry
    file_entry_t new_file;
    memset(&new_file, 0, sizeof(file_entry_t));
    strncpy(new_file.name, filename, NAME_MAX_LENGTH - 1);
    new_file.start_block = free_block;
    new_file.size = 0;  // New file starts empty
    new_file.mode = mode;

    // Add the file to the directory
    if (add_file_to_directory(&new_file) < 0) {
        printf("create: failed to add file to directory\n");
        return -ENOSPC;  // No directory space left
    }

    // Save the directory back to disk
    save_directory();

    // Clear the block and write it
    memset(block_buf, 0, BLOCK_SIZE);
    if (write_block(free_block) < 0) {
        printf("create: failed to write to block %u\n", free_block);
        return -EIO;  // General input/output error
    }

    printf("create: file '%s' created at block %u\n", filename, free_block);
    return 0;
}


// Truncate file (truncate)
static int memefs_truncate(const char *path, off_t size) {
    // Example implementation: only supports truncating to zero for simplicity
    memset(block_buf, 0, BLOCK_SIZE);
    write_block(1);  // Example: Clear block 1 (hardcoded for simplicity)
    return 0;
}

// Create directory (mkdir)
static int memefs_mkdir(const char *path, mode_t mode) {
    // Directories are not supported in this simple implementation
    return -ENOTSUP;
}

// File system operations structure
static struct fuse_operations memefs_ops = {
    .getattr    = memefs_getattr,
    .readdir    = memefs_readdir,
    .open       = memefs_open,
    .read       = memefs_read,
    .create     = memefs_create,
    .truncate   = memefs_truncate,
    .mkdir      = memefs_mkdir,
};

// Main function
int main(int argc, char **argv) {
    int i;

    // Get the device or image filename from arguments
    for (i = 1; i < argc && argv[i][0] == '-'; i++);
    if (i < argc) {
        devfile = realpath(argv[i], NULL);
        memcpy(&argv[i], &argv[i+1], (argc-i) * sizeof(argv[0]));
        argc--;
    }

    // Initialize the filesystem with the device or image file
    if (devfile) init_fs(devfile);

    // Leave the rest to FUSE
    return fuse_main(argc, argv, &memefs_ops, NULL);
}

