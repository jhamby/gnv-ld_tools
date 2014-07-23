/* elf.c

<Copyright notice>

Author:         Colin Blake, HP.

Description:
                Code to read an IA64 object (ELF) file.

		This module should be built with long (64-bit) pointers
		since much of the ELF stuff is defined that way.

		$ cc /pointer=long elf

		The code can also be built on Alpha with support only
		for IA64 object (OBJ) files (no object library support
		since IA64 LBR routines are not available on Alpha).
		However, you will need to obtain two header files from
		an IA64 system and copy them to Alpha:

		$ library /text /extract=elfdatyp /output=elfdatyp.h -
			SYS$LIBRARY:SYS$STARLET_C
		$ library /text /extract=elfdef /output=elfdef.h -
			SYS$LIBRARY:SYS$STARLET_C

		and then compile as follows:

		$ cc /pointer=long /define=CROSS /include=[] elf

Modification history:

        05      Colin Blake             July 2004
                Symbols with a binding type of STB_LOCAL should be ignored.

        04      Colin Blake             June 2004
                Make it build with/without the map/unmap routines being
                defined in lbr$routines (appeared around 8.2 FT time).

	03	Colin Blake		March 2004
		Added support for building an Alpha image which
		will process IA64 object files (no object
		library support since IA64 LBR routines are not
		available on Alpha).

        02      Colin Blake		December 2003
		Add check for section index, otherwise we end up with all
		the external symbols too. Also exclude ELF$TFRADR.

        01      Colin Blake		July 2003
                Original work. Mostly code taken from the OpenVMS
		ANAL/OBJECT utility.
*/

/* These compiler versions have bugs where they incorrectly emit
   a cc-w-maylosedata3 warning.
   Older compilers do not have this message and will warn if you
   try to supress the message.
 */
#ifdef __ia64
#if __DECC_VER == 70390020
#pragma message disable maylosedata3
#endif
#endif
#ifdef __alpha
#if __DECC_VER == 70390010
#pragma message disable maylosedata3
#endif
#endif


/*********************
 * Include files:    *
 *********************/

/* Lots of debugging info gets printed out if you define DPRINT */
#if 0
#define DPRINT printf
#else
#define DPRINT(...)
#endif

#include "elf.h"	/* contains most other #include statements */
#include stsdef		/* status codes */

#include fcntl		/* for open, etc */
#include unistd		/* for close, etc */
#include types		/* for mmap */
#include mman		/* for mmap */
#include errno		/* for errno */
#include lbr$routines	/* for lbr$ routines */
#include lbrdef		/* for lbr$ definitions */
#include <sys/stat.h>	/* for stat */


#if !defined(CROSS)
/* These are not in Itanium lbr$routines.h until field test */

#ifndef lbr$map_module

#define  lbr$map_module    LBR$MAP_MODULE
#define  lbr$unmap_module  LBR$UNMAP_MODULE

#pragma __nostandard               /* This file uses non-ANSI-Sndard features */
#pragma __member_alignment __save
#pragma __nomember_alignment
#ifdef __INITIAL_POINTER_SIZE  /* Defined whenever ptr size pragmas supported */
#pragma __required_pointer_size __save  /* Save the previously-defined
					/* required ptr size */
#pragma __required_pointer_size __short /* And set ptr size default to
					  32-bit pointers */
#endif

   extern unsigned int lbr$map_module (
     unsigned int * control_index,
     unsigned __int64 * return_va_64,
     unsigned __int64 * return_length_64,
     unsigned int * txtrfa
   );

   extern unsigned int lbr$unmap_module (
     unsigned int * control_index,
     unsigned int * txtrfa
   );

#pragma __member_alignment __restore
#ifdef __INITIAL_POINTER_SIZE  /* Defined whenever ptr size pragmas supported */
#pragma __required_pointer_size __restore /* Restore the previously-defined
					  /* required ptr size */
#endif
#pragma __standard

#endif
#endif

/***************************************************
 * Definitions for data known throughout this file *
 ***************************************************/

ELF64_EHDR * elfFileAddr;       // will point to file mapped into memory
unsigned int g_lib_index;

void *mmap_ptr;
int mmap_size;


/**********************************
 * LOCAL ROUTINE TABLE OF CONTENTS
 **********************************/
int analyzeLoop(void);
static void analyzeOneModule(uint32 moduleNum, uint32 moduleCount);
static uint32 countModules(void);
int resetGlobalVariables(void);
static Elf64_Xword getModuleSize(ELF64_EHDR *elfHdr);

/* Any pointers passed to code in cc.c have to be short */

#pragma __required_pointer_size __save
#pragma __required_pointer_size __short
typedef int (*LBR_ACT_RTN)(void);
#ifdef STANDALONE
void process_symbol(char *name, int is_proc) {
    if (is_proc)
	printf("PROCEDURE: %s\n",name);
    else
	printf("DATA: %s\n",name);
}
void printf_symvec(char *s) {
    printf("%s",s);
}
void sprintf_symvecHeading(char *ctrstr, char *s) {
    printf(ctrstr,s);
    printf("\n");
}
#else
extern void process_symbol(char *name, int is_proc);
extern void printf_symvec(char *s);
extern void sprintf_symvecHeading(char *ctrstr, char *s);
#endif
#pragma __required_pointer_size __restore

void *
SectionContentsAddr(Elf64_Word secNum)
{
    ELF64_SHDR *secHdr;
    Elf64_Off offset;
    void *answer;

    if (secNum >= ElfData.NumSections) {        /* should never happen */
        return NULL;
    }

    secHdr = &ElfData.SecHdrTable[secNum];

    /* check for offset too big */
    offset = secHdr->shdr$q_sh_offset;
    if (offset >= ElfData.ModuleSize) {
        return NULL;
    }

    answer = BYTEPTR(ElfHdr) + offset;

    return (answer);
}

/******************************************************************************
 * IsElfObjectModule - return FALSE if this is an image file, TRUE otherwise
 ******************************************************************************/
int IsElfObjectModule(void)
{
    DPRINT("Start of IsElfObjectModule, type is %d\n", ElfHdr->ehdr$w_e_type);
    switch (ElfHdr->ehdr$w_e_type) {
    case EHDR$K_ET_EXEC:
    case EHDR$K_ET_DYN:
	return FALSE;
	break;
    default:
	return TRUE;
	break;
    }
}

/******************************************************************************
 * VerifyElfMagic - see if this is an elf format file
 ******************************************************************************/
int
VerifyElfMagic(unsigned char *e_ident)
{
    if (e_ident[EHDR$K_EI_MAG0] != EHDR$K_ELFMAG0)
	return FALSE;
    if (e_ident[EHDR$K_EI_MAG1] != EHDR$K_ELFMAG1)
	return FALSE;
    if (e_ident[EHDR$K_EI_MAG2] != EHDR$K_ELFMAG2)
	return FALSE;
    if (e_ident[EHDR$K_EI_MAG3] != EHDR$K_ELFMAG3)
	return FALSE;

    return TRUE;
}

/******************************************************************************
 * map_elf_file - see if this is an elf format file and if so map it
 ******************************************************************************/

int
map_elf_file(char *fileName)
{
    int status;
    ELF64_EHDR *elfHdr;
    int isElfFile;

    int fd, rv;
    struct stat st_buffer;

    rv=stat(fileName, &st_buffer);
    if (rv == -1) {
	perror("stat");
	return FALSE;
    }

    ElfFileSize = st_buffer.st_size;
    DPRINT("Size of %s is %lld\n", fileName, ElfFileSize);

    fd = open(fileName, O_RDONLY, 0, "shr=get");
    if (fd == -1) {
	perror("open");
	return FALSE;
    }
    mmap_size = st_buffer.st_size;
    mmap_ptr = mmap(0, mmap_size, PROT_READ,
			MAP_FILE|MAP_VARIABLE|MAP_PRIVATE, fd, 0);
    if (mmap_ptr == MAP_FAILED) {
	perror("mmap");
        close(fd);
	return FALSE;
    }
    close(fd);

    elfFileAddr = mmap_ptr;
    ElfFileSize = mmap_size;

    DPRINT("Mapping starts at 0x%X\n",elfFileAddr);

    isElfFile = TRUE;	/* default value */

    if (ElfFileSize < sizeof(ELF64_EHDR)) {
        printf("Size check fail: %lld is not at least %d\n",
	       ElfFileSize, sizeof(ELF64_EHDR));
	isElfFile = FALSE;	/* not an elf file */
    }
    else {
	elfHdr = elfFileAddr;
	if (! VerifyElfMagic(elfHdr->ehdr$t_e_ident)) {
        printf("Magic check failed\n");
	    isElfFile = FALSE;	/* not an elf file */
        }
    }
    DPRINT("Magic check succeeded\n");

    /* Get the address */
    ElfFileStart = elfFileAddr;	/* save address of mapped file */
    ElfHdr = elfFileAddr;	/* point to first module in file */

    /* if not an elf object file, close it (ignoring failures)
       and so indicate */
    if ( (!isElfFile) || (!IsElfObjectModule()) ) {
        printf("Not an ELF object file\n");
        rv = munmap(mmap_ptr, mmap_size);
        if (rv != 0) {
            perror("munmap");
        }
	return FALSE;
    }
    DPRINT("ELF object file checks succeeded\n");

    rv=resetGlobalVariables();	/* set globals for the first module */

    DPRINT("map_elf_file returning with %d\n",rv);
    return rv;
}

/******************************************************************************
 * AlignedLen - if necessary, round up a numeric value to enforce alignment
 ******************************************************************************/

uint64 AlignedLen(uint64 align_val, uint64 len)
{

    if (align_val != 0)		/* align_val must be non-zero */
	len =  (((len - 1) / align_val) + 1) * align_val;

    return(len);
}

/******************************************************************************
 * analyzeLoop - Loop through (possibly several) object modules in a file
 ******************************************************************************/
int analyzeLoop(void)
{
    uint32 moduleCount=0;
    uint32 ctr;

    switch (ElfHdr->ehdr$w_e_type) {
    case EHDR$K_ET_REL:
	moduleCount = countModules();
	break;
    case EHDR$K_ET_VMS_LINK_STB:
    case EHDR$K_ET_VMS_DSF:
    case EHDR$K_ET_EXEC:
    case EHDR$K_ET_DYN:
	moduleCount = 1;
	break;
    }

    /*
    ** Now find the symbols
    */

    for (ctr = 1; ctr <= moduleCount; ++ctr) {
        Elf64_Word symSecNum;
        ELF64_SHDR *symSecHdr;
        uint32 symCount;
        ELF64_SYM *symTable;
        Elf64_Word strSecNum;
        unsigned char *symStrTab;
        unsigned char st_visibl;
        unsigned char st_bind;
        unsigned char st_type;
        uint64 st_value;
        ELF64_SYM *sym;
        unsigned char *symName;

        DPRINT("Module %d\n",ctr);

        /* reset the Elf Header pointer */
        if (ctr > 1) {
            ElfHdr = (ELF64_EHDR *)(BYTEPTR(ElfHdr) + ElfData.ModuleSize);
            if (!resetGlobalVariables()) { /* set globals for the next module */
                return FALSE;
	    }
        }

	/* should never happen */
	if (! VerifyElfMagic(ElfHdr->ehdr$t_e_ident)) {
	    return FALSE;
	}

        /* Symbols are in a REL module */
        if (ElfHdr->ehdr$w_e_type == EHDR$K_ET_REL) {

            /* There is only one symbol section, but allow for many in case */
            for (symSecNum = 0; symSecNum < ElfData.NumSections; ++symSecNum) {
                DPRINT("  Section Number %d, type %d\n",
		       symSecNum,
		       ElfData.SecHdrTable[symSecNum].shdr$l_sh_type);
                if (ElfData.SecHdrTable[symSecNum].shdr$l_sh_type ==
                    SHDR$K_SHT_SYMTAB) {

                    DPRINT("    Found symbol section\n");

                    /* Look for symbol table */
    if (symSecNum < ElfData.NumSections) {
        symSecHdr = &ElfData.SecHdrTable[symSecNum];
        symCount = (symSecHdr->shdr$q_sh_size) / ELF64_SYM$K_ST_SIZE;
        symTable = SectionContentsAddr(symSecNum);
        strSecNum = symSecHdr->shdr$l_sh_link;
        symStrTab = SectionContentsAddr(strSecNum);
        DPRINT("      Symbol Count: %d\n", symCount);
    } else {
        DPRINT("      No symbol table present\n");
        symCount = 0;
    }


        for (ctr = 1; ctr < symCount; ++ctr) {  /* entry 0 not symbol */
	    char sym_on_stack[512];
            sym = &symTable[ctr];
	    if (sym->symtab$w_st_shndx != 0) {
        	st_visibl = VMS_ST_VISIBILITY(sym->symtab$b_st_other);
        	st_value = (uint64)(sym->symtab$pq_st_value);
        	symName = &symStrTab[sym->symtab$l_st_name];
		/* copy symbol from p2 space */
		strcpy(sym_on_stack, (char *)symName);
		if (strcmp(sym_on_stack,"ELF$TFRADR") != 0) {
        	    st_type = ELF64_ST_TYPE(sym->symtab$b_st_info);
                    st_bind = ELF64_ST_BIND(sym->symtab$b_st_info);
                    if (st_type == SYMTAB$K_STT_FUNC) {
                        /* Ignore symbols with binding of local */
                        if (st_bind != SYMTAB$K_STB_LOCAL) {
                            process_symbol(sym_on_stack,1);
                        }
                    }
                    else {
                        if (st_type == SYMTAB$K_STT_OBJECT)
                            process_symbol(sym_on_stack,0);
                    }
                }
	    }
        }


                }
            }
        }
    }


    return TRUE;
}

/******************************************************************************
 * analyzeOneModule - routine controlling analysis of an elf format module.
 ******************************************************************************/
static void
analyzeOneModule(uint32 moduleNum, uint32 moduleCount)
{
    int secNum;

    switch (ElfHdr->ehdr$w_e_type) {
    case EHDR$K_ET_REL:
        DPRINT("Module %d of %d is of type REL\n", moduleNum, moduleCount);
	break;
    case EHDR$K_ET_EXEC:
        DPRINT("Module %d of %d is of type EXEC\n", moduleNum, moduleCount);
	break;
    case EHDR$K_ET_DYN:
        DPRINT("Module %d of %d is of type DYN\n", moduleNum, moduleCount);
	break;
    case EHDR$K_ET_VMS_LINK_STB:
        DPRINT("Module %d of %d is of type STB\n", moduleNum, moduleCount);
	break;
    case EHDR$K_ET_VMS_DSF:
        DPRINT("Module %d of %d is of type DSF\n", moduleNum, moduleCount);
	break;
    default:
        DPRINT("Module %d of %d is of type UNKNOWN(%d)\n",
              moduleNum, moduleCount, ElfHdr->ehdr$w_e_type);
	break;
    }

    if (ElfData.ModuleSize == 0) {	/* should never happen */
        printf("Error: ElfData.ModuleSize is 0\n");
	return;
    }

    DPRINT("  NumSections: %lld\n", ElfData.NumSections);
    DPRINT("  NumSegments: %d\n", ElfData.NumSegments);

    for (secNum=0; secNum<ElfData.NumSections; secNum++) {
        ELF64_SHDR *secHdr = &ElfData.SecHdrTable[secNum];
        DPRINT("  sec %d: shdr$l_sh_type=%d\n",secNum, secHdr->shdr$l_sh_type);
    }

}

/******************************************************************************
 * countModules
 ******************************************************************************/
static uint32
countModules(void)
{
    ELF64_EHDR *elfHdr = ElfHdr;
    Elf64_Xword moduleSize;
    Elf64_Sxword totalSize = ElfFileSize;
    uint32 answer = 0;

    while (totalSize > sizeof(ELF64_EHDR)) {
	    moduleSize = getModuleSize(elfHdr);
	    if (moduleSize == 0)
		return answer;	/* not an elf file */
	    moduleSize = AlignedLen(8, moduleSize);
	    totalSize -= moduleSize;
	    elfHdr = (ELF64_EHDR *)(BYTEPTR(elfHdr) + moduleSize);
	    ++answer;
    }

    return answer;
}

/******************************************************************************
 * getModuleSize - determine the size of an module within an Elf file.
 ******************************************************************************/

static Elf64_Xword
getModuleSize(ELF64_EHDR *elfHdr)
{
#pragma required_pointer_size save
#pragma required_pointer_size long
    uint8 *startAddr;
    uint8 *highAddr;
    uint8 *tmpAddr;
#pragma required_pointer_size restore
    Elf64_Xword numSections;
    Elf64_Half numSegments;
    Elf64_Xword ctr;
    ELF64_PHDR *pSegHdr;
    ELF64_PHDR *pSegHdrTable;
    ELF64_SHDR *secHdr;
    ELF64_SHDR *secHdrTable;

    if (! VerifyElfMagic(elfHdr->ehdr$t_e_ident))
	return 0;	/* not an elf module */

    startAddr = BYTEPTR(elfHdr);

    if (elfHdr->ehdr$q_e_shoff != 0) {
	secHdrTable = (ELF64_SHDR *)(BYTEPTR(elfHdr) + elfHdr->ehdr$q_e_shoff);
	numSections = elfHdr->ehdr$w_e_shnum;
	/**************************************************************
	 * If the number of sections is greater than or equal to      *
	 * SHN_LORESERVE, e_shnum holds the value zero and the actual *
	 * number of Section Header Table entries is contained in the *
	 * sh_size field of the section header at index 0.            *
	 **************************************************************/
	if (numSections == 0)
	    numSections = secHdrTable[0].shdr$q_sh_size;
    } else {
	numSections = 0;
	secHdrTable = NULL;
    }

    if (elfHdr->ehdr$q_e_phoff != 0) {
	numSegments = elfHdr->ehdr$w_e_phnum;
	pSegHdrTable = (ELF64_PHDR *)(BYTEPTR(elfHdr) + elfHdr->ehdr$q_e_phoff);
    } else {
	numSegments = 0;
	pSegHdrTable = NULL;
    }

    /* The module contains at least an elf header */
    highAddr = startAddr + ELF64_EHDR$S_VMS_OBJECT_V1;

    /* Examine the data section headers and section data */
    for (ctr = 0; ctr < numSections; ++ctr) {
	secHdr = &secHdrTable[ctr];
	tmpAddr = startAddr + secHdr->shdr$q_sh_offset;
	tmpAddr += secHdr->shdr$q_sh_size;
	if (tmpAddr > highAddr)	/* end of data */
	    highAddr = tmpAddr;
	tmpAddr = BYTEPTR(secHdr) + ELF64_SHDR$K_SH_SIZE;
	if (tmpAddr > highAddr)	/* end of section header */
	    highAddr = tmpAddr;
    }

    /* Examine the program segment headers and segment data */
    for (ctr = 0; ctr < numSegments; ++ctr) {
	pSegHdr = &pSegHdrTable[ctr];
	tmpAddr = startAddr + pSegHdr->phdr$q_p_offset;
	tmpAddr += pSegHdr->phdr$q_p_filesz;
	if (tmpAddr > highAddr)	/* end of data */
	    highAddr = tmpAddr;
	tmpAddr = BYTEPTR(pSegHdr) + ELF64_PHDR$K_SIZE;
	if (tmpAddr > highAddr)	/* end of segment header */
	    highAddr = tmpAddr;
    }

    return (highAddr - startAddr);

}

/******************************************************************************
 * resetGlobalVariables
 *
 * FUNCTIONAL DESCRIPTION:
 *	Reset the values of ElfHdr and other global variables
 *	dependent on it.
 *	This routine is, and should remain to be, called before accessing
 *	any of the values it sets.
 *	It is currently called when the file being analyzed is identified as an
 *	Elf file and also when processing each of a sequence of concatenated
 *	object modules.
 *	Note: This routine is called very early in the processing sequence.
 *	As the error-reporting mechanism is not set up at that time, this
 *	routine or routines that it calls, should report no errors.
 ******************************************************************************/

int resetGlobalVariables(void)
{
    Elf64_Xword tableSize;	/* size of section/segment tables */
    Elf64_Off dataOffset;
    Elf64_Word secNum;	/* section name string table section number */

    /* reset the other global variables for module (except ElfHdr of course) */

    bzero(&ElfData, sizeof(ElfData));	/* unused values will be zero */

    ElfData.ModuleSize = getModuleSize(ElfHdr);

    if (ElfHdr->ehdr$q_e_phoff != 0) {
	/* ensure that the program headers are within bounds */
	ElfData.NumSegments = ElfHdr->ehdr$w_e_phnum;
	tableSize = ElfData.NumSegments * ELF64_PHDR$K_SIZE;
	dataOffset = ElfHdr->ehdr$q_e_phoff;
	if (dataOffset > (ElfData.ModuleSize - tableSize))
	    return FALSE;
	ElfData.PSegHdrTable = (ELF64_PHDR *) (BYTEPTR(ElfHdr) + dataOffset);
    }

    if (ElfHdr->ehdr$q_e_shoff != 0) {
	/* Point to Section Header Table */
	dataOffset = ElfHdr->ehdr$q_e_shoff;
	ElfData.SecHdrTable =
	    (ELF64_SHDR *) (BYTEPTR(ElfHdr) + dataOffset);

	/****************************************************************
	 * Retrieve the number of sections                              *
	 *                                                              *
	 * If the number of sections is greater than or equal to        *
	 * SHN_LORESERVE, e_shnum holds the value zero and the actual   *
	 * number of Section Header Table entries is contained in the   *
	 * sh_size field of the section header at index 0.  (Otherwise, *
	 * the sh_size member of the initial entry contains zero.)      *
	 ****************************************************************/
	ElfData.NumSections = ElfHdr->ehdr$w_e_shnum;
	if (ElfData.NumSections == 0)
	    ElfData.NumSections =
		ElfData.SecHdrTable[0].shdr$q_sh_size;

	/* ensure that the data is within bounds */
	tableSize = ElfData.NumSections * ELF64_SHDR$K_SH_SIZE;
	if (dataOffset > (ElfData.ModuleSize - tableSize))
	    return FALSE;

	/****************************************************************
	 * Get section name string table information                    *
	 *                                                              *
	 * If the module has no section name string table, e_shstrndx   *
	 * holds the value SHN_UNDEF.  If the section name string table *
	 * section index is greater than or equal to SHN_LORESERVE,     *
	 * e_shstrndx has the value SHN_XINDEX and the actual index of  *
	 * the section name string table is contained in the sh_link    *
	 * field of the section header at index 0.                      *
	*****************************************************************/
	secNum = ElfHdr->ehdr$w_e_shstrndx;
	switch (secNum) {
	case SHDR$K_SHN_UNDEF:
	    ElfData.SecStr.Table = NULL;	/* no secname string table */
	    break;
	case SHDR$K_SHN_XINDEX:
	    secNum = ElfData.SecHdrTable[0].shdr$l_sh_link;
	    /* intentional fall-through to set table pointer and size */
	default:	/* secNum fits in ehdr$w_e_shstrndx */
	    ElfData.SecStr.Table = SectionContentsAddr(secNum);
	    ElfData.SecStr.Size =
		ElfData.SecHdrTable[secNum].shdr$q_sh_size;
	    ElfData.SecStr.SecNum = secNum;
	    break;
	}

    }

    return TRUE;
}

#if !defined(CROSS) /* no ELF suport in LBR routines on Alpha */
#pragma __required_pointer_size __save
#pragma __required_pointer_size __short
int olb_action_rtn_ia64(struct dsc$descriptor *d, unsigned int *rfa)
#pragma __required_pointer_size __restore
{
    ELF64_EHDR *elfHdr;
    unsigned long status, count=0, rv, aok=TRUE;
    struct dsc$descriptor rec;
    char temp[512];

    sprintf(temp, "!\n! MODULE %1.*s\n", d->dsc$w_length, d->dsc$a_pointer);
    printf_symvec(temp);

    status = lbr$find(&g_lib_index,  rfa);
    DPRINT("Status from lbr$find is %d\n", status);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

#pragma __required_pointer_size __save /* Save the previously-defined
					  required ptr size */
#pragma __required_pointer_size __short /* And set ptr size default to
					   32-bit pointers */
    status = lbr$map_module(&g_lib_index,
			    (unsigned __int64 *) &elfFileAddr,
			    &ElfFileSize,  rfa);
#pragma __required_pointer_size __restore /* Restore the previously-defined
					     required ptr size */
    DPRINT("Status from lbr$map_module is %d\n", status);
    if(!$VMS_STATUS_SUCCESS(status)) return status;
    DPRINT("  Mapping at %lld, size %lld\n", elfFileAddr, ElfFileSize);

    if (ElfFileSize < sizeof(ELF64_EHDR)) {
	printf("Size check fail: %lld is not at least %d\n",
		ElfFileSize, sizeof(ELF64_EHDR));
	aok = FALSE;
    }

    if (aok) {
	elfHdr = elfFileAddr;
	if (!VerifyElfMagic(elfHdr->ehdr$t_e_ident)) {
	    printf("Magic check failed\n");
	    aok = FALSE;
	}
    }

    if (aok) {
	ElfFileStart = elfFileAddr; /* save address of mapped file */
	ElfHdr = elfFileAddr;       /* point to first module in file */
	if (!resetGlobalVariables())/* set globals for the first module */
	    aok = FALSE;
    }


    if (aok) {
	/* Check its an object file */
	if (!IsElfObjectModule()) {
            printf("Error - not an object\n");
	    aok = FALSE;
	}
    }

    if (aok) {
	if (!analyzeLoop())
	    aok = FALSE;
    }

    status = lbr$unmap_module(&g_lib_index, rfa);
    if(!$VMS_STATUS_SUCCESS(status)) aok = FALSE;

    return aok;
}
#endif /* !defined(CROSS) */

#if !defined(CROSS)
#pragma __required_pointer_size __save
#pragma __required_pointer_size __short
unsigned long process_olb_file_ia64(char *name)
#pragma __required_pointer_size __restore
{
    unsigned int status, lib_index, lib_func, lib_type, lib_index_number;
    struct dsc$descriptor_s lib_name_d;

    lib_func = LBR$C_READ;
    lib_type = LBR$C_TYP_ELFOBJ;

    status = lbr$ini_control(&lib_index, &lib_func, &lib_type);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    /* needed by action routine */
    g_lib_index = lib_index;

    lib_name_d.dsc$b_dtype = DSC$K_DTYPE_T;
    lib_name_d.dsc$b_class = DSC$K_CLASS_S;
    lib_name_d.dsc$w_length = strlen(name);
    lib_name_d.dsc$a_pointer = &name[0];
    status = lbr$open(&lib_index, &lib_name_d, 0, 0, 0, 0, 0);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    status = lbr$set_locate(&lib_index);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    sprintf_symvecHeading("!! LIBRARY FILE %s", name);

    lib_index_number = 1;
    status = lbr$get_index(&lib_index, &lib_index_number,
		(LBR_ACT_RTN) olb_action_rtn_ia64,
		0);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    status = lbr$close(&lib_index);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    return SS$_NORMAL;
}
#endif /* !defined(CROSS) */

int process_obj_file_ia64(char *name) {
    int rv, rv2;
    uint32 nModules;

    rv = map_elf_file(name);
    if (!rv) {
        printf("Error - not an ELF file\n");
        return FALSE;
    }

    nModules = countModules();
    DPRINT("Its an object file with %d modules\n", nModules);

    rv=analyzeLoop(); /* returns TRUE or FALSE */

    DPRINT("Unmapping the file\n");
    rv2 = munmap(mmap_ptr, mmap_size);
    if (rv2 != 0)
        perror("munmap");

    return rv;
}

#ifdef STANDALONE
int main (int argc, char *argv[]) {
    int rv, status;

    rv = process_obj_file_ia64(argv[1]);
    DPRINT("process_obj_file_ia64 returned with %d\n", rv);

#if !defined(CROSS)
    status = process_olb_file_ia64(argv[1]);
    DPRINT("process_olb_file_ia64 returned with %d\n", status);
#endif /* !defined(CROSS) */

}
#endif
