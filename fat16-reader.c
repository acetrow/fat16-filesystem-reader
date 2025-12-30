#include <stdio.h>
#include <stdint.h>
//for file opening modes ( O_RDONLY)
#include <fcntl.h>
//read function
#include <unistd.h>
//for malloc and free
#include <stdlib.h>  
#include <string.h>

// BootSector structure (TASK 2)
typedef struct __attribute__((__packed__)) 
{
    uint8_t BS_jmpBoot[ 3 ]; // x86 jump instr. to boot code
    uint8_t BS_OEMName[ 8 ]; // What created the filesystem
    uint16_t BPB_BytsPerSec; // Bytes per Sector
    uint8_t BPB_SecPerClus; // Sectors per Cluster
    uint16_t BPB_RsvdSecCnt; // Reserved Sector Count
    uint8_t BPB_NumFATs; // Number of copies of FAT
    uint16_t BPB_RootEntCnt; // FAT12/FAT16: size of root DIR
    uint16_t BPB_TotSec16; // Sectors, may be 0, see below
    uint8_t BPB_Media; // Media type, e.g. fixed
    uint16_t BPB_FATSz16; // Sectors in FAT (FAT12 or FAT16)
    uint16_t BPB_SecPerTrk; // Sectors per Track
    uint16_t BPB_NumHeads; // Number of heads in disk
    uint32_t BPB_HiddSec; // Hidden Sector count
    uint32_t BPB_TotSec32; // Sectors if BPB_TotSec16 == 0
    uint8_t BS_DrvNum; // 0 = floppy, 0x80 = hard disk
    uint8_t BS_Reserved1; // 
    uint8_t BS_BootSig; // Should = 0x29
    uint32_t BS_VolID; // 'Unique' ID for volume
    uint8_t BS_VolLab[ 11 ]; // Non zero terminated string
    uint8_t BS_FilSysType[ 8 ]; // e.g. 'FAT16 ' (Not 0 term.)
} BootSector;

// DirectoryEntry structure (TASK 3)
typedef struct __attribute__((__packed__)) {
    uint8_t DIR_Name[ 11 ]; // Non zero terminated string
    uint8_t DIR_Attr; // File attributes
    uint8_t DIR_NTRes; // Used by Windows NT, ignore
    uint8_t DIR_CrtTimeTenth; // Tenths of sec. 0...199
    uint16_t DIR_CrtTime; // Creation Time in 2s intervals
    uint16_t DIR_CrtDate; // Date file created
    uint16_t DIR_LstAccDate; // Date of last read or write
    uint16_t DIR_FstClusHI; // Top 16 bits file's 1st cluster
    uint16_t DIR_WrtTime; // Time of last write
    uint16_t DIR_WrtDate; // Date of last write
    uint16_t DIR_FstClusLO; // Lower 16 bits file's 1st cluster
    uint32_t DIR_FileSize; // File size in bytes
} DirectoryEntry;

// file structure (TASK 5)
typedef struct {
    int fdesc;  // File descriptor
    size_t fileLength;  // Length of the file
    size_t currentPosition;  // Current position in the file
    uint16_t startCluster;  // Starting cluster of the file
} File;

//Long Name structure (TASK 6)
typedef struct {
    uint8_t LDIR_Ord; // Order/ position in sequence/ set
    uint8_t LDIR_Name1[ 10 ]; // First 5 UNICODE characters
    uint8_t LDIR_Attr; // = ATTR_LONG_NAME (xx001111)
    uint8_t LDIR_Type; // Should = 0
    uint8_t LDIR_Chksum; // Checksum of short name
    uint8_t LDIR_Name2[ 12 ]; // Middle 6 UNICODE characters
    uint16_t LDIR_FstClusLO; // MUST be zero
    uint8_t LDIR_Name3[ 4 ]; // Last 2 UNICODE characters

} LongName;

/*/////////////////////////////////////////////////////////////
                        TASK 1 + 2
/////////////////////////////////////////////////////////////*/

//read function
//a pointer to a constant character (string) representing the filename
void readDisk(const char *filename, BootSector *bootSector) 
{
    //open disk image
    int fdisk = open(filename, O_RDONLY);

    if (fdisk == -1) 
    {
        perror("Unable to open disk file");
        return;
    }

    //read up to count bytes from file descriptor
    //ssize_t read(int fdesc, void *buf, size_t count);
    ssize_t reading = read(fdisk, bootSector, sizeof(BootSector));

    if (reading == -1) 
    {
        perror("Error reading from disk file");
    } 
    else 
    {
        printf("Bytes per Sector: %u\n", bootSector->BPB_BytsPerSec);
        printf("Sectors per Cluster: %u\n", bootSector->BPB_SecPerClus);
        printf("Reserved Sector Count: %u\n", bootSector->BPB_RsvdSecCnt);
        printf("Number of copies of FAT: %u\n", bootSector->BPB_NumFATs);
        printf("FAT12/FAT16: size of root DIR: %u\n", bootSector->BPB_RootEntCnt);
        printf("Sectors, may be 0, see below: %u\n", bootSector->BPB_TotSec16);
        printf("Sectors in FAT (FAT12 or FAT16): %u\n", bootSector->BPB_FATSz16);
        printf("Sectors if BPB_TotSec16 == 0: %u\n", bootSector->BPB_TotSec32);
        printf("Non zero terminated string: %s\n", bootSector->BS_VolLab);

    }

    close(fdisk);
}

/*/////////////////////////////////////////////////////////////
                        TASK 3
/////////////////////////////////////////////////////////////*/

// Function to load a copy of the first FAT into memory
void loadFAT(const char *filename, BootSector bootSector, uint16_t *fat, size_t fatSize) 
{
    int fdisk = open(filename, O_RDONLY);

    if (fdisk == -1) 
    {
        perror("Unable to open disk file");
        return;
    }

    //offset is at first FAT which is after reserved sectors
    //Reserved Sector Count * Bytes per Sector
    off_t offset = bootSector.BPB_RsvdSecCnt * bootSector.BPB_BytsPerSec;
    if (lseek(fdisk, offset, SEEK_SET) == -1) 
    {
        perror("Error seeking to the start of the first FAT");
        close(fdisk);
        return;
    }

    //read the first FAT into memory
    ssize_t reading = read(fdisk, fat, fatSize);
    if (reading == -1) 
    {
        perror("Error reading the first FAT");
    } 

    close(fdisk);
}

//function to get ordered lists of file cluster starting from the initial cluster
void fileClusters(uint16_t *fat, size_t fatSize, uint16_t startCluster, size_t *clusters, size_t *clustersNumber) 
{
    //number of clusters (counter)
    *clustersNumber = 0;

    //starting cluster is not end of file
    while (startCluster < 0xFFF8) 
    {
        //stores the new cluster in the clusters array
        clusters[*clustersNumber] = startCluster;
        (*clustersNumber)++;

        //go to the next cluster
        startCluster = fat[startCluster];
    }
}


/*/////////////////////////////////////////////////////////////
                        TASK 4
/////////////////////////////////////////////////////////////*/

void convertDateTime(uint16_t time, uint16_t date, uint16_t *hours, uint16_t *minutes, uint16_t *seconds, uint16_t *day, uint16_t *month, uint16_t *year) 
{
    //Bits 15-11: hours (0-23)
    //Bits 10-5: minutes (0-59)
    //Bits 4-0: seconds/2 (0-29, 2-second intervals)

    //right shifts by 11 bits - left with 5 bits representing hours
    //bits >= 5 are 0
    *hours = (time >> 11) & 0x1F;

    //right shifts by 5 bits
    *minutes = (time >> 5) & 0x3F;

    //2-second interval
    *seconds = (time & 0x1F) * 2; 
    
    //0x1F (total 31)
    *day = date & 0x1F;
    //right shifts by 5 bits
    //0x0F (total 15)
    *month = (date >> 5) & 0x0F;
    //offset year = 1980 (7 bits/year)
    *year = ((date >> 9) & 0x7F) + 1980; 
}

void printRootDirectory(const char *filename, BootSector bootSector) 
{
    int fdisk = open(filename, O_RDONLY);

    if (fdisk == -1) 
    {
        perror("Unable to open disk file");
        return;
    }

    //first sector of the root directory
    //(Reserved Sector Count + Number of copies of FAT * Sectors in FAT) * Bytes per Sector
    off_t offset = (bootSector.BPB_RsvdSecCnt + bootSector.BPB_NumFATs * bootSector.BPB_FATSz16) * bootSector.BPB_BytsPerSec;

    if (lseek(fdisk, offset, SEEK_SET) == -1) 
    {
        perror("Error seeking to the start of the root directory");
        close(fdisk);
        return;
    }

    //numOfEntry = size of root DIR
    size_t numOfEntry = bootSector.BPB_RootEntCnt;

    printf("-------------------------------------------------------------------------------------------------\n");
    printf("| Cluster \t | Date \t | Time \t | Attributes \t | Size \t | Name \t|\n");
    printf("-------------------------------------------------------------------------------------------------\n");

    for (size_t i = 0; i < numOfEntry; i++) 
    {
        DirectoryEntry entry;
        ssize_t reading = read(fdisk, &entry, sizeof(DirectoryEntry));

        if (reading == -1) 
        {
            perror("Error to read directory entry");
            break;
        }
        //the first byte of the directory name is zero, there are no further valid entries
        //0xe5 - specific entry is currently unused (deleted files)
        //& 0x0F operation isolates the lower 4 bits of entry.DIR_Attr
        //!= 0x0F, ensuring that the lower 4 bits don't have all bits set == a valid file or directory entry
        if (entry.DIR_Name[0] != 0x00 && entry.DIR_Name[0] != 0xE5 && (entry.DIR_Attr & 0x0F) != 0x0F ) 
        {
            //declares variables of type uint16_t
            uint16_t hours, minutes, seconds, day, month, year;
            convertDateTime(entry.DIR_WrtTime, entry.DIR_WrtDate, &hours, &minutes, &seconds, &day, &month, &year);

            printf("| %u \t\t | %u/%u/%u \t | %u:%u:%u \t | %c%c%c%c%c%c \t | %u\t\t | %.11s \t|\n",
                   entry.DIR_FstClusLO,
                   day, month, year,
                   hours, minutes, seconds,
                   (entry.DIR_Attr & 0x01) ? 'R' : '-',
                   (entry.DIR_Attr & 0x02) ? 'H' : '-',
                   (entry.DIR_Attr & 0x04) ? 'S' : '-',
                   (entry.DIR_Attr & 0x08) ? 'V' : '-',
                   (entry.DIR_Attr & 0x10) ? 'D' : '-',
                   (entry.DIR_Attr & 0x20) ? 'A' : '-',
                   entry.DIR_FileSize,
                   entry.DIR_Name);
        }
    }
    printf("-------------------------------------------------------------------------------------------------\n");
    close(fdisk);
}

/*/////////////////////////////////////////////////////////////
                        TASK 5
/////////////////////////////////////////////////////////////*/

size_t clusterOffsetCalculation(BootSector bootSector, uint16_t *fat, uint16_t cluster) 
{
    //sectors per cluster * bytes per sector
    size_t clusterSize = bootSector.BPB_SecPerClus * bootSector.BPB_BytsPerSec;
    //calculate offset to the beginning of the data area (after reserved sectors and root directory)
    size_t offset = (bootSector.BPB_RsvdSecCnt + bootSector.BPB_NumFATs * bootSector.BPB_FATSz16) * bootSector.BPB_BytsPerSec + bootSector.BPB_RootEntCnt * sizeof(DirectoryEntry);

    //calculate offset for current cluster by using the correct clusterSize
    //"- 2" is a common adjustment for fat16.img 
    //offset is calculated as if they are 0-based.
    offset += (cluster - 2) * clusterSize;

    // Move to the next cluster in the chain
    cluster = fat[cluster];
    return offset;
}


//seek to a position in the file
off_t seekFile(File *file, off_t offset, int whence) 
{

    // '->' to access members of file structure through a pointer
    //wence so that can use SEEK_SET, SEEK_CUR, SEEK_END (flexibility)
    off_t newPos = lseek(file->fdesc, offset, whence);
    //error handling
    if (newPos == -1) 
    {
        perror("Error seeking in file");
        return -1;
    }

    //updating current position to new position in the File structure
    file->currentPosition = newPos;

    return newPos;
}

//Function to read 
size_t readFile(File *file, void *buffer, size_t length) 
{

    //to read bytes from the file
    ssize_t reading = read(file->fdesc, buffer, length);
    //Error Handling
    if (reading == -1) 
    {
        perror("Error reading from file");
        return 0;
    }

    //access the currentPosition (member of the structure) that file is pointing to
    //to update current position in the File structure
    file->currentPosition += reading;

    //return results casted to size_t because of size_t function
    return (size_t)reading;
}


//Function to close the file
//pointer to a File structure named file
void closeFile(File *file) 
{
    //  refers to a member (fdesc) of the File structure (file descriptor)
    //if not close, can lead to resource leaks
    close(file->fdesc);
    
    //deallocate memory previously allocated by malloc
    //release memory occupied preventing memory leaks
    free(file);
}

//return a member of the struct File
//parameter: filename, bootSector, array representing FAT
File *openFile(const char *filename, BootSector bootSector, uint16_t *fat) {
    //directory entry corresponding to the file
    DirectoryEntry dirEntry;
    //int to check if file being found
    int found = 0;

    //open disk image file in read-only mode
    int fdisk = open("fat16.img", O_RDONLY);
    //error handling
    if (fdisk == -1) {
        perror("Unable to open disk file");
        return NULL;
    }

    //offset to the root directory
    off_t offset = (bootSector.BPB_RsvdSecCnt + bootSector.BPB_NumFATs * bootSector.BPB_FATSz16) * bootSector.BPB_BytsPerSec;
    //seek to offset
    if (lseek(fdisk, offset, SEEK_SET) == -1) {
        perror("Error seeking to root directory");
        close(fdisk);
        return NULL;
    }

    //Retrieves number of entries in root directory from BootSector 
    //max number of directory entries
    size_t numOfEntry = bootSector.BPB_RootEntCnt;
    //iterates through each directory entry in root directory
    for (size_t i = 0; i < numOfEntry; i++) {
        //read next directory entry from disk image (fdisk) into dirEntry structure
        //size of each directory entry
        ssize_t reading = read(fdisk, &dirEntry, sizeof(DirectoryEntry));

        //error handling
        if (reading == -1) {
            perror("Error to read directory entry");
            close(fdisk);
            return NULL;
        }

        //check if the entry is not empty, not deleted and not long file name entry
        if (dirEntry.DIR_Name[0] != 0x00 && dirEntry.DIR_Name[0] != 0xE5 && (dirEntry.DIR_Attr & 0x0F) != 0x0F) {
            //compare the directory entry name with filename
            //up to 11 characters because file names are typically 8+3 format
            if (strncmp((char *)dirEntry.DIR_Name, filename, 11) == 0) {
                // if names match, breaks the loop indicating file has been found
                found = 1;
                break;
            }
        }
    }

    //check if entry is a directory
    if (dirEntry.DIR_Attr & 0x10) {
        printf("%s is a directory and not a regular file.\n", filename);
        close(fdisk);
        return NULL; // Return NULL for directories
    }
    //error handling
    if (!found) {
        fprintf(stderr, "File cant be found: %s\n", filename);
        close(fdisk);
        return NULL;
    }


    //duplicate the file descriptor
    // creates a new file descriptor (fdesc) that refers to the same open file description
    //original file descriptor (fdisk) is used for reading the directory entries and other operations
    // Duplicating the file descriptor allows the code to maintain a separate (fdesc) that can be used independently
    int fdesc = dup(fdisk);
    if (fdesc == -1) {
        perror("Unable to duplicate file descriptor");
        close(fdisk);
        return NULL;
    }

    //allocates memory for a new File structure
    File *file = malloc(sizeof(File));
    //error handling
    if (file == NULL) {
        perror("Error allocating memory for File");
        close(fdisk);
        close(fdesc);
        return NULL;
    }

    //Initialize File Structure Fields
    file->fdesc = fdesc;//The duplicated file descriptor
    file->fileLength = dirEntry.DIR_FileSize;//file size obtained from the directory entry
    file->currentPosition = 0;//initialized to 0
    file->startCluster = dirEntry.DIR_FstClusLO; //Set the starting cluster

    //Close the directory entry file descriptor
    close(fdisk);
    return file;
}

/*/////////////////////////////////////////////////////////////
                        TASK 6
/////////////////////////////////////////////////////////////*/


void unicodeConverter(const uint8_t *unicodeString, char *asciiString) 
{
    
}

//function to print long directory entry
void printLongEntry(LongName *longEntry) 
{
    printf("Long Name: %s%s%s\n",
           longEntry->LDIR_Name3,
           longEntry->LDIR_Name2,
           longEntry->LDIR_Name1);
}

//print a short directory entry
void printShortEntry(DirectoryEntry *entry) 
{
    //similar to task 4
    uint16_t hours, minutes, seconds, day, month, year;
    convertDateTime(entry->DIR_WrtTime, entry->DIR_WrtDate, &hours, &minutes, &seconds, &day, &month, &year);

    printf("| %u \t\t | %u/%u/%u \t | %u:%u:%u \t | %c%c%c%c%c%c \t | %u\t\t | %.11s \t|\n",
           entry->DIR_FstClusLO,
           day, month, year,
           hours, minutes, seconds,
           (entry->DIR_Attr & 0x01) ? 'R' : '-',
           (entry->DIR_Attr & 0x02) ? 'H' : '-',
           (entry->DIR_Attr & 0x04) ? 'S' : '-',
           (entry->DIR_Attr & 0x08) ? 'V' : '-',
           (entry->DIR_Attr & 0x10) ? 'D' : '-',
           (entry->DIR_Attr & 0x20) ? 'A' : '-',
           entry->DIR_FileSize,
           entry->DIR_Name);
}

void printRootDirectoryLN(const char *filename, BootSector bootSector) 
{
    int fdisk = open(filename, O_RDONLY);
    //error handling
    if (fdisk == -1) 
    {
        perror("Unable to open disk file");
        return;
    }

    //offset
    off_t offset = (bootSector.BPB_RsvdSecCnt + bootSector.BPB_NumFATs * bootSector.BPB_FATSz16) * bootSector.BPB_BytsPerSec;

    if (lseek(fdisk, offset, SEEK_SET) == -1) 
    {
        perror("Error seeking to the start of the root directory");
        close(fdisk);
        return;
    }

    size_t numOfEntry = bootSector.BPB_RootEntCnt;

    //read entries 1 by 1
    for (size_t i = 0; i < numOfEntry; i++) 
    {
        //read short entry 1st
        DirectoryEntry entry;
        ssize_t reading = read(fdisk, &entry, sizeof(DirectoryEntry));

        if (reading == -1) 
        {
            perror("Error to read directory entry");
            break;
        }

        //check if the short entry is not empty or deleted or not long 
        if (entry.DIR_Name[0] != 0x00 && entry.DIR_Name[0] != 0xE5 && (entry.DIR_Attr & 0x0F) != 0x0F) 
        {
            //read long entry after the short entry
            LongName longEntry;
            ssize_t longReading = read(fdisk, &longEntry, sizeof(LongName));

            if (longReading == -1) 
            {
                perror("Error to read long directory entry");
                break;
            }

            //check if last entry in the sequence
            if (longEntry.LDIR_Ord & 0x40) 
            {
                //convert to ascci from unicode for long name
                char ASCIIname[256];  //declare an array
                ASCIIname[0] = '\0';   //null-terminate

                //concatenate three parts of the long name in reverse order
                strncat(ASCIIname, (char *)longEntry.LDIR_Name3, 4);
                strncat(ASCIIname, (char *)longEntry.LDIR_Name2, 12);
                strncat(ASCIIname, (char *)longEntry.LDIR_Name1, 10);

                printf("\nLong Name: %s", ASCIIname);
            }

            // Print the short entry
            printf("\nShort Name: %.11s", entry.DIR_Name);
        }
    }

    close(fdisk);
}




int main() 
{
    //      TASK2       //
    
    BootSector bootSector;
    readDisk("fat16.img", &bootSector);

    //      TASK3       //

    //Sectors in FAT * Bytes per Sector = total size
    size_t fatSize = bootSector.BPB_FATSz16 * bootSector.BPB_BytsPerSec;

    //allocate memory to store FAT table
    uint16_t *fat = malloc(fatSize);
    if (fat == NULL) 
    {
        perror("Error allocating memory");
        return 1;
    }

    loadFAT("fat16.img", bootSector, fat, fatSize);

    //read starting cluster from user
    int startingCluster;
    printf("What is the starting cluster?\n");
    scanf("%u",&startingCluster);
    uint16_t startCluster = startingCluster;
    
    // array to store the ordered list of clusters
    size_t clusters[100]; 
    //number of clusters
    size_t clustersNumber;

    fileClusters(fat, fatSize, startCluster, clusters, &clustersNumber);

    // Print the ordered list of clusters
    for (size_t i = 0; i < clustersNumber; i++) {
        printf("starting cluster: %u\n", (unsigned int)clusters[i]);
        //dont print next cluster if (i+1) more than the number of clusters
        if ((i+1) < clustersNumber)
        {
            printf("next cluster: %u\n", (unsigned int)clusters[i+1]);
        }
    }

    //      TASK4       //

    // Read and decode entries in the root directory
    printRootDirectory("fat16.img", bootSector);

    //      TASK5       //

    char Filename[12];  //filename is 8 characters long with 3 char extension. 1 for null terminator
    printf("Enter the filename: ");
    scanf("%s", Filename);

    //Calls openFile function to open the specified file
    File* file = openFile(Filename, bootSector, fat);

    if (file != NULL) {
        //calculate offset based on starting cluster of file
        //unsigned integer type that is commonly used to represent sizes of objects in memory
        size_t fileOffset = clusterOffsetCalculation(bootSector, fat, file->startCluster);

        //off_t - represents offset
        off_t newPos = seekFile(file, fileOffset, SEEK_SET);
        if (newPos != -1) {
            //store the data read from the file (declares an array)
            char buffer[2024];
            size_t reading = readFile(file, buffer, sizeof(buffer));
            //if  data is successfully read (>0 bytes)
            if (reading > 0) {
                //reading(type size_t) is cast to (int) to match expected argument type for the precision
                printf("%.*s", (int)reading, buffer);
            }
        }
        // Close the file
        closeFile(file);
    }

    //      TASK6       //

    
    printRootDirectoryLN("fat16.img", bootSector);

    //free the memory allocated
    free(fat);

    return 0;
}
