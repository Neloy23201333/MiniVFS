// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_minivsfs.c -o mkfs_builder
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#define BS 4096u               // block size
#define INODE_SIZE 128u
#define ROOT_INODE 1u
#define MAX_DIRECT_PTR 12u
#define MAX_FILE_NAME_SZ 58

uint64_t g_random_seed = 0; // This should be replaced by seed value from the CLI.

// Inteser --> Start

#pragma pack(push,1)
typedef struct {
    uint32_t mag;
    uint32_t ver;
    uint32_t blockSize;
    uint64_t totBlocks;
    uint64_t iCount;
    uint64_t iBitStart;
    uint64_t iBitBlocks;
    uint64_t dBitStart;
    uint64_t dBitBlocks;
    uint64_t iTabStart;
    uint64_t iTabBlocks;
    uint64_t dRegStart;
    uint64_t dRegBlocks;
    uint64_t rootI;
    uint64_t mtimeEpoch;
    uint32_t flag;
    uint32_t checksum;
} superblock_t;
#pragma pack(pop)
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block");

// Inteser --> End

// Wahid --> Start

#pragma pack(push,1)
typedef struct {
    uint16_t mode;
    uint16_t links;
    uint32_t uID;
    uint32_t gID;
    uint64_t sizeBytes;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t directPtr[MAX_DIRECT_PTR];
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t projId;
    uint32_t uID16_gID16;
    uint64_t xattrPtr;
    uint64_t inodeCrc;
} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

// Wahid --> End

// Abid --> Start

#pragma pack(push, 1)
typedef struct {
    uint32_t inodeNo;
    uint8_t entryType;
    char entryName[MAX_FILE_NAME_SZ];
    uint8_t checksum;
} dirent64_t;
#pragma pack(pop)
_Static_assert(sizeof(dirent64_t)==64, "dirent size mismatch");

// Abid --> End


// ==========================DO NOT CHANGE THIS PORTION=========================
// These functions are there for your help. You should refer to the specifications to see how you can use them.
// ====================================CRC32====================================
uint32_t CRC32_TAB[256];
void crc32_init(void) {
    for (uint32_t i=0; i<256; i++) {
        uint32_t c=i;
        for(int j=0; j<8; j++) {
            c = (c&1) ? (0xEDB88320u ^ (c>>1)) : (c>>1);
        }
        CRC32_TAB[i]=c;
    }
}

uint32_t crc32(const void* data, size_t n) {
    const uint8_t* p=(const uint8_t*)data; 
    uint32_t c=0xFFFFFFFFu;
    for(size_t i=0; i<n; i++) {
        c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    }
    return c ^ 0xFFFFFFFFu;
}
// ====================================CRC32====================================

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, BS - 4);
    sb->checksum = s;
    return s;
}

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
void inode_crc_finalize(inode_t* ino) {
    uint8_t tmp[INODE_SIZE]; 
    memcpy(tmp, ino, INODE_SIZE);
    // zero crc area before computing
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inodeCrc = (uint64_t)c; // low 4 bytes carry the crc
}

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) {
        x ^= p[i];   // covers ino(4) + type(1) + name(58)
    }
    de->checksum = x;
}


int main(int argc, char *argv[]) {
    crc32_init();

    // Inteser --> Start

    if(argc != 7) {
        printf("Not enough arguments\n"); 
        printf("Correct Format: \n");
        printf("%s --image <out.img> --size-kib <size> --inodes <count>\n", argv[0]);
        return 1;
    }
    
    char *theImageFile = NULL;
    int givenImgSz = 0, givenInCnt = 0;

    for(int i=1; i < argc; i++){
        if (strcmp(argv[i],"--image") == 0) {
            theImageFile = argv[++i];
        } 
        else if(strcmp(argv[i],"--size-kib") == 0) {
            givenImgSz = atoi(argv[++i]);
        } 
        else if(strcmp(argv[i],"--inodes") == 0) {
            givenInCnt = atoi(argv[++i]);
        }
    }
    
    if(!theImageFile || givenImgSz<180 || givenImgSz>4096 || givenInCnt<128 || givenInCnt>512) {
        printf("Invalid arguments.\n"); 
        return 1;
    }

    uint64_t totImgBlocks = (givenImgSz * 1024) / BS;

    uint64_t ibStart = 1;
    uint64_t dbStart = 2;
    uint64_t ibBlock = 1;
    uint64_t dbBlock = 1;
    uint64_t itStart = 3;

    uint64_t itBlock = (givenInCnt * INODE_SIZE + BS - 1) / BS;
    uint64_t drStart = itStart + itBlock;
    uint64_t drBlock = totImgBlocks - drStart;

    if(drBlock == 0) {
        printf("Not enough space for data region.\n");
        return 1;
    }

    // setting up superblock
    superblock_t sbFields = {0};
    sbFields.mag = 0x4D565346;
    sbFields.ver = 1;
    sbFields.blockSize = BS;        
    sbFields.totBlocks = totImgBlocks;

    sbFields.iCount = givenInCnt;
    sbFields.iBitStart = ibStart;         
    sbFields.iBitBlocks = ibBlock;

    sbFields.dBitStart = dbStart;
    sbFields.dBitBlocks = dbBlock;

    sbFields.iTabStart = itStart;
    sbFields.iTabBlocks = itBlock;

    sbFields.dRegStart = drStart;
    sbFields.dRegBlocks = drBlock;

    sbFields.flag = 0;
    sbFields.rootI = ROOT_INODE;     
    sbFields.mtimeEpoch = (uint64_t)time(NULL);  

    uint8_t sbBlock[BS] = {0};
    memcpy(sbBlock, &sbFields, sizeof(superblock_t));

    superblock_crc_finalize((superblock_t*)sbBlock);
    // Inteser --> End
    
    // Wahid --> Start

    // setting up inode and data bitmap
    uint8_t inodeBitmap[BS];
    memset(inodeBitmap, 0, BS);
    inodeBitmap[0] |= 1 << 0;   

    uint8_t dataBitmap[BS];
    memset(dataBitmap, 0, BS);
    dataBitmap[0] |= 1 << 0;  

    // setting up inode table
    size_t iTabBytes = itBlock * BS;
    uint8_t *inodeTab = calloc(1, iTabBytes);
    
    if(inodeTab == NULL) { 
        printf("Out of memory for inode table\n");
        return 1;
    }

    // setting up inode for root directory
    inode_t rootInode;
    memset(&rootInode, 0, sizeof(rootInode));
    rootInode.mode = 0040000;                   
    rootInode.links = 2;
    rootInode.uID = 0;
    rootInode.gID = 0;
    rootInode.sizeBytes = 2 * sizeof(dirent64_t);
    rootInode.atime = rootInode.mtime = rootInode.ctime = (uint64_t)time(NULL);
    rootInode.directPtr[0] = (uint32_t) drStart;

    inode_crc_finalize(&rootInode);
    memcpy(inodeTab + 0 * INODE_SIZE, &rootInode, INODE_SIZE);

    // Wahid --> End

    // Abid --> Start

    // Setting up the root directory
    uint8_t rootDirDB[BS];
    memset(rootDirDB, 0, BS);

    dirent64_t* rootDirEntry = (dirent64_t*) rootDirDB;

    // The (.) entry
    rootDirEntry[0].inodeNo = ROOT_INODE;
    rootDirEntry[0].entryType = 2; 
    strncpy(rootDirEntry[0].entryName, ".", MAX_FILE_NAME_SZ - 1);
    rootDirEntry[0].entryName[MAX_FILE_NAME_SZ - 1] = '\0';
    dirent_checksum_finalize(&rootDirEntry[0]);

    // The (..) entry
    rootDirEntry[1].inodeNo = ROOT_INODE;
    rootDirEntry[1].entryType = 2; 
    strncpy(rootDirEntry[1].entryName, "..", MAX_FILE_NAME_SZ - 1);
    rootDirEntry[1].entryName[MAX_FILE_NAME_SZ - 1] = '\0';
    dirent_checksum_finalize(&rootDirEntry[1]);

    // Making the disk image file
    FILE *fp = fopen(theImageFile, "wb");
    if(!fp) { 
        perror("Failed to execute fopen"); 
        free(inodeTab); 
        return 1; 
    }

    if(fwrite(sbBlock, BS, 1, fp) != 1) { 
        perror("Failed to write the superblock in the image file"); 
        fclose(fp); 
        free(inodeTab); 
        return 1; 
    }

    if(fwrite(inodeBitmap, BS, 1, fp) != 1) { 
        perror("Failed to write the inode bitmap in the image file"); 
        fclose(fp); 
        free(inodeTab); 
        return 1; 
    }

    if(fwrite(dataBitmap, BS, 1, fp) != 1) { 
        perror("Failed to write the data bitmap in the image file"); 
        fclose(fp); 
        free(inodeTab); 
        return 1; 
    }

    if(fwrite(inodeTab, iTabBytes, 1, fp) != 1) { 
        perror("Failed to write the inode table in the image file"); 
        fclose(fp); 
        free(inodeTab); 
        return 1; 
    }

    if(fwrite(rootDirDB, BS, 1, fp) != 1) { 
        perror("Failed to write the root directory data block in the image file"); 
        fclose(fp); 
        free(inodeTab); 
        return 1; 
    }

    uint8_t tempBuff[BS];
    memset(tempBuff, 0, BS);
    for(uint64_t i = 1; i < drBlock; i++) {
        fwrite(tempBuff, BS, 1, fp);
    }

    fclose(fp);
    free(inodeTab);

    printf("The MiniVSFS image '%s' has been created successfully.\n", theImageFile);

    return 0;
}
