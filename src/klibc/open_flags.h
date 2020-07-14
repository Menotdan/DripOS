#ifndef OPEN_FLAGS_H
#define OPEN_FLAGS_H

// reserve 3 bits for the access mode
#define O_ACCMODE 0x0007
#define O_EXEC 1
#define O_RDONLY 2
#define O_RDWR 3
#define O_SEARCH 4
#define O_WRONLY 5

// all remaining flags get their own bit
#define O_APPEND 0x0008
#define O_CREAT 0x0010
#define O_DIRECTORY 0x0020
#define O_EXCL 0x0040
#define O_NOCTTY 0x0080
#define O_NOFOLLOW 0x0100
#define O_TRUNC 0x0200
#define O_NONBLOCK 0x0400
#define O_DSYNC 0x0800
#define O_RSYNC 0x1000
#define O_SYNC 0x2000
#define O_CLOEXEC 0x4000

#endif