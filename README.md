# FAT16 Filesystem Reader

A simple C program to read and interpret a FAT16 filesystem image. It can display boot sector information, FAT table, root directory entries, and read file contents, including support for long filenames (LFN).

## Features

- Read boot sector information: bytes per sector, sectors per cluster, reserved sectors, number of FATs, root directory size, FAT size
- Load FAT table and follow cluster chains
- Print root directory entries with cluster number, date and time of last write, file attributes, file size, and file name
- Open and read files from FAT16 images
- Handle long file names (LFN)

## Requirements

- GCC or any C compiler
- Linux or Unix-based OS (tested on Ubuntu)
- FAT16 disk image (e.g., fat16.img)

## Usage

1. Clone the repository:
   git clone https://github.com/acetrow/fat16-filesystem-reader.git
   cd fat16-filesystem-reader

2. Compile the program:
   gcc -o fat16-reader fat16-reader.c

3. Run the program with a FAT16 disk image:
   ./fat16-reader

4. Follow the prompts:
   - Enter the starting cluster number to see cluster chains
   - Enter a filename to read its content

## Example Output

Boot Sector Information:
Bytes per Sector: 512
Sectors per Cluster: 4
Reserved Sector Count: 4
Number of copies of FAT: 2
FAT12/FAT16: size of root DIR: 512
Sectors, may be 0, see below: 32000
Sectors in FAT (FAT12 or FAT16): 32
Sectors if BPB_TotSec16 == 0: 0
Non zero terminated string: SCC.211    FAT16   

Root Directory Entries:
Cluster | Date       | Time     | Attributes | Size  | Name
2       | 01/01/2024 | 12:00:00 | ---V--     | 1024  | FILE1.TXT

Long Filename Example:
Long Name: VeryLongFileName.txt
Short Name: VERYLO~1TXT

## License

This project is licensed under the MIT License.
