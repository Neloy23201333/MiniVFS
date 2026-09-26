// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder
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

#pragma pack(push, 1)
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


int main(int argc, char *argv[]){
    crc32_init();

    // Inteser --> Start

    if (argc != 7) {
        printf("Not enough input");
        return 1;
    }

    char *input = NULL;
    char *output = NULL;
    char *fileName = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            fileName = argv[++i];
        }
    }

    if (!input || !output || !fileName) {
        printf("Invalid arguments.\n");
        return 1;
    }

    // Read entire image into memory
    FILE *fp = fopen(input, "rb");
    if (!fp) {
        printf("Could not open input image file\n");
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        printf("fseek failed on input image\n");
        fclose(fp);
        return 1;
    }

    long imgSize_l = ftell(fp);
    if (imgSize_l < 0) {
        printf("ftell failed on input image\n");
        fclose(fp);
        return 1;
    }

    size_t imgSize = (size_t)imgSize_l;
    if (fseek(fp, 0, SEEK_SET) != 0) {
        printf("fseek again failed on input image\n");
        fclose(fp);
        return 1;
    }

    uint8_t *img = (uint8_t *)malloc(imgSize);
    if (!img) {
        printf("Out of memory allocating image buffer\n");
        fclose(fp);
        return 1;
    }

    if (fread(img, 1, imgSize, fp) != imgSize){
        printf("Failed to read image\n");
        fclose(fp);
        free(img);
        return 1;
    }
    fclose(fp);

    // Map on-disk structures
    if (imgSize < BS * 4) {
        printf("Image too small\n");
        free(img);
        return 1;
    }

    superblock_t *sb = (superblock_t *)img;

    if (sb->blockSize != BS || sb->mag != 0x4D565346u) {
        printf("The image is invalid\n");
        free(img);
        return 1;
    }

    // marking the regions
    uint8_t *ib = img + sb->iBitStart * BS;  // inode bitmap
    uint8_t *db = img + sb->dBitStart * BS;  // data bitmap
    inode_t *it = (inode_t *)(img + sb->iTabStart * BS);  // inode table
    uint8_t *dr = img + sb->dRegStart * BS;  // data region

    // no duplicate file name
    inode_t *r = &it[sb->rootI];
    dirent64_t *dir = (dirent64_t*)(dr+r->directPtr[0]*BS);
    size_t dirEntry = (r->sizeBytes)/sizeof(dirent64_t);

    for (size_t i=0; i<dirEntry; i++) {
        if (strncmp(dir[i].entryName, fileName, MAX_FILE_NAME_SZ)==0) {
            printf("File already exists in the filesystem.\n");
            free(img); 
            return 1;
        }
    }
    
    // Open file to add
    FILE *af = fopen(fileName, "rb");
    if (!af) {
        printf("Could not open file to add: %s\n", fileName);
        free(img);
        return 1;
    }

    if (fseek(af, 0, SEEK_END) != 0) {
        printf("fseek failed on add-file\n");
        fclose(af);
        free(img);
        return 1;
    }

    long fileSize_l = ftell(af);
    if (fileSize_l < 0) {
        printf("ftell failed on add-file\n");
        fclose(af);
        free(img);
        return 1;
    }

    size_t fileSize = (size_t)fileSize_l;
    if (fseek(af, 0, SEEK_SET) != 0) {
        printf("fseek again failed on add-file\n");
        fclose(af);
        free(img);
        return 1;
    }

    uint64_t blockNeed = (fileSize + BS - 1) / BS;
    if (blockNeed > MAX_DIRECT_PTR) {
    fprintf(stderr, "File needs %llu blocks; MiniVSFS supports only %d direct blocks.\n",
            (unsigned long long)blockNeed, MAX_DIRECT_PTR);
    fclose(af);
    free(img);
    return 1;
    }

    // Inteser --> end

    //Wahid --> start
    
   // looking for first free inode
    int64_t freeInIdx = -1;
    for (uint64_t i = 1; i < sb->iCount; i++) {
        if (((ib[i >> 3] >> (i & 7u)) & 1u) == 0) {
            freeInIdx = (int64_t)i;
            break;
        }
    }

    if (freeInIdx < 0) {
        fprintf(stderr, "No free inode available.\n");
        fclose(af);
        free(img);
        return 1;
    }

    // allocating db 
    uint32_t freeDbList[MAX_DIRECT_PTR] = {0};
    uint64_t found = 0;

    if (blockNeed > 0) {
        for (uint64_t i = 0; i < sb->dRegBlocks && found < blockNeed; i++) {
            if (((db[i >> 3] >> (i & 7u)) & 1u) == 0) {
                freeDbList[found++] = (uint32_t)i;
            }
        }
        if (found < blockNeed) {
            fprintf(stderr, "Not enough free data blocks.\n");
            fclose(af);
            free(img);
            return 1;
        }
    }

    // updating bitmap
    ib[freeInIdx >> 3] |= (uint8_t)(1u << (freeInIdx & 7u));
    for (uint64_t j = 0; j < blockNeed; j++) {
        uint64_t b = freeDbList[j];
        db[b >> 3] |= (uint8_t)(1u << (b & 7u));
    }

    // copying file data
    uint8_t *ioBuff = NULL;
    if (blockNeed) {
        ioBuff = (uint8_t *)malloc(BS);
        if (!ioBuff) {
            printf("Out of memory\n");
            fclose(af);
            free(img);
            return 1;
        }
    }

    size_t bytesLeft = fileSize;
    for (uint64_t j = 0; j < blockNeed; j++) {
        size_t toRead;

        if (bytesLeft < BS) 
            toRead = bytesLeft;  
        else 
            toRead = BS;           

        size_t got = fread(ioBuff, 1, toRead, af);
        if (got != toRead) {
            fprintf(stderr, "Short read from input file\n");
            if (ioBuff) free(ioBuff);
            fclose(af);
            free(img);
            return 1;
        }
        if (toRead < BS) memset(ioBuff + toRead, 0, BS - toRead);

        uint64_t abs_block = sb->dRegStart + freeDbList[j];
        if (abs_block >= sb->totBlocks) {
            fprintf(stderr, "Calculated abs_block out of image range\n");
            if (ioBuff) free(ioBuff);
            fclose(af);
            free(img);
            return 1;
        }
        uint8_t *dst = img + abs_block * BS;
        memcpy(dst, ioBuff, BS);

        bytesLeft -= toRead;
    }
    if (ioBuff) free(ioBuff);
    fclose(af);

    // Wahid --> end


    // Abid --> start


    // updating inode table
    inode_t newInode;
    memset(&newInode, 0, sizeof(newInode));
    newInode.mode = 0100000;
    newInode.links = 1;
    newInode.uID = 0;
    newInode.gID = 0;
    newInode.sizeBytes = fileSize;
    uint64_t timeNow = (uint64_t)time(NULL);
    newInode.atime = newInode.mtime = newInode.ctime = timeNow;
    for (uint64_t j = 0; j < blockNeed; j++) {
        newInode.directPtr[j] = (uint32_t)(sb->dRegStart + freeDbList[j]);
    }
    inode_crc_finalize(&newInode);
    it[freeInIdx] = newInode;

    // updating root dir DB that is entering the dir ent for the file
    inode_t *rootIno = &it[0];
    uint32_t absRootDB = rootIno->directPtr[0];
    if (absRootDB == 0) {
        printf("The image is corrupted because root inode has no data block.\n");
        free(img);
        return 1;
    }
    if (absRootDB < sb->dRegStart || absRootDB >= sb->dRegStart + sb->dRegBlocks) {
        printf("The image is corrupted because root data block is out of data region.\n");
        free(img);
        return 1;
    }

    size_t byteOffset = absRootDB * BS;
    dirent64_t *rootDirEntry = (dirent64_t *)(img + byteOffset);

    int freeSlot = -1;

    int rootDirSlots = (int)(BS / sizeof(dirent64_t));
    for (int i = 0; i < rootDirSlots; i++) {
        if (rootDirEntry[i].inodeNo == 0) {
            freeSlot = i;
            break;
        }
    }
    if (freeSlot < 0) {
        printf("The root directory is full.\n");
        free(img);
        return 1;
    }

    dirent64_t *fileDirEntry = &rootDirEntry[freeSlot];
    memset(fileDirEntry, 0, sizeof(*fileDirEntry));
    fileDirEntry->inodeNo = (uint32_t)(freeInIdx + 1);
    fileDirEntry->entryType = 1;
    strncpy(fileDirEntry->entryName, fileName, sizeof(fileDirEntry->entryName));
    fileDirEntry->entryName[sizeof(fileDirEntry->entryName)-1] = '\0';
    dirent_checksum_finalize(fileDirEntry);

    rootIno->sizeBytes += sizeof(dirent64_t);
    rootIno->mtime = timeNow;
    rootIno->links += 1;
    inode_crc_finalize(rootIno);

    sb->mtimeEpoch = timeNow;
    superblock_crc_finalize(sb);

    // generating the output image
    FILE *out = fopen(output, "wb");
    if (!out) {
        perror("Could not open file to write output image");
        free(img);
        return 1;
    }
    size_t bytesWritten = fwrite(img, 1, imgSize, out);
    if (bytesWritten != imgSize) {
        printf("Could not write output image\n");
        fclose(out);
        free(img);
        return 1;
    }
    fclose(out);
    free(img);

    printf("Added '%s' to disk image successfully.\n", fileName);

    return 0;
}
