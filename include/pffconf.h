#ifndef PFFCONF_H
#define PFFCONF_H

#define PFCONF_DEF 8088 /* must match PF_DEFINED in pff.h */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/
#define PF_USE_READ 1  /* Enable pf_read() */
#define PF_USE_WRITE 1 /* Enable pf_write() */
#define PF_USE_DIR 0   /* Disable pf_opendir() and pf_readdir() */
#define PF_USE_LSEEK 1 /* Enable pf_lseek() */
#define PF_USE_LCC 0   /* Disable lower case conversion */

/*---------------------------------------------------------------------------/
/ Filesystem Configurations
/---------------------------------------------------------------------------*/
#define PF_FS_FAT12 0
#define PF_FS_FAT16 1
#define PF_FS_FAT32 1

#endif
