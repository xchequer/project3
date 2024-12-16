#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("MEMEfs reader kernel module");
MODULE_VERSION("0.1");

#define MAX_FILE_SIZE 1024

static char *filepath = "/tmp/memefs/file1.txt";
static char buffer[MAX_FILE_SIZE];

static int read_file_content(void) {
    struct file *file;
    loff_t pos = 0;
    int read_bytes;

    // Open the file
    file = filp_open(filepath, O_RDONLY, 0);
    if (IS_ERR(file)) {
        printk(KERN_ERR "MEMEfs: Error opening file %s\n", filepath);
        return PTR_ERR(file);
    }

    // Read the file content
    read_bytes = kernel_read(file, buffer, MAX_FILE_SIZE - 1, &pos);
    if (read_bytes < 0) {
        printk(KERN_ERR "MEMEfs: Error reading file\n");
        filp_close(file, NULL);
        return read_bytes;
    }

    // Null terminate the buffer
    buffer[read_bytes] = '\0';

    // Print the content to kernel log
    printk(KERN_INFO "MEMEfs: File content: %s\n", buffer);

    // Close the file
    filp_close(file, NULL);
    return 0;
}

static int __init memefs_module_init(void) {
    printk(KERN_INFO "MEMEfs: Module loading\n");
    return read_file_content();
}

static void __exit memefs_module_exit(void) {
    printk(KERN_INFO "MEMEfs: Module unloaded\n");
}

module_init(memefs_module_init);
module_exit(memefs_module_exit);
