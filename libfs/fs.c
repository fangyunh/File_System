#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF

/* TODO: Phase 1 */
// Data structures of blocks
struct superblock {
    char signature[8];
    uint16_t disk_block_num;    // Total amount of blocks of virtual disk
    uint16_t root_dict;
    uint16_t start_idx;         // Data block start index
    uint16_t data_block_num;
    uint8_t fat_block_num;
    char padding[4079];
} __attribute__ ((packed));

struct fat {
    uint16_t* entries;
    size_t size;
};

struct root {
    char file_name[FS_FILENAME_LEN];
    uint32_t file_size;
    uint16_t first_data_idx;
    char padding[10];
}__attribute__((packed));

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
    struct superblock sb;

    if (block_disk_open(diskname) == -1) {
        return -1;
    }

    if(block_read(0, (void*)sb) == -1) {
        return -1;
    }

    if (strcmp(sb.signature, "ECS150FS") != 0) {
        return -1;
    }

    

}

int fs_umount(void)
{
	/* TODO: Phase 1 */
}

int fs_info(void)
{
	/* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

