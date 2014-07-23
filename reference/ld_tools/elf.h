/* elf.h

<Copyright notice>

Author:         Colin Blake, HP.

Description:
                Code to read an IA64 object (ELF) file.


Modification history:

        02      Colin Blake
		Added support for building an Alpha image which
		will process IA64 object files (no object
		library support since IA64 LBR routines are not
		available on Alpha).

        01      Colin Blake
                Original work. Mostly code taken from the OpenVMS
                ANAL/OBJECT utility.
*/

/*********************
 * Include files:    *
 *********************/


#include fabdef
#include iodef
#include iosadef
#include rms
#include stdio
#include lib$routines
#include str$routines
#include descrip
#include stddef			// offsetof
#include stdlib
#include stdarg
#include string
#include far_pointers
#include gen64def
#include ints
#include secdef			// Symbolic flags for SYS$CRMPSC
#include signal
#include ssdef
#include starlet
#include vadef
#pragma __required_pointer_size __save
#pragma __required_pointer_size __long
#include <elfdef.h>

#if defined(CROSS)
#define LBR$C_TYP_ELFOBJ 9	/* ELF object library */
#define LBR$C_TYP_ELFSHSTB 10	/* ELF Shareable image */
#endif

#define BYTEPTR(ArG) ((uint8 *)(ArG))
uint64 ElfFileSize;		/* size for Elf file (<= ElfMemUsed) */
ELF64_EHDR *ElfFileStart;	/* start of file mapped into memory */
ELF64_EHDR *ElfHdr;		/* start of current module */
struct ELF_DATA {
    ELF64_SHDR *SecHdrTable;	/* The Section Header table */
    ELF64_PHDR *PSegHdrTable;	/* The Program Segment Header table */
    struct {			/* section name strings */
	char *Table;		/* section name string table data */
	Elf64_Xword Size;	/* section name string table data size */
	Elf64_Word SecNum;	/* section name string table section number */
    } SecStr;
    /******************************************************************
     * According to the Elf specification, the largest possible value *
     * for NumSections is a quadword!                                 *
     ******************************************************************/
    Elf64_Xword NumSections;	/* How many sections */
    Elf64_Half NumSegments;	/* How many program segments */
    struct {
	ELF64_PHDR *SegHdr;	/* Dynamic segment header */
	ELF64_DYN *Table;	/* Dynamic segment table */
	Elf64_Half SegNum;	/* Dynamic segment number */
	uint32 NumTags;		/* Entries in the dynamic segment table */
    } DynSeg;
    struct {
	uint32 Count;		/* number of true image names */
	char *Table;		/* string table of shareable image names */
	Elf64_Half SegNum;	/* Image name string table segment number */
	Elf64_Off SegOff;	/* stringtable offset in that segment */
	Elf64_Xword Size;	/* size of ImgStr.Table */
    } ImgStr;
    Elf64_Xword ModuleSize;	/* size of this module */
    /* note errors to report them at an appropriate point of processing */
    union {
	uint64 allBits;
	struct {
	    unsigned noDynSeg: 1;
	    unsigned noDynSegEnd: 1;
	    unsigned badDynTag: 1;
	    unsigned dupStrTab: 1;
	    unsigned dupStrSize: 1;
	    unsigned noStrSeg: 1;
	    unsigned strOffBig: 1;
	    unsigned needCntMismatchDyn: 1;
	    unsigned needCntTwoMany: 1;
	    unsigned missingImgStrTable: 1;
	} Bit;
    } Error;
};

struct ELF_DATA ElfData;	/* globally known data */

#pragma __required_pointer_size __restore
