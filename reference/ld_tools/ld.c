/* ld.c

<Copyright notice>

Authors:	Ken Block
		Dave Faulkner	Compaq Computer Corp

Description:
		Convert a Compaq C Tru64 compiler command line into a
		Compaq C OpenVMS compiler command line and execute.


Modification history:
	01	Ken Block
		Original work

	02	Dave Faulkner		May-2001
		Derived work. Restructured to support larger number
		of UNIX arguments.

		See ccopt.h for primary conversion table.

		Added logic to detect duplicate options and their complements
		and supercede with the new value. This is to work around the
		two restrictions in length of DCL command.
		1. Maximum number of elements appears to be 64
		2. Maximum command line length after symbol expansion is about
		   1023 bytes.

	03	Dave Faulkner		Jun-2001
		Use popen() instead of lib$spawn().
		Consolidate all compile and link commands into a single
		command file.
		Fix handling of option -E.

	04	Dave Faulkner		Oct-2001
		Add explicit support for -lX11, -lXt, -lXm and -lPthread

		Two new environment variables:

		    GNV_LINK_DEBUG		{1, 0}
			When enabled, when compiled with /DEBUG use
			LINK /DEBUG.

		    GNV_CC_WARN_INFO_ALL	{1, 0}
			When enabled, set default warning to
			/WARN=INFO=ALL

	05	Dave Faulkner		7-Oct-2001
		Provide basic HELP for options -h and --help.

	06	Dave Faulkner		8-Oct-2001
		Colin Blake merged in changes for X11 and pthread
		libraries and cleaned up exception processing.

		Fixed match_option() to recognize duplicate options preceded
		by /NO.

		Sort options table using qsort() so that header ccopt.h can
		be in order more suited for maintainers and help.

		Refined help text. Now ordered by phase.

	07	Colin Blake		9-Oct-2001
		Compile with _POSIX_EXIT defined by default. Disabled if
		environment variable GNV_CC_NO_POSIX_EXIT is defined.

	08	Colin Blake		11-Oct-2001
		In -Wc and -Wl options, if first character is "," then
		convert it to "/" (POSIX uses the "," syntax).

	09	Colin Blake		11-Oct-2001
		Added -Xmu and -Xext to known library list.

	10	Colin Blake		22-Oct-2001
		Reworked -include code (to make it work!)

	11	Colin Blake		13-Nov-2001
		Changed __VMS_PCLOSE call to regular pclose, since the VMS
		version isn't widely available yet. As a result we are
		getting a POSIX status when we really expect a VMS status.
		So long as we compile with _VMS_WAIT defined, this problem
		will fix itself with later headers files and CRTLs.

	12	Colin Blake		13-Nov-2001
		Add support of -all and -none (link options).

	13	Colin Blake		14-Nov-2001
		Fix bug I introduced in edit 10 (could cause CC to accvio).

	14	Steve Pitcher		03-Jan-2002
		Convert UNIX DSF filespec to VMS.

	15	Colin Blake		16-Jul-2002
		Fix bug in GNV_OPT_DIR implementation.

	16	Colin Blake		17-Jul-2002
		GNV_LINK_LIB_TYPES can be defiend as a list of file suffixes.
		Defines file types (suffixes) and search order for libraries
		specified via the -l option. If not specified, the default
        	is "olb a exe so" (which is what it used to be).

	17	Colin Blake		17-Jul-2002
		If a library is not found, treat it as an error (only if
		GNV_LINK_MISSING_LIB_ERROR is defined).

	18	Colin Blake		17-Jul-2002
		New command line option -auto_linker_symvec, cause the
		following line to be included in the linker options file:
		SYMBOL_VECTOR=AUTO_GENERATE

	19	Colin Blake		18-Jul-2002
		New command line option -auto_symvec. Similar to
		-linker_symvec except we produce the options file if it
		doesn't already exist by scanning the OBJ and OLB files.

	20	Colin Blake		23-Jul-2002
		Only include a shared library once.

	21	Colin Blake		31-Jul-2002
		New command line option -names_as_is_short. Causes
		/NAMES=(AS_IS,SHORT) to be added to the compile command line.
		Although -Wc,NAMES=(AS_IS,SHORT) can be used to achieve the
		same result, there is a difference in the way configure and
		make handle "special characters" such a "(" and ")" when they
		are used in the definition of CFLAGS, which results in the
		inability to use -Wc,NAMES=(AS_IS,SHORT) in CFLAGS. So this
		works around that problem.

	22	Colin Blake		31-Jul-2002
		If the number of defines and undefines is too long (can cause
		DCL command buffer overflow), then write them to a
		pre-include file instead.

	23	Colin Blake		6-Aug-2002
		Send output to stderr too if its different than stdout.

	24	Colin Blake		14-Aug-2002
		Fix for nested include files problem. See comment in code.

	25	Colin Blake		20-Aug-2002
		For a compile only, if GNV_CC_WARN_INFO is defined,
		then downgrade any warning status to informational.

	26	Colin Blake		20-Aug-2002
		Add -cxx option to force C++ mode. Needed when using LD to
		just link C++ object files.

	27	Colin Blake		21-Aug-2002
		Add GNV_LINK_AUTO_SYMVEC_NODATA feature logical.

	28	Colin Blake		21-Aug-2002
		Ensure that the auto_symvec options file comes before our
		SYS$INPUT options file otherwise file stickyness can bite us.

	29	Colin Blake		21-Aug-2002
		Add GNV_LINK_AUTO_SYMVEC_NODUPS feature logical.

	30	Colin Blake		9-Sep-2002
		Fix bug in output_list where a line greater than 127 wasn't
		terminated.

	31	Colin Blake		9-Sep-2002
		If -I include directory does not include a path, then
		prefix "./".

	32	Colin Blake		1-Oct-2002
		Auto_symvec options files will now grow if new symbols
		are found. New symbols are added to the end of the auto_symvec
		options file. The GSMATCH statement is NOT changed.

	33	Colin Blake		4-Oct-2002
		Make the pre-include file name be based on the object file
		name (if specified). This makes more sense since one source
		file can be build several different ways into several
		different object files.

	34	Colin Blake		7-Oct-2002
		If many -I options are specified, then use "#pragma
		include_directory". GNV_CC_INCLUDE_LENGTH_MAX can be used
		to control/disable this feature.

	35	Steve Pitcher		21-Oct-2002
		Output a.out rather than a_out.exe, unless we're outputing
		VMS style (e.g. GNV_SUFFIX_MODE="vms").

	36	Steve Pitcher		21-Oct-2002
		Fix tempfile for case sensitive operatation.

	37	Steve Pitcher		14-Nov-2002
		The -P option turns on /PREP=filename.  That filename has
		has a "i" inserted after it!  Remove it!

	38	Steve Pitcher		12-Dec-2002
		After doing the compilations (etc.), do a post-processing
		phase to convert an MMS Dependency file into Unix filename
		syntax.

	39	Steve Pitcher		02-Jan-2003
		Add a couple more options for MMS Dependency processing:
		-MF to specify the output file
		-MM to specify NOSYSTEM_INCLUDE_FILES.

	40	Colin Blake             03-Feb-2003
		Predefined libraries should come from SYS$LIBRARY and not
		SYS$SHARE.
		02-Dec-2005 - J. Malmberg - This seems wrong, shared images
		should be SYS$SHARE and object libraries should be SYS$LIBRARY.

	41	Colin Blake             03-Feb-2003
		Fix bug introduced in #pragma include_directory implementation.
		If we were using an include file because of the number of -D
		operands, then we were also writing #pragma include_directory
		statements to the include for even if we weren't over the
		include directory threshold. This resulted in include
		directories being specified both on the command line and via
		#pragma include_directory.

	42	Colin Blake             04-Feb-2003
		Fix problem with -MF not performing the MMS postprocess task.

	43	Steve Pitcher		04-Feb-2003
		Colin pointed out that the getname () call to get the name of
		the command procedure to run, may return a unix file spec...
		which doesn't' do much good if we're going to @ it.
		Call unix_to_vms.

	44	Colin Blake             07-Feb-2003
		Edit 43 caused the delete of the temporary command file to
		fail and broke -E if DECC$FILENAME_UNIX_ONLY was defined.

	45	Colin Blake             07-Feb-2003
		Don't specity /STAND=COMMON when preprocessing with the
		C++ compiler (because its not a valid qualifier).

	46	Steve Pitcher		02-26-2003
		44 broke -E if DECC$FILENAME_UNIX_ONLY is NOT defined...
		but it should be defined by virtue of vms_crtl_init.c.
		For some reason that lib$initialize thing isn't working...
		so invoke it manually.  This has the side effect of
		correcting the case of generated files.

	47	Colin Blake             07-Apr-2003
		Treat undefined symbols at link time as a error.

	46	Colin Blake             23-Apr-2003
		Make -V work.

	47	Colin Blake             30-Apr-2003
		Ignore SYS$INPUT when translating the MMS dependancy file.

	48	Colin Blake             21-May-2003
		Use "file" syntax instead of <file> syntax for include files
		used as pre-includes. This is needed because /MMS=NOSYSTEM
		will treat an include file included via <file> as a system
		header file and process its contents.

	49	Colin Blake             21-May-2003
		Fix bugs in MMS dependancy file post-processing.
		1. It was looking for a ":" as the separator which would
		   incorrectly  match something like DISK:[DIR]FILE.OBJ,
		   which resulted in the source file names not getting
		   converted to UNIX. The correct match is " :" (space colon).
		2. The target (object) file name was not translated to UNIX.

	50	Steve Pitcher		12-Jun-2003
		Fix MMS post-processing.  Code was trying to copy whitespace.
		It checked for space twice... should have checked for tab.

	50	Colin Blake             06-Jun-2003
		Move lib_list out into utils since ar now uses it too.

	51	Colin Blake             30-Jul-2003
		IA64 support for finding symbols in object/library files.

	52	Steve Pitcher		28-Aug-2003
		Remove the kludge for LIB$INITIALIZE.  Its no longer
		needed, being resolved properly in VMS_CRTL_INIT.C.

	53	Steve Pitcher		21-Nov-2003
		For I64, the linker messages come out %ILINK, rather than
		%LINK.  Catch this when looking for linker NUDFSYMS
		messages.

	54	Martin Borgman		22-Apr-2004
		The default name for the GNU C++ compiler is g++ rather than
		cxx. Added code to recognize the name g++ as C++ compiler.

	55	Steve Pitcher		22-Sep-2004
		Remove an extraneous "lib" from the name of the CRTL
		Supplemental library.

	56	Steve Pitcher		04-Nov-2004
		For POSIX_COMPLIANT_PATHTNAMES, when about to run the .COM file
		ALWAYS run UNIX_TO_VMS on the temporary command filename.  This
		is to insure that the trailing dot (.) is appended or not,
		according to POSIX_COMPLIANT_PATHNAMES.

	57	Steve Pitcher		15-Nov-2004
		Again, for POSIX_COMPLIANT_PATHNAMES, default an executable
		image to having no dot at then end of the filename.
		Null filetype (suffix) and no dot.

	57	Steve Pitcher		04-Feb-2004
		Third try's the charm?  Fix the last two edits in the case where
		PCP is unknown... i.e. VMS systems prior to V8.3.

	58?	John Malmberg		21-Oct-2005
	     *  On options that take a value, apparently having
                whitespace is optional, Fixed the -x case.
	     *  V7.3-2 has longer command lines.
	     *  Need hack to optionally magically have module
		specific or a global /FIRST_INCLUDE file.
	     *  With out _USE_STD_STAT on, anything that works with
		dev_t is probably broken, and stuff referencing ino_t
		will not build.  This implies _LARGEFILE is on.
		Make it default.
	     *  UNIX network programs expect _SOCKADDR_LEN behavior.
		Allow it to be set, but default it off.
	     *  Encountered a configure program that absolutely would
		not continue unless /lib/cpp would run the C preprocessor.
		So now the binary produced by this program can also be
		CPP.
	     *  Samba 4 uses .ho as an object file name.
	     *  Samba 4 uses -Wl,-rpath,bin/ which means that -Wl can not
		always be VMS specific.  If "," followed by a "-", then
		treat as UNIX.  Rewrite of parser to better follow GCC rules.
	     *  Implement "-r","-i","--relocatable" to produce a
		concatenated object file.
	     *  Allow disabling the specific include of the path of the
		primary module to allow applications to have the same
	        include file names as system supplied header files.
	     *  Linker can magically find a special option file based on
		the image name if it exists.  Help documentation on this
		feature was incorrect.  Add an optional GNV$ prefix to the
		file so that rm -f conftest* does not remove it.
	     *  CXX programs assume that #define __USE_STD_IOSTREAM 1 is set.
	     *  Add GNV_CC_MAIN_POSIX_EXIT, which if defined would modify
		the C and C++ compiler command line to be /MAIN=POSIX_EXIT.
		This is not being made the default since the older C compiler
		in common use at this time does not support it.
	59   *	Fix for the searching mechanism for library when used cc -l
		option. (to make compatible with  unix environment)

        60	Philippe Vouters
		Two changes.
	     *	First change in lookup_lib where a library is not necessarily
		prefixed with "lib". This is the case with most OpenVMS object
		libraries such as TCPIP$LIBRARY:TCPIP$LIB.OLB
	     *	Next change in do_link. The link command can be:
		cc -o x $(LIBS) $(OBJS)
		The VMS Linker requires input files to be ordered from the
		one containing the most undefined symbols to the one containing
		the less undefined symbols. So reorder the files to link with
		object files coming first, next object or archive libraries and
		last shareable or shared libraries.

	61	Philippe Vouters
		According to Linux Fedora 14 ld's man -lfilename option will
		unconditionally look for after libfilename. However
		-l:filename looks for filename. In consequence, a correct
		Linux compatible way to specify a VMS object library (.olb or
		.a) such as TCPIP$LIBRARY:TCPIP$LIB.OLB is to specify
		-Ltcpip\$library -l:tcpip\$lib.olb on the link command. In
		consequence, modified the lookup_lib code so that it
		unconditionally looks after libmylib when -lmylib and looks
		after mylib.ext when -l:mylib.ext This conforms to
		(Unix ???)/Linux ld man. In the -l: format, the full filename
		is required. For the TCPIP$LIB.OLB example, the .olb extension
		is required to match  Linux ld behavior. The link order between
		{.o | .obj } and {.a | .olb } does matter. However the link
		order with { .so | .exe } with the rest does not.

	62	Philippe Vouters
		As the OpenVMS CRTL open call does not handle symlinks, called
		dereference_symlink in lookup_lib which routine fully
		dereferences a symlink if filename is a symlink or return the
		unchanged filename if filename is not a symlink.

	63	Philippe Vouters
		-input_to_ld was not correctly processed in main when
		cc -input_to_ld <file>. It generated /OPT=file. Now it
		correctly appends ",-\n`pwd`<file>/opt" after sys$input:/opt

	64	John E. Malmberg
		* Catgorize all the options based on available documenation.
		* Have help default output limited commmands.
		* Fix up help text.
		* Implement -print-multiarch needed for cPython 3.x
		* Rename to ld.c all systems have a LINK command.
		* -x c++ now implemented, -x cxx should not have been.
		* .so files need to be linked/shared.
		* -P is now mapped to /NOINLINE_DIRECTIVES.
		* -Dfoo="a b" now works.
		* cc/cxx foreign command no longer cases infinite recursion.
		* --version working correctly.
		* Missing library or shared images should not cause CPU loops.
		* If -o path/foo is specified, then path/gnv$foo_*.com is
		  where * is "cxx", "cc", or "link" is run if present
		  before the action is run.
		* Look for gnv$lib<foo> logical name over libfoo.a for "-l"
		  option unless feature is disabled.  This allows the
		  gnv$foo_link.com above to create a shared image with out
		  modifying the makefile.
		* if there is only one -lxxx file which is a shared image
		  and no object files in the list, the object sorting needs
		  to be skipped, or it will re-sort forever.
		* Start to clean up code format.
		  80 column maximum line length.
		  tabs and indent use fixed to be consistent.
		  Tabs on 8 character boundaries and explict \t in strings.
		  Indents are 4 characters.
		  Opening braces on end of line not next line,
		  } else { not \nl<indent>}\nl<indent>else {.
		  All if / else / while blocks enclosed in braces.
		  No space after open parenthesis or before close.
		  Space after commas.
		  No space between function name and open parenthesis
		  Space between if or while and open parenthesis.

	65	John E. Malmberg
		* Do not quote numeric defines.

	66	John E. Malmberg
		* Added execv_symbol test program to call this program
		  the same way Bash does for testing.
		* Do not quote definitions unless they are already
		  quoted.
		* Fix program name setting for VAX.
		* Added support for input from stdin from
		  Eric Robertson of IQware.
		* Replicate selected C feature settings to be logical
		  name as they affect the C/C++ compilers.
*/

#include <assert.h>
#include <ctype.h>
#include <descrip.h>
#include <lbr$routines.h>
#include <lbrdef.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stsdef.h>
#include <string.h>
#include <unistd.h>
#include <unixio.h>
#include <libgen.h>
#ifndef __VAX
#include <gen64def.h>
#include <ppropdef.h>
#endif
#define sys$trnlnm hide_sys$trnlnm
#define sys$crelnm hide_sys$crelnm
#include <starlet.h>
#undef sys$trnlnm
#undef sys$crelnm
#include <ssdef.h>
#include <eobjrecdef.h>
#include <egsydef.h>
#include <esdfdef.h>
#include <gsydef.h>
#include <fcntl.h>
#include <rmsdef.h>
#include <time.h>
#include <unixlib.h>
#include <lnmdef.h>

#include "utils.h"
#include "version.h"
#include "vms_eco_level"

#ifdef __CRTL_VER
#if __CRTL_VER < 80300000
#define lstat stat
#endif
#endif

#pragma member_alignment save
#pragma nomember_alignment longword
#pragma message save
#pragma message disable misalgndmem
struct itmlst_3 {
  unsigned short int buflen;
  unsigned short int itmcode;
  void *bufadr;
  unsigned short int *retlen;
};
#pragma message restore
#pragma member_alignment restore

int   SYS$TRNLNM(
	const unsigned long * attr,
	const struct dsc$descriptor_s * table_dsc,
	struct dsc$descriptor_s * name_dsc,
	const unsigned char * acmode,
	const struct itmlst_3 * item_list);
int   SYS$CRELNM(
	const unsigned long * attr,
	const struct dsc$descriptor_s * table_dsc,
	const struct dsc$descriptor_s * name_dsc,
	const unsigned char * acmode,
	const struct itmlst_3 * item_list);

#define append_last(__arglist_ptr, __str) \
    append_list(&(__arglist_ptr)->last, (__str))

typedef enum {
    X_0, X_control, X_compile, X_linker, X_profile
} phase_t;

typedef enum {
    X_bad_parse, X_noval, X_xval, X_val, X_list
} parse_t;

typedef enum {
    X_bad_rule,
    X_accept,
    X_assemblero,
    X_assume,
    X_auto_symvec,
    X_bin_prefix,
    X_check,
    X_compile_foreign,
    X_compile_type,
    X_crtlsup,
    X_cxx,
    X_debug,
    X_debug_noopt,
    X_define,
    X_first_include,
    X_help,
    X_help_opt,
    X_include_dir,
    X_input_to_ld,
    X_libdir,
    X_library,
    X_link_all,
    X_link_foreign,
    X_link_incr,
    X_link_none,
    X_linker_symvec,
    X_lookup,
    X_makelib,
    X_mms,
    X_mms_noobject,
    X_mms_file,
    X_mms_nosys,
    X_nodebug,
    X_noobject,
    X_nooptimize,
    X_nowarn,
    X_optimize,
    X_output,
    X_preprocess,
    X_proto,
    X_p_multiarch,
    X_p_searchdirs,
    X_rpath,
    X_retain,
    X_shared,
    X_show,
    X_undefine,
    X_version,
    X_verbose,
    X_warn
} rule_t;

/* Unknown and deprecated options may be removed in the future if they
   are found to conflict with documented behavior.
   Badimp behavior should not be depended on in the future.
 */
typedef enum {
    X_ignore,    /* Silently ignore this option */
    X_active,    /* This is an active option */
    X_tru64,     /* This is an active tru64 option */
    X_unknown,   /* Unknown option - no documentation found - warn */
    X_unsup,     /* Option is unsupported - warn about it */
    X_t64usup,   /* Tru64 unsupported option - warn about it. */
    X_deprec,    /* This is not a cc/ld option and will be removed in future */
    X_badimp,    /* This not implemented as expected */
    X_conflict,  /* This conflicts with cc/ld option */
} support_t;


typedef struct {
    char *   arg_str;
    char *   str;
    phase_t  phase;
    parse_t  parse;
    rule_t   rule;
    support_t support;
} ccopt_t;

#define _X_ARG( _arg_str, _phase, _parse, _rule, _str, _support) \
    { _arg_str, _str,  X_##_phase, X_##_parse, X_##_rule, X_##_support }

ccopt_t ccopt_table[] = {
#   include "ccopt.h"
    {"zzzzzzzzzz", NULL, 0, 0, 0, 0}
};


#define MAX_NAME 255
extern char *program_name;
char actual_name[MAX_NAME + 1];

int gnv_cc_debug;
int gnv_crtl_sup;
int gnv_link_debug;
int gnv_link_map;
int gnv_link_missing_lib_error;
int gnv_link_lib_over_shared;
int gnv_link_auto_symvec_nodata;
int gnv_link_auto_symvec_nodups;
int gnv_link_no_undef_error;
int gnv_cc_warn_info;
int gnv_cc_warn_info_all;
int gnv_cc_no_posix_exit;
int gnv_cc_main_posix_exit;
int gnv_cc_no_use_std_stat;
int gnv_cxx_no_use_std_iostream;
int gnv_cc_sockaddr_len;
int gnv_cc_define_length_max;
int gnv_cc_include_length_max;
int gnv_cc_no_module_first;
int gnv_cc_no_inc_primary;
char * gnv_cc_module_first_dir;
char module_first[1024];
char gnv_cc_stdin_file[1024];
int gnv_ld_object_length_max;

char *gnv_cc_set_command;
char *gnv_cxx_set_command;
char *gnv_suffix_mode;
char *gnv_opt_dir;
char *gnv_cc_qualifiers;
char *gnv_cxx_qualifiers;
char *gnv_link_qualifiers;
char *gnv_cxxlink_qualifiers;

char *default_obj_suffix;
char *default_exe_suffix;

char *lib_types[11]; /* max of 10 */
char *lib_types_defs = "olb a exe so"; /* defaults */
char *lib_types_seps = " .,/"; /* separators */

char symvecHeadingDate[256] = {'\0'};
char symvecHeading[256] = {'\0'};
char *symvecList = 0;
int symvecListSize = 0;
int symvecFromFile = 0;

char curdir[1024];

int is_cxx = 0;
int is_ld = 0;
int is_cc = 0;
int verbose = 0;

char *command_line=NULL;
int command_line_len;
extern char *program_name;
void * set_program_name(const char *);

enum {
    force_none, force_c, force_cxx
} force_src = force_none;

int debug = 0;
int debug_link = 0;
int optimize = 1;
int preprocess = 0;
int process_stdin = 0;
int version = 0;
int help = 0;
int mms_dep_file = 0;
int mms_nosys = 0;
char *mms_file = 0;

int linkx = 1;
int sharedx = 0;
int auto_symvecx = 0;
int linker_symvecx = 0;
int no_object = 0;
int objfile = 0;
char *outfile = 0;
int link_incr = 0;
char * define_file = NULL;

struct sysshr_lib {
    const char *lib;
    const char *lognam;
    const char *file;
    int seen;
} sysshr_libs[] = {
    { "X11",     "DECW$XLIBSHR",    "SYS$LIBRARY:DECW$XLIBSHR.EXE",    0 },
    { "Xt",      "DECW$XTLIBSHRR5", "SYS$LIBRARY:DECW$XTLIBSHRR5.EXE", 0 },
    { "Xm",      "DECW$XMLIBSHR12", "SYS$LIBRARY:DECW$XMLIBSHR12.EXE", 0 },
    { "Xmu",     "DECW$XMULIBSHR",  "SYS$LIBRARY:DECW$XMULIBSHR.EXE",  0 },
    { "Xext",    "DECW$XEXTLIBSHR", "SYS$LIBRARY:DECW$XEXTLIBSHR.EXE", 0 },
    { "pthread", "PTHREAD$RTL",     "SYS$LIBRARY:PTHREAD$RTL.EXE",     0 }
};

const char
    ld_token_all[] = "-all",
    ld_token_none[] = "-none";

typedef struct arglist {
    list_t list;
    list_t *last;
    char *str;
    int rule;
    int quote;
} arglist_t;

#define ARGLIST_D( _name, _str, _rule, _quote) \
    arglist_t _name = {NULL, (list_t *)&_name, _str, _rule, _quote }

ARGLIST_D( l_cfiles		, NULL		, 0, 0 );
ARGLIST_D( l_cxxfiles		, NULL		, 0, 0 );
ARGLIST_D( l_objfiles		, NULL		, 0, 0 );

ARGLIST_D( l_compile		, NULL		, 0, 0 );
ARGLIST_D( l_accept		, "/ACC"	, 0, 0 );
ARGLIST_D( l_assume		, "/ASS"	, 0, 0 );
ARGLIST_D( l_check		, "/CHE"	, 0, 0 );
ARGLIST_D( l_define		, "/DEF"	, 0, 1 );
ARGLIST_D( l_undefine		, "/UND"	, 0, 1 );
ARGLIST_D( l_optimize		, "/OPT"	, 0, 0 );
ARGLIST_D( l_proto		, "/PRO"	, 0, 0 );
ARGLIST_D( l_show		, "/SHO"	, 0, 0 );
ARGLIST_D( l_warn		, "/WAR"	, 0, 0 );
ARGLIST_D( l_extra_compile	, NULL		, 0, 0 );

ARGLIST_D( l_include_dir	, "/INC"	, 0, 0 );
ARGLIST_D( l_preinclude_files	, "/FI"		, 0, 0 );

ARGLIST_D( l_linker		, NULL		, 0, 0 );
ARGLIST_D( l_libdir 		, NULL		, 1, 0 );
ARGLIST_D( l_optfiles		, "/OPT"	, 1, 0 );
ARGLIST_D( l_extra_link		, NULL		, 0, 0 );

static char cmd_proc_name[MAX_FILENAME_BUF];
static char *cmd_proc_name_ptr;
static char prep_file_name[MAX_FILENAME_BUF];
static char object_opt_name[MAX_FILENAME_BUF];
static char *object_opt_name_ptr = NULL;

#if __CRTL_VER >= 70302000 && !defined(__VAX)
#define MAX_DCL_LINE_LENGTH 4095
#ifdef PPROP$C_TOKEN
#define DEFAULT_DEFINE_LEN_MAX 3000
#define DEFAULT_INCLUDE_LEN_MAX 3000
#define DEFAULT_OBJECT_LEN_MAX 3000
#else
#define DEFAULT_DEFINE_LEN_MAX 1000
#define DEFAULT_INCLUDE_LEN_MAX 1000
#define DEFAULT_OBJECT_LEN_MAX 1000
#endif
#else
#define MAX_DCL_LINE_LENGTH 1023
#define DEFAULT_DEFINE_LEN_MAX 200
#define DEFAULT_INCLUDE_LEN_MAX 200
#define DEFAULT_OBJECT_LEN_MAX 200
#endif
static char cmd[256];
static char buf[2048];

FILE *cmd_proc;
FILE *object_opt;


static char buffer[EOBJ$C_MAXRECSIZ];

typedef struct eobjrecdef objdef_t;
typedef struct esdfdef   gsdef_t;
objdef_t*   objdef_p;
gsdef_t*    gsdef_p;
unsigned long g_lib_index;
FILE *f_auto_symvec = NULL;


/*
** Steve, for now only reference the ia64 routines if we're building
** on/for IA64. If you ever allow cross-linking, then you'll have to
** always reference them.
*/
#ifdef __ia64
extern int process_obj_file_ia64(char *name);
extern unsigned long process_olb_file_ia64(char *name);
extern int olb_action_rtn_ia64(struct dsc$descriptor *d, unsigned int *rfa);
#endif

/* Take all the fun out of simply looking up a logical name */
static int sys_trnlnm
   (const char * logname,
    char * value,
    int value_len)
{
    const $DESCRIPTOR(table_dsc, "LNM$FILE_DEV");
    const unsigned long attr = LNM$M_CASE_BLIND;
    struct dsc$descriptor_s name_dsc;
    int status;
    unsigned short result;
    struct itmlst_3 itlst[2];

    itlst[0].buflen = value_len;
    itlst[0].itmcode = LNM$_STRING;
    itlst[0].bufadr = value;
    itlst[0].retlen = &result;

    itlst[1].buflen = 0;
    itlst[1].itmcode = 0;

    name_dsc.dsc$w_length = strlen(logname);
    name_dsc.dsc$a_pointer = (char *)logname;
    name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
    name_dsc.dsc$b_class = DSC$K_CLASS_S;

    status = SYS$TRNLNM(&attr, &table_dsc, &name_dsc, 0, itlst);

    if ($VMS_STATUS_SUCCESS(status)) {

	 /* Null terminate and return the string */
	/*--------------------------------------*/
	value[result] = '\0';
    }

    return status;
}

/* How to simply create a logical name */
static int sys_crelnm
   (const char * logname,
    const char * value)
{
    int ret_val;
    const char * proc_table = "LNM$PROCESS_TABLE";
    struct dsc$descriptor_s proc_table_dsc;
    struct dsc$descriptor_s logname_dsc;
    struct itmlst_3 item_list[2];

    proc_table_dsc.dsc$a_pointer = (char *) proc_table;
    proc_table_dsc.dsc$w_length = strlen(proc_table);
    proc_table_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
    proc_table_dsc.dsc$b_class = DSC$K_CLASS_S;

    logname_dsc.dsc$a_pointer = (char *) logname;
    logname_dsc.dsc$w_length = strlen(logname);
    logname_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
    logname_dsc.dsc$b_class = DSC$K_CLASS_S;

    item_list[0].buflen = strlen(value);
    item_list[0].itmcode = LNM$_STRING;
    item_list[0].bufadr = (char *)value;
    item_list[0].retlen = NULL;

    item_list[1].buflen = 0;
    item_list[1].itmcode = 0;

    ret_val = SYS$CRELNM(NULL, &proc_table_dsc, &logname_dsc, NULL, item_list);

    return ret_val;
}

/*
 * generate_stdin_file Generates a file based on the stdin stream. It copies
 * the contents of stdin to a file with a specified file name. This function
 * is used expressly for the purpose of allowing a user to specify a
 * standalone hyphen in the cc command in order to compile stdin.
 */
 void generate_stdin_file(char *filename)
{
    FILE *outfile;
    char linebuffer[32678];

    remove(filename);
    outfile = fopen(filename, "w");

    fgets(linebuffer, sizeof(linebuffer), stdin);
    while (!feof(stdin) ) {
        fputs(linebuffer, outfile);
        fgets(linebuffer, sizeof(linebuffer), stdin);
    }
    fclose(outfile);
}


/*
** Support for making sure that we don't have duplicates in the symvec list
**
** The format of the list is: /name1/name2/name3.../
*/

/*
** Check to see if a name exists in the list.
** If it does, returns its offset from the beginning of the list.
*/

int symvecExists(char *name) {
    char ss[128], *p;
    if (symvecListSize) {
	strcpy(ss,"/");
	strcat(ss,name);
	strcat(ss,"/");
	p=strstr(symvecList,ss);
	if (p != NULL) {
	    return (int) p + 1 - (int) symvecList;
	} else {
	    return 0;
	}
    }
    else
	return 0;
}

/* Add a name to the list */

void symvecAdd(char *name) {
    if (symvecListSize == 0) {
	symvecListSize = 1000;
	symvecList = malloc(symvecListSize);
	strcpy(symvecList,"/");
    } else {
	if ((strlen(symvecList)+strlen(name)+2) >= symvecListSize) {
	    symvecListSize += symvecListSize;
	    symvecList = realloc(symvecList,symvecListSize);
	}
    }
    strcat(symvecList,name);
    strcat(symvecList,"/");
}

/* Read/parse an existing symbol options file */

void symvecRead(char *fname) {
    FILE *f;
    char read_buf[1024];
    f = fopen(fname, "r");
    if (!f) {
	errmsg("Unable to read auto_symvec file %s",fname);
	exit(EXIT_FAILURE);
    }
    fgets(read_buf, sizeof(read_buf), f);
    while (!feof(f) ) {
	char *p, *p2;
	p = strstr(read_buf, "SYMBOL_VECTOR=(");
	if (p != NULL) {
	    p = p+15;
	    p2 = strchr(p,'=');
	    if (p2 != NULL) {
		*p2 = '\0';
		symvecAdd(p);
	    }
	}
	fgets(read_buf, sizeof(read_buf), f);
    }
    fclose(f);
    if (symvecList) {
	symvecFromFile = strlen(symvecList);
    }
    return;
}

/*
** Action routine for the -all support.
**
** For each module, lib_action_rrn is called to output a line to
** linker options file of the format:
**
**    olb_file_name/include=module_name
*/

char *lib_action_rtn_name;

unsigned long lib_action_rtn(struct dsc$descriptor *d, long rfa[2])
{
    fprintf(cmd_proc,"%s/INCLUDE=%1.*s\n", lib_action_rtn_name,
	    d->dsc$w_length, d->dsc$a_pointer);
    return 1;
}

/*
** Determine if stdout and stderr are the same.
*/
int stdout_is_stderr() {
    char out[MAX_FILENAME_BUF], err[MAX_FILENAME_BUF];

    fgetname(stdout,out);
    fgetname(stderr,err);

    if ((strncasecmp(out,"/SYS$OUTPUT/",12) == 0) &&
	(strncasecmp(err,"/SYS$ERROR/",11) == 0)) {
	    return TRUE;
    }

    if ((strncasecmp(out,"SYS$OUTPUT:",11) == 0) &&
	(strncasecmp(err,"SYS$ERROR:",10) == 0)) {
	    return TRUE;
    }

    if (strcasecmp(out,err) == 0) {
	return TRUE;
    } else {
	return FALSE;
    }
}

/*
** Support for -auto_symvec - automatically generate a linker
** options file to promote all global symbols to universal.
**
** printf_symvec - write to the symbol vector options file
**
** sprintf_symvecHeading - define heading line for symbol vector options file
**
** process_symbol - add symbol to opt file, if not already present, take
**     care of headings ,etc.
**
** process_gsd_rec - find all the symbols in a single global
**     symbol record. Called by both OBJ and OLB processing.
**
** process_obj_file - reads an object (OBJ) file and finds all
**     the global symbol records.
**
** olb_action_rtn - called by LBR for each object module found.
**
** process_olb_file - reads an object library (OLB) file and
**     locates all the object modules.
**
** For IA64 support the following routines are in ELF.C:
**   process_obj_file_ia64
**   process_olb_file_ia64
**   olb_action_rtn_ia64
** These all provide the same functionality as their non-IA64 routines.
*/

void printf_symvec(char *s)
{
    fprintf(f_auto_symvec, "%s", s);
}

void sprintf_symvecHeading(char *ctrstr, char *s)
{
    sprintf(symvecHeading, ctrstr, s);
}

void process_symbol(char *name, int is_proc) {

    /*
    ** name - symbol to be processed
    ** is_proc - if true, symbol is a procedure, otherwise data
    */

    int add_it = TRUE;
    int pos;
    char temp_string[512];

    /* Do we alraedy have this symbol? */
    pos = symvecExists(name);
    if (pos) {
	/* Yes. If it came from an existing opt file, or if we */
	/* were told to ignore duplicates, then don't add it. */
	if ((pos < symvecFromFile) || gnv_link_auto_symvec_nodups)
	    add_it = FALSE;
    }

    /* If we're going to use it, add it to our internal list */
    if (add_it) {
	symvecAdd(name);
    }

    /* Detemine if its PROCEDURE or DATA symbol */
    if (add_it) {
	if (is_proc) {
	    sprintf(temp_string, "%s=PROCEDURE", name);
	} else {
	    if (gnv_link_auto_symvec_nodata) {
		add_it = FALSE;
	    } else {
		sprintf(temp_string, "%s=DATA", name);
	    }
	}
    }

    /* Ignore any symbols starting "$$" */
    if (add_it) {
	if ((temp_string[0] == '$') && (temp_string[1] == '$')) {
	    add_it = FALSE;
	}
    }
    /* If all is still well, write this symbol to the opt file */
    /* If we have a heading to print, do that too. */
    if (add_it) {
	if (symvecHeadingDate[0] != '\0') {
	    fprintf(f_auto_symvec, "%s",symvecHeadingDate);
	    symvecHeadingDate[0] = '\0';
	}
	if (symvecHeading[0] != '\0') {
	    fprintf(f_auto_symvec, "%s",symvecHeading);
	    symvecHeading[0] = '\0';
	}
	fprintf(f_auto_symvec, "SYMBOL_VECTOR=(%s)\n",temp_string);
    }
}

int process_gsd_rec(char *buffer, int bytcnt, int count) {

    int buffpos=8;
    char temp_string[512];

    if (bytcnt > EOBJ$C_MAXRECSIZ) {
	goto error;
    }
    if (bytcnt == 0) {
	return 0; /* all done */
    }
    objdef_p = (objdef_t*) &buffer[0];

    if (count == 0 && objdef_p->eobj$w_rectyp != EOBJ$C_EMH) {
	goto error;	/* first record type should be HEADER*/
    }
    if ( objdef_p->eobj$w_rectyp == EOBJ$C_EGSD) {
	while(buffpos < bytcnt) {
	    gsdef_p = (gsdef_t*) &buffer[buffpos];
	    if (gsdef_p->esdf$w_flags & GSY$M_DEF) {
		strncpy(temp_string,
			gsdef_p->esdf$t_name,
			gsdef_p->esdf$b_namlng);
		temp_string[gsdef_p->esdf$b_namlng] = '\0';
		process_symbol(temp_string, /* name of symbol */
			       gsdef_p->esdf$w_flags & EGSY$M_NORM); /* proc */
	    }
	    buffpos += gsdef_p->esdf$w_size;
	}
    }
    return(0);

error:
    errmsg("Object Library file has an invalid format");
    return(2);

}

unsigned long process_obj_file(char *name) {

    int in_file, bytcnt, count=0;
    char buffer[EOBJ$C_MAXRECSIZ];

    in_file = open(name,O_RDONLY, 0, "rfm=var");

    if (in_file == -1) {
	errmsg("Error opening object file: %s", name);
	return 2;
    }

    sprintf(symvecHeading, "!\n! OBJECT FILE %s\n", name);

    while (1) {
	bytcnt = read(in_file,buffer,EOBJ$C_MAXRECSIZ);
	if (bytcnt == 0)
	    break; /* all done */
	if (bytcnt == -1) {
	    printf("** OBJect file has an invalid format\n");
	    return 2;
	}

	process_gsd_rec(&buffer[0], bytcnt, count);
	count++;
    }

    close(in_file);
    return 1;
}

unsigned long olb_action_rtn(struct dsc$descriptor *d, long rfa[2])
{
    unsigned long status, count=0;;
    struct dsc$descriptor rec;

    fprintf(f_auto_symvec,
	"!\n! MODULE %1.*s\n", d->dsc$w_length, d->dsc$a_pointer);

    status = lbr$find(&g_lib_index, &rfa[0]);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    while ($VMS_STATUS_SUCCESS(status = lbr$get_record(&g_lib_index,0,&rec))) {
	process_gsd_rec(rec.dsc$a_pointer, rec.dsc$w_length, count);
	count++;
    }

    if (status != RMS$_EOF)
	return(status);

    return 1;
}

unsigned long process_olb_file(char *name)
{
    unsigned long status, lib_index, lib_func, lib_type, lib_index_number;
    struct dsc$descriptor lib_name_d;

    lib_func = LBR$C_READ;
#ifdef __ia64
    lib_type = LBR$C_TYP_ELFOBJ;
#else
#ifdef __alpha
    lib_type = LBR$C_TYP_EOBJ;
#else
#ifdef __vax
    lib_type = LBR$C_TYP_OBJ;
#else
#error Unknown architecture
    lib_type =-1;
#endif
#endif
#endif
    status = lbr$ini_control(&lib_index, &lib_func, &lib_type);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    /* needed by action routine */
    g_lib_index = lib_index;

    sprintf(symvecHeading, "!\n! LIBRARY FILE %s\n", name);

    lib_name_d.dsc$w_length = strlen(name);
    lib_name_d.dsc$b_dtype = DSC$K_DTYPE_T;
    lib_name_d.dsc$b_class = DSC$K_CLASS_S;
    lib_name_d.dsc$a_pointer = name;
    status = lbr$open(&lib_index, &lib_name_d, 0, 0, 0, 0, 0);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    status = lbr$set_locate(&lib_index);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    lib_index_number = 1;
/*
** Steve, if you ever support cross-linking in GNV then the compile
** time test below to determine which version of the action routine to
** call will have to become a run time test.
*/
#ifdef __ia64
    status = lbr$get_index(&lib_index, &lib_index_number,
                           olb_action_rtn_ia64, 0);
#else
    status = lbr$get_index(&lib_index, &lib_index_number, olb_action_rtn, 0);
#endif
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    status = lbr$close(&lib_index);
    if(!$VMS_STATUS_SUCCESS(status)) return status;

    return 1;
}


/*
 * Output help text derived from the option table.
 *
 */

void output_help( void)
{
    int i;

    printf("    %s  [option...] file... [option...]\n", program_name);
    puts("\n\
    if -o <path>/<file> is specified, then for the action of \"cc\",\n\
    \"cxx\", or \"link\" file <path>/gnv$<file>_<action>.com will be\n\
    run if it exists");
    /*
    ** List environment variables that can be set
    */

    if (is_cc || verbose) {
        puts("\n\
    GNV_CC_DEBUG		{1, 0}\n\
	When enabled, DCL command files are echoed and temporary files\n\
	are not deleted.\n\
    GNV_CC_MAIN_POSIX_EXIT	{1, 0}\n\
	When enabled, adds /MAIN=POSIX_EXIT.  This reqires version 7.1 or\n\
	of the C/C++ compilers.\n\
    GNV_CC_NO_POSIX_EXIT	{1, 0}\n\
	When enabled, prevents /DEFINE=_POSIX_EXIT.\n\
    GNV_CC_NO_USE_STD_STAT	{1, 0}\n\
	When enabled, prevents /DEFINE=_USE_STD_STAT on OpenVMS ALPHA\n\
	and Integrity for versions 8.2 and later.\n\
    GNV_CC_QUALIFERS		command-qualifiers\n\
	When set, appends the CC qualifers to the CC command line.\n\
    GNV_CC_SET_COMMAND		dcl-command\n\
	When set, this DCL command is executed before the CC command.\n\
    GNV_CC_WARN_INFO		{1, 0}\n\
	When enabled, a compiler warning is downgraded to info severity\n\
    GNV_CC_WARN_INFO_ALL	{1, 0}\n\
	When enabled, sets the default message level to /WARN=INFO=ALL.\n\
    GNV_CC_SOCKADDR_LEN	{1, 0}\n\
	When enabled, causes /DEFINE=_SOCKADDR_LEN.\n\
    GNV_CC_NO_INC_PRIMARY	{1, 0}\n\
	When set the path that the primary source file is present on\n\
	will not be added as an additional include path.  This is to\n\
	prevent \"foo.h\" accidentally replacing <foo.h>.");
    }
    if (is_cxx || verbose) {
        puts("\n\
    GNV_CXX_NO_USE_STD_IOSTREAM	{1, 0}\n\
	When enabled, prevents /DEFINE=__USE_STD_IOSTREAM\n\
    GNV_CXX_QUALIFERS		command-qualifiers\n\
	When set, appends the CXX qualifers to the CXX command line.\n\
    GNV_CXX_SET_COMMAND		dcl-command\n\
	When set, this DCL command is executed before the CXX command.");
    }
    if (is_cxx || is_cc || verbose) {
        puts("\
    GNV_CC_DEFINE_LENGTH_MAX	number\n\
	Controls how long the /DEFINE and /UNDEFINE lists can be before a\n\
	pre-include file is used instead. Default is 1000 characters for\n\
	OpenVMS Alpha and Integrity for versions 7.3-2 and later and\n\
	200 characters otherwise.\n\
    GNV_CC_INCLUDE_LENGTH_MAX	number\n\
	Controls how long the /INCLUDE list can be before a\n\
	pre-include file is used instead. Default is 1000 characters for\n\
	OpenVMS Alpha and Integrity for versions 7.3-2 and later and\n\
	200 characters otherwise.\n\
    GNV_CC_MODULE_FIRST_DIR		option-directory\n\
	When set, this specifies the directory in UNIX format that a\n\
	first include file with the same name as the source module but\n\
	prefixed with \"gnv$\" and \"_first\" appended to be used instead\n\
	of the directory the module is in.\n\
    GNV_CC_NO_MODULE_FIRST	{1, 0}\n\
	When set, a file with the source module name with \"gnv$\" and\n\
	\"_first\" appended will not be looked for in the same directory,\n\
	otherwise the first one found for a list of files will be used as a\n\
	/FIRST_INCLUDE file for all files to be compiled.\n\
	If no files are found, for .c and .cxx files only, if the the\n\
	GNV_OPT_DIR feature is set, a gnv$first_include.h file will be\n\
	used as a /FIRST_INCLUDE file if it exists.");
    }
    if (is_ld || verbose) {
        puts("\n\
    GNV_CRTL_SUP		{1, 0}\n\
	When enabled, uses the Supplementary C RTL library.\n\
    GNV_CXXLINK_QUALIFERS	command-qualifiers\n\
	When set, appends the CXXLINK qualifers to the CXXLINK command line.\n\
    GNV_LD_OBJECT_LENGTH_MAX	number\n\
	Controls how long the ojbject file list can be before a\n\
	option file is used. Default is 1000 characters for\n\
	OpenVMS Alpha and Integrity for versions 7.3-2 and later and\n\
	200 characters otherwise.\n\
    GNV_LINK_AUTO_SYMVEC_NODATA	{1, 0}\n\
	When enabled, prevents -auto_symvec from making any DATA symbols\n\
	universal. The default is to promote both DATA and PROCEDURE symbols.\n\
    GNV_LINK_AUTO_SYMVEC_NODUPS	{1, 0}\n\
	When enabled, prevents -auto_symvec from creating duplicate symbols\n\
	in the options file. The default will cause linker errors if\n\
	duplicate symbols exist.\n\
    GNV_LINK_DEBUG		{1, 0}\n\
	When enabled, uses LINK/DEBUG when compiling /DEBUG. This\n\
	sets the initial breakpoint. If not enabled, a separate\n\
	debug symbol file is created which can be used with RUN/DEBUG.\n\
	The debug symbol file is not created on VAX.\n\
    GNV_LINK_LIB_OVER_SHARED	{1, 0}\n\
	When enabled, -l<foo> will try to find libraries the same way LD\n\
	does on Linux.  When disabled, -l<foo> will use a logical name\n\
	GNV$LIB<foo>, with periods with underscores and pluses replaced\n\
	with x characters.\n\
    GNV_LINK_LIB_TYPES		list of file suffixes\n\
	Defines file types (suffixes) and search order for libraries\n\
	specified via the -l option. If not specified, the default\n\
	is \"olb a exe so\". Use space, comma, or slash as separators.\n\
    GNV_LINK_MAP		{1, 0}\n\
	When enabled, uses LINK/MAP=exefile.MAP.  The exefile is\n\
	based on the image file being created.\n\
    GNV_LINK_MAP		{1, 0}\n\
	When enabled, uses LINK/MAP=exefile.MAP.  The exefile is\n\
	based on the image file being created.\n\
    GNV_LINK_MISSING_LIB_ERROR	{1, 0}\n\
	When enabled, cause a missing library file to be treated as\n\
	an error. If not enabled, a missing library file is a warning.\n\
    GNV_LINK_NO_UNDEF_ERROR	{1, 0}\n\
	When enabled, prevents undefined symbols at link time from being\n\
	an error. The default is undefined symbols cause an error status.\n\
    GNV_LINK_QUALIFERS		command-qualifiers\n\
	When set, appends the LINK qualifers to the LINK command line.\n\
    GNV_SUFFIX_MODE		{vms, unix}\n\
	When set to \"vms\", object files have type .OBJ and executable\n\
	files have type .EXE.");
   }
   puts("\
    GNV_OPT_DIR			option-directory\n\
	When set, this specifies the directory in UNIX format that a\n\
	linker option file with the same name as the executable but\n\
	optionally prefixed with gnv$ will be used if it exists.");

    /*
    ** Output one line for each supported argument in the option table
    */
    for (i = 0; i < DIM(ccopt_table); i++) {
	ccopt_t *cc_opt = &ccopt_table[i];
	char *phase;
	char *text;
	char *support;
	char *val;
	char txt[80];
	int txt_len;
	char value[80];
	char line[256];
	int len, fill;
	arglist_t *list_ptr;
	int val_with_text;

        if (cc_opt->phase == X_0) {
            continue;
        }

        /* Hide things if not verbose */
        if (!verbose) {
            if (is_ld && (cc_opt->phase == X_compile)) {
                continue;
            }
            if (!is_ld && (cc_opt->phase == X_linker)) {
                continue;
            }

            if (cc_opt->support == X_deprec) {
                continue;
            }
            if (cc_opt->support == X_badimp) {
                continue;
            }
            if (cc_opt->support == X_unknown) {
                continue;
            }
        }

	switch (cc_opt->phase) {
	default:
	case X_0:	phase = ""; 		break;
	case X_control:	phase = "..";		break;
	case X_compile:	phase = "cc";		break;
	case X_linker:	phase = "ld";		break;
	case X_profile:	phase = "prof";		break;
	}

	text = cc_opt->str ? cc_opt->str : "";
	val = "value";
	list_ptr = NULL;
	val_with_text = TRUE;

	switch (cc_opt->rule) {
	case X_bad_rule:
            break;
	case X_accept:
	    list_ptr = &l_accept;
	    break;
        case X_assemblero:
            text = "** Output intermediate assember only.";
            break;
	case X_assume:
	    list_ptr = &l_assume;
	    break;
	case X_auto_symvec:
#ifndef __VAX
	    text = "** LD is to create/use automated symbol vector table";
#else
	    text = "** Automated symbol vector not implemented on VAX.";
#endif
	    break;
        case X_bin_prefix:
            text = "** Specifies a prefix for the compiler files.";
	    val_with_text = FALSE;
            break;
	case X_check:
	    list_ptr = &l_check;
	    break;
	case X_compile_foreign:
	    val = "/CC_Opts";
	    text = "** Append CC command options to command line";
	    val_with_text = FALSE;
	    break;
	case X_compile_type:
	    val = "{c, c++}";
	    text = "** Compilation type c, c++. Not implemented.";
	    val_with_text = FALSE;
	    break;
	case X_crtlsup:
	    text = "** Enable Supplementary C RTL library";
	    break;
	case X_cxx:
	    text = "** Force C++ mode";
	    break;
	case X_debug:
	    break;
	case X_debug_noopt:
	    break;
	case X_define:
	    val = "def[=value]";
	    list_ptr = &l_define;
	    break;
	case X_first_include:
	    val = "include_file";
	    list_ptr = &l_preinclude_files;
	    break;
	case X_help:
	    text = "** Prints usage";
	    break;
        case X_help_opt:
            text = "** Display current option settings.  Not implemented.";
            break;
	case X_include_dir:
	    val = "include_dir";
	    list_ptr = &l_include_dir;
	    break;
	case X_input_to_ld:
	    val = "Linker_opt_file";
            list_ptr = &l_optfiles;
	    text = "** Linker options file";
//	    val_with_text = FALSE;
	    break;
	case X_libdir:
	    list_ptr = &l_libdir;
	    val = "Link_library_directory";
	    text = "** Directory containing link libraries";
	    val_with_text = FALSE;
	    break;
	case X_library:
	    val = "Link_library";
	    text = "** Library to be searched in link phase";
	    val_with_text = FALSE;
	    break;
	case X_link_all:
	    text = "** Force load entire contents of libraries";
	    break;
	case X_link_foreign:
	    val = "/LINK_Opts";
	    text = "** Append Linker command options to command line";
	    val_with_text = FALSE;
	    break;
	case X_link_incr:
	    text = "** Incremental Link.  Produces concatenated object file.";
	    break;
	case X_link_none:
	    text = "** Cancel previous -all option";
	    break;
	case X_linker_symvec:
	    text = "** Linker is to produce automated symbol vector table";
	    break;
	case X_lookup:
	    val_with_text = FALSE;
	    break;
	case X_makelib:
	    text = "** Create object file and suppress link";
	    break;
	case X_mms_noobject:
	    text =
            "** Generate MMS dependency file.  Don't generate object file. ";
	    break;
	case X_mms:
	    text =
            "** Generate MMS dependency file.  Do generate object file. ";
	    break;
	case X_mms_file:
	    val = "filename";
	    text = "** Generate MMS dependency file with specified filename.";
	    val_with_text = FALSE;
	    break;
	case X_mms_nosys:
	    text =
              "** Generate MMS dependency file, excluding system dependencies.";
	    break;
	case X_nodebug:
	    break;
	case X_noobject:
	    break;
	case X_nooptimize:
	    break;
	case X_nowarn:
	    break;
	case X_optimize:
	    list_ptr = &l_optimize;
	    break;
	case X_output:
	    val = "ofile";
	    text = "** Output file";
	    val_with_text = FALSE;
	    break;
	case X_preprocess:
	    break;
	case X_p_multiarch:
	    text = "** Print multiarch version tuple and exit.";
	    break;
	case X_p_searchdirs:
	    text = "** Print search directories (incomplete) and exit.";
	    break;
	case X_proto:
	    list_ptr = &l_proto;
	    break;
	case X_retain:
	    break;
	case X_rpath:
	    text = "Path to search for shared images - Not implemented.";
	    val_with_text = FALSE;
	    break;
	case X_shared:
	    break;
	case X_show:
	    list_ptr = &l_show;
	    break;
	case X_undefine:
	    val = "def";
	    list_ptr = &l_undefine;
	    break;
	case X_version:
	    break;
        case X_verbose:
            text = "** More verbose help";
            break;
	case X_warn:
	    val = "msg_list";
	    list_ptr = &l_warn;
	    break;
        default:
            text = "undocumented rule";
	}

	switch (cc_opt->parse) {
	case X_bad_parse:
                strcpy(value, "bad parse");
	case X_noval:
		val = "";
		*value = '\0';
		break;

	case X_xval:
		sprintf(value, "[%s]", val);
		break;

	case X_val:
		sprintf(value, " %s", val);
		break;
        default:
                strcpy(value, "Unknown value");
	}

	if (!val_with_text)
	    val = "";

	if (list_ptr && list_ptr->str && !list_ptr->rule) {
	    if (*text)
		sprintf(txt,"%s=(%s%s%s)",
                        list_ptr->str, text, *val ? "=" : "", val);
	    else
		sprintf(txt,"%s%s%s",
                        list_ptr->str, *val ? "=" : "", val);
	}
	else if (*text)
	    sprintf(txt, "%s%s%s", text, *val ? "=" : "", val);
	else
	    strcpy(txt, val);

        switch (cc_opt->support) {
        case X_ignore:
            /* Silently ignore this option */
            support = "Ignored";
            break;
        case X_active:
            /* This is an active option */
            support = "";
            break;
        case X_tru64:
            /* This is an active option */
            support = "Tru64";
            break;
        case X_unknown:
            /* Unknown option - no documentation found - warn */
            support = "Unknown";
            break;
        case X_unsup:
            /* Option is unsupported - warn about it */
            support = "Unsupported";
            break;
        case X_t64usup:
            /* Option is unsupported - warn about it */
            support = "Tru64 Unsupported";
            break;
        case X_deprec:
            /* This is not a known cc/ld option and may be removed in future */
            support = "Deprecated";
            break;
        case X_badimp:
            /* This not implemented as expected */
            support = "Non-std";
            break;
        case X_conflict:
            /* This conflicts with cc/ld option */
            support = "Non-std";
            break;
        default:
            support = "Undefined";
        }

	len = sprintf(line, "  %4s -%s%s", phase, cc_opt->arg_str, value);
	fill = len < 31 ? 31 - len : 0;
        txt_len = strlen(txt);
        if (txt_len < (80 - 31 - 15)) {
	    sprintf(line+len, "%*c%s", fill, ' ', txt);
        } else {
            int wrap_offset;
            wrap_offset = 10;
	    sprintf(line+len, "\n%*c%s", wrap_offset, ' ', txt);
        }
        if (support[0] != 0) {
            int line_len;
            line_len = strlen(line);
            sprintf(line+line_len, " [%s]", support);
        }
	puts(line);
    }
    if (is_ld) {
    puts("\n\
    When linking an executable, the ld program will check if an option\n\
    file exists with the same name as the executable and automatically\n\
    use that option file.  This allows you to add shared libraries.\n");
    }
}

/*
 * Compares two option strings and returns TRUE
 * if they match. Comparison is case-insensitive.
 *
 * Comparison stops when either '=' or '\0' is found.
 */
int match_option(const char *new, const char *old)
{
    const char *px[2] = { new, old};
    const char *p, *q;
    int i;


    if (*new != '/') {
	errmsg("Option not starting with '/'. '%s'", new);
	return 0;
    }

    for (i = 0; i < 2; i++) {
	if (*px[i] == '/')
	    px[i]++;

	if (!strncasecmp(px[i], "NO", 2))
	    px[i] += 2;
    }

    for (p = px[0], q = px[1]; *p && *q && *p != '=' && *q != '='; p++, q++) {
	if (toupper(*p) != toupper(*q))
	    return FALSE;
    }

    return TRUE;
}
/*
 * Add entries for each option in string arg.
 * Check if the option or its complement has
 * already been specified. If so replace its
 * entry in the list.
 *
 * No attempt is made to recover lost memory.
 */

void append_arg (arglist_t *l_arglist, const char *arg)
{
    char *newarg;
    const char *ptr;
    list_t lptr;
    int found;
    int quote = 0;

    if (!arg || !*arg)
	return;

    ptr = arg + 1;
    while (TRUE ) {
	char c = *ptr++;
	if (c == '"') {
	    quote = !quote;
	    continue;
	}
	if (!c)
	    newarg = (char *)arg;
	else if (quote || c != '/')
	    continue;
	else {
	    int len = ptr - arg - 1;
	    newarg = malloc(len + 1);
	    strncpy(newarg, arg, len);
	    newarg[len] = '\0';
	}

	arg = ptr - 1;

	/*
	 * Remove any duplicate entries from list
	 * including negative entries /NO
	 */
	found = 0;
	lptr = l_arglist->list;
	while (lptr) {
	    if (!match_option(newarg, lptr->str)) {
		lptr = lptr->next;
		continue;
	    }
	    found = 1;
	    if (gnv_cc_debug)
	        errmsg("Option %s replaces %s", newarg, lptr->str);
	    lptr->str = newarg;
	    break;
	}

	if (!found)
	    append_last(l_arglist, newarg);

	if (!c)
	    break;
    }
}


/*
 * Determine the size, in bytes, of a list.
 * Only an approximation. Doesn't take into account any quote fixing or line
 * wrapping that may actually occur in output_list.
 */
int list_size (arglist_t *arglist) {
    list_t lptr = arglist->list;
    int siz=0;
    if (lptr)
	/* include "=()", allow for no "," for the first item */
	if (arglist->str) {
	    siz += strlen(arglist->str)+3-1;
	}
    while (lptr != 0) {
	siz += strlen(lptr->str)+1;
	if (arglist->quote)
	    siz += 2;
        lptr = lptr->next;
    }
    return siz;
}

/*
 * Determine if a pre-include file is needed.  Need to check both
 * the include and define lengths against the max DCL length.
 * TODO: This check really needs to know the how long the actual command
 * is going to end up.
 */
int use_pre_include_file(void) {
    int define_length;
    int include_length;

    define_length = list_size(&l_define)+list_size(&l_undefine);
    if (define_length > gnv_cc_define_length_max) {
	return 1;
    }
    include_length = list_size(&l_include_dir);
    if (include_length > gnv_cc_include_length_max) {
	return 1;
    }

    /* This calculation needs to be more robust */
    if ((define_length + include_length) > (MAX_DCL_LINE_LENGTH - 1000)) {
	return 1;
    }
    return 0;
}


/*
 * Create an include file, write the define/undefines to it, and then
 * add the include file to the head of the pre-include list.
 */
void create_define_list(const char* file_base) {
    list_t lptr;
    char fn[255];
    FILE *f=NULL;

     /* create/open the file to hold the definitions */
    sprintf(fn, "%s_defines", file_base);
    f = fopen(fn, "w");

    if (f) {

	list_t new;

	define_file = strdup(fn);
	fprintf(f, "/* Created automatically by cc */\n");

	/* write all the defines to the pre-include file */
	lptr = l_define.list;
	while (lptr != 0) {
	    char *p = strchr(lptr->str,'=');
	    char *s;
	    int quoted = 0;
	    /* lptr->str is const, but existing code is modifying
	     * the contents.
	     */
	    s = (char *)lptr->str;
	    if (*s == '"') {
		s++;
		quoted=1;
	    }
	    if (p) {
		int plen;
		*p++='\0';
		if (quoted) {
		    plen = strlen(p);
		    p[plen -1] = 0;
		}
		fprintf(f, "#define %s %s\n", s, p);
	    }
	    else
		fprintf(f, "#define %s 1\n",lptr->str);
            lptr = lptr->next;
	}

	/* write all the undefines to the pre-include file */
	lptr = l_undefine.list;
	while (lptr != 0) {
	    fprintf(f, "#undef %s\n",lptr->str);
            lptr = lptr->next;
	}

	fclose(f);

	/* Make this the first pre-include file */
	new = (list_t) malloc(sizeof(struct list));
	new->str = strdup(fn);
	new->next = l_preinclude_files.list;
	l_preinclude_files.list = new;
	if (l_preinclude_files.last == NULL)
	    l_preinclude_files.last = (list_t *) new;
    }
    else {
	printf("Error, unable to create pre-include file %s\n", fn);
	exit(EXIT_FAILURE);
    }

    return;
}

/*
 * Write the list to the command channel
 * If the list is a set of options then enclose it
 * in parentheses.
 */

void output_list (FILE *fp, arglist_t *arglist)
{
    list_t lptr = arglist->list;
    char dcl_buf[MAX_DCL_LINE_LENGTH+1], this_one[512];
    int len = 0;
    char *p;

    if (!lptr)
	return;

    *dcl_buf = '\0';

    if (arglist->str && !arglist->rule)
	sprintf(dcl_buf, "%s=(", arglist->str);

    len = strlen(dcl_buf);
    p = dcl_buf + len;

    while (lptr) {
	int this_len;

	if (arglist->quote) {
	    if (lptr->str[0] == '"') {
		strcpy(this_one, lptr->str);
	    } else {
		sprintf(this_one, "\"%s\"", fix_quote(lptr->str));
	    }
	} else {
	    strcpy(this_one, lptr->str);
	}
	this_len = strlen(this_one);

	if (arglist->str && lptr != arglist->list )
	    *p++ = ',';

	if (len && (len + this_len) > 127) {
	    *p = '\0';
	    fprintf(fp, "%s-\n", dcl_buf);
	    len = 0;
	    *dcl_buf = '\0';
	    p = dcl_buf;
	}

	strcpy(p, this_one);

	len += this_len;
	p += this_len;
	*p = '\0';

	lptr = lptr->next;
    }

    if (arglist->str && !arglist->rule)
	strcat(p++, ")");

    if (p != dcl_buf)
	fprintf(fp, "%s-\n", dcl_buf);

    return;
}

/*
** Make a DCL command option in one of the following forms
** 	str
** 	str=val
** 	str=(val)
** 	val
*/

char *make_opt( char *str, char *val)
{
    static char dcl_buf[MAX_DCL_LINE_LENGTH + 1];

    if (str && *str) {
	if (!val || !*val)
	    return str;

	if (strchr(val,','))
	    sprintf(dcl_buf, "%s=(%s)", str, val);
	else
	    sprintf(dcl_buf, "%s=%s", str, val);

	return strdup(dcl_buf);
    }

   return val;
}

/*
** Find the UNIX style argument in the table defined
** in ccopt.h. This uses a binary search.
**
** The table is sorted here so that the header can
** be arranged to suit the maintainer.
*/

int compare_options(const void *p1, const void *p2)
{
    register const ccopt_t *opt1 = *(ccopt_t **)p1;
    register const ccopt_t *opt2 = *(ccopt_t **)p2;

    return strcmp(opt1->arg_str, opt2->arg_str);
}

ccopt_t *lookup_arg(char *arg)
{
    int low = 0;
    int high = DIM(ccopt_table) - 1;
    int mid;
    static ccopt_t **sorted_ccopts = NULL;

    /*
    ** Build a sorted array of pointers to enries in ccopt_table
    */

    if (!sorted_ccopts) {
	int i;
	sorted_ccopts = calloc(DIM(ccopt_table), sizeof(ccopt_table[0]));

	if (!sorted_ccopts) {
	    perror("? calloc");
	    exit(EXIT_FAILURE);
	}

	for (i = 0; i < DIM(ccopt_table); i++)
	    sorted_ccopts[i] = &ccopt_table[i];

	qsort((void *)sorted_ccopts, DIM(ccopt_table),
              sizeof(sorted_ccopts[0]), compare_options);

#ifdef DEBUG_SORT
	/* If you need to verify sort worked okay */
	for (i = 0; i < DIM(ccopt_table); i++)
	    printf("%2d. -%s\n", i, sorted_ccopts[i]->arg_str);
#endif
    }

    while (low <= high) {
	ccopt_t *ccopt;
	int cond;

	mid = (high+low) / 2;
	ccopt = sorted_ccopts[mid];

	switch(ccopt->parse) {
	    case X_xval:
		cond = strncmp(arg, ccopt->arg_str, strlen(ccopt->arg_str));
		break;

	    case X_noval:
	    case X_val:
	    default:
		cond = strcmp(arg, ccopt->arg_str);
		break;
	}

	if (!cond)
	    return ccopt;

	if (cond < 0)
	    high = mid - 1;
	else if (cond > 0)
	    low = mid + 1;
    }

    return NULL;
}

/*
** Clear a list.
*/

void del_list(arglist_t *lptr)
{
    lptr->list = 0;
    lptr->last = &lptr->list;
}


/*
** Lookup a keyword allowed for an argument and return
** its equivalence.
*/

char *lookup_val(char *i_src, char *r_str)
{
    char c;
    int len = strlen(i_src);

    for (r_str = strchr(r_str,'|'); r_str; r_str = strchr(r_str,'|')) {
	r_str++;

	if (!strncmp(i_src, r_str, len) && r_str[len] == '=') {
	    char *ptr;
	    char *e_ptr;
	    r_str += len + 1;

	    ptr = strdup(r_str);
	    if (ptr) {
		e_ptr = strchr(ptr, '|');
		if (e_ptr)
		    *e_ptr = '\0';
		return ptr;
	    }
 	}
    }
    errmsg("Ignoring unrecognized option %s", i_src);
    return NULL;
}

/*
** Fully dereferences a symlink if filename is a symlink or return filename
** if filename is not a symlink. This routine is a NOOP operation if S_ISLINK i
** is undefined.
*/
#ifndef PATH_MAX
#ifdef __VAX
#define PATH_MAX 255
#else
#define PATH_MAX 4096
#endif
#endif
static char *dereference_symlink(const char *filename){
#if defined (S_ISLNK)
   char actual_file[PATH_MAX+1];
   struct stat st;
   int number_found;

   /*
    ** Get the Unix filename.
    */
   if (strpbrk(filename,"<[>]:"))
       strcpy(actual_file, vms_to_unix(filename));
   else
       strcpy(actual_file, filename);

   /*
    ** Walk through all symlinks to get the final translation
    */
#if __CRTL_VER >= 80300000
   while (!lstat(actual_file,&st) && S_ISLNK(st.st_mode)){
       char realfile[PATH_MAX + 1];
       ssize_t size;

       memset(realfile, 0, sizeof(realfile));
       size = readlink(actual_file, realfile, sizeof(realfile));
       if (!lstat(realfile, &st) && S_ISREG(st.st_mode)){
           strcpy(actual_file, realfile);
           break;
       }
       if (!lstat(realfile, &st) && S_ISLNK(st.st_mode))
           strcpy(actual_file, realfile);
       else{
           char temp_name[PATH_MAX + 1];
           strcpy(temp_name, actual_file);
           strcpy(actual_file, dirname(temp_name));
           strcat(actual_file, "/");
           strcat(actual_file, realfile);
       }
   }
#endif
#endif

   /*
   ** Get the VMS style dereferenced filename if
   ** filename was a VMS style filename.
   */
   if (strpbrk(filename, "<[>]:"))
       return(unix_to_vms(actual_file, 0));

   return strdup(actual_file);
}

/* Convert the library name to GNV name */
char * lib_to_gnv_name(const char * name) {
    char lib_path[1024];
    char value[256];
    char * test_path;
    char * test_name;
    char * lastdot;
    char * ret_name;
    int status;
    int i;

    ret_name = NULL;
    test_path = strdup(name);
    test_name = basename(test_path);

    /* Convert to a logical name */
    if (strncasecmp("lib", test_name, 3) == 0) {
        sprintf(lib_path, "gnv$%s", test_name);
    } else {
        sprintf(lib_path, "gnv$lib%s", test_name);
    }

    /* remove the suffix */
    lastdot = strrchr(lib_path, '.');
    if (lastdot) {
        if ((strncasecmp(lastdot, ".a", 3) == 0) ||
	    (strncasecmp(lastdot, ".so", 4) == 0) ||
	    (strncasecmp(lastdot, ".olb", 5) == 0) ||
	    (strncasecmp(lastdot, ".exe", 5) == 0)) {

	    lastdot[0] = 0;
	}
    }

    /* Fix up the logical name to not have special characters */
    i = 7;
    while (lib_path[i] != 0) {
	if (!isalnum(lib_path[i])) {
	    if (lib_path[i] == '+') {
		lib_path[i] = 'x';
	    } else {
		lib_path[i] = '_';
	    }
	}
	i++;
    }
    /* Verify that this is a logical name. */
    /* The file may not yet exist */
    status = sys_trnlnm(lib_path, value, 255);
    if ($VMS_STATUS_SUCCESS(status)) {
	ret_name = strdup(lib_path);
    }
    free(test_path);
    return ret_name;
}

/*
** Locate a library in one of the specified directpories.
**
*/

char *lookup_lib(list_t libdir, const char *libname)
{
    char lib_path[1024];
    char temp[]= "lib" ;
    list_t lptr = libdir;
    int handle, i;

    if (!gnv_link_lib_over_shared) {
	const char *name;
	char *gnv_lib_name;

	/* Skip over optional leading colon */
	name = libname;
	if (name[0] == ':') {
	    name++;
	}

	/* Convert to a logical name */
	gnv_lib_name = lib_to_gnv_name(name);
	if (gnv_lib_name != NULL) {
	    return gnv_lib_name;
	}
    }

    while (lptr != 0) {
	if (libname[0] != ':') {
	    i = 0;
	    while (lib_types[i]) {
		sprintf(lib_path, "%s/%s%s.%s", lptr->str,temp,libname,
			lib_types[i]);
		if (file_exists(lib_path)) {
		    return strdup(dereference_symlink(lib_path));
		}
		i++;
	    }
	} else {
	    /*
	     * When linking with an object library such as
	     * TCPIP$LIBRARY:TCPIP$LIB.OLB or any other VMS library not
	     * prefixed with "lib". The correct Linux syntax way is to
	     * add the colon character between -l and mylib.ext (-l:mylib.ext)
	     * In which case it looks for mylib.ext instead of libmylib.*
	     */
	    struct stat st;
	    sprintf(lib_path, "%s/%s", lptr->str, &libname[1]);
	    if (!lstat(dereference_symlink(lib_path),&st)) {
	        return strdup(dereference_symlink(lib_path));
	    }
	}             /* end of if else condition */

	lptr = lptr->next;
    }

    return 0;
}


int test_is_library(const char *str, unsigned int slen)
{
    int result;
    char * lastdot;
    char * test_name;
    char * test_path;
    char * lib_path;
    lastdot = strrchr(str, '.');
    result = test_suffix(str, slen, ".a") ||
	     test_suffix(str, slen, ".olb");
    if (result == 0) {
	return result;
    }
    /* Fix-me, we do this test and then discard the result many times. */
    lib_path = lib_to_gnv_name(str);
    if (lib_path != NULL) {
	result = 0;
	free(lib_path);
    }
    return result;
}

int test_is_shared_library(const char *str, unsigned int slen)
{
    char * lastdot;
    lastdot = strrchr(str, '.');
    if (lastdot == NULL) {
	/* Check to see if this is our special logical name */
	if (strncasecmp("gnv$lib", str, 7) == 0) {
	    return 1;
        }
    } else {
	int result;
	result = test_suffix(str, slen, ".a") ||
		 test_suffix(str, slen, ".olb");
	if (result) {
	    char * test_name;
	    char * test_dir;
	    char * lib_path;
	    char * test_path;
	  /* Fix-me, we do this test and then discard the result many times. */
	    lib_path = lib_to_gnv_name(str);
	    if (lib_path != NULL) {
		free(lib_path);
		return 1;
	    }
	}
	result = test_suffix(str, slen, ".so") ||
		 test_suffix(str, slen, ".exe");
	return result;
    }
    return 0;
}

int test_is_object(const char *str, unsigned int slen) {
    return test_suffix(str, slen, ".o") ||
	   test_suffix(str, slen, ".obj") ||
	   test_suffix(str, slen, ".ho");	/* SAMBA 4 hack */
}

void usage(const char *str, ...)
{
    va_list va;
    va_start(va, str);

    printf("Error: ");
    vprintf(str, va);

    va_end(va);

    exit(GNV_ERROR_STATUS);
}


void do_outfile_cmdfile(FILE *fp, const char *action) {
    char * name;
    int name_len;
    char * path;
    int path_len;
    char cmd_file[256];
    char * ext;
    char * output_file;
    char * output_file2;
    int acc_stat;

    if (outfile == NULL) {
	return;
    }
    output_file = strdup(outfile);
    output_file2 = strdup(outfile);
    path = dirname(output_file2);
    path_len = strlen(path);
    name = basename(output_file);
    ext = strrchr(output_file, '.');
    if (ext == NULL) {
	name_len = strlen(name);
    } else {
	name_len = ext - name;
    }
    strcpy(cmd_file, path);
    strcat(cmd_file, "/gnv$");
    path_len += 5;
    strncat(cmd_file, name, name_len);
    cmd_file[path_len + name_len] = 0;
    strcat(cmd_file, "_");
    strcat(cmd_file, action);
    strcat(cmd_file, ".com");
    acc_stat = access(cmd_file, F_OK);
    if (acc_stat == 0) {
	fprintf(fp, "$@%s\n", unix_to_vms(cmd_file, FALSE));
    }
    free(output_file);
    free(output_file2);
}

/*
** Generate the commands to compile
**
*/

void do_compile(FILE *fp, list_t files_list, int use_cxx)
{
    int many_includes;
    list_t lptr;
    const char *fmt_str = "$if f$type(%s) .eqs. \"STRING\" then "
		          "if f$extract(0,1,%s) .eqs. \"$\" then %s := %s\n";
    const char *cxx = "cxx";
    const char *cc = "cc";

    if (use_cxx) {
	if (files_list == NULL) {
	    /* Version */
	    fprintf(fp, "$cxx := cxx\n");
	} else {
	    if (gnv_cxx_set_command)
		fprintf(fp, "$%s\n", gnv_cxx_set_command);

	    /* Prevent loops from cxx :== $gnv$gnu:[usr.bin]gnv$cxx.exe */
	    fprintf(fp, fmt_str, cxx, cxx, cxx, cxx);
	}
	do_outfile_cmdfile(fp, cxx);
	fprintf(fp, "$cxx -\n");
    }
    else {
	if (files_list == NULL) {
	    /* Version */
	    fprintf(fp, "$cc := cc\n");
	} else {
	    if (gnv_cc_set_command)
		fprintf(fp, "$%s\n", gnv_cc_set_command);

	    /* Prevent loops from cc :== $gnv$gnu:[usr.bin]gnv$cc.exe */
	    fprintf(fp, fmt_str, cc, cc, cc, cc);
	}
	do_outfile_cmdfile(fp, cc);
	fprintf(fp, "$cc -\n");
    }

    output_list(fp, &l_compile);
    output_list(fp, &l_accept);
    output_list(fp, &l_assume);
    output_list(fp, &l_check);
    output_list(fp, &l_optimize);
    output_list(fp, &l_proto);
    output_list(fp, &l_show);
    output_list(fp, &l_warn);
    output_list(fp, &l_extra_compile);

    /*
     * Are we doing MMS dependency files?
     */
    if (mms_dep_file) {
	fprintf (fp, "/MMS");

	if (mms_file || mms_nosys)  /* If filename specified, or no system */
				    /* include files requested	*/
	    fprintf (fp, "=(");	    /* Then we'll be using the /MMS  */
				    /* qualifier(s)  */

	if (mms_file)		    /* If filename specified, then apply it */
	   {
	    char *file;

	    file = unix_to_vms (mms_file, 0);	/* Translate it to VMS */
	    fprintf (fp, "FILE=%s", file);

	    if (mms_nosys)	/* If also NO system include files */
				/* requested, then we'll be doing THAT. */
		fprintf (fp, ", ");	/*	Add a comma.		*/
	    }

	if (mms_nosys) {		/* If no system include files, then */
					/* do it. */
	    fprintf (fp, "NOSYST");
        }
	if (mms_file || mms_nosys) { /* If either filename or no system */
				     /* include files, then close the parens. */
	    fprintf (fp, ")");
        }
	fprintf (fp, "-\n"); /* And finally, finish off the /MMS qualifier! */
    }

    /*
    ** If the lists of defines and undefines are too long, then use a
    ** pre-include file.
    */

    if (use_pre_include_file()) {
	char *f;
	if (outfile && !linkx)
	    f = outfile;
	else {
	    if (files_list)
		f = basename((char *)files_list->str);
	    else
		f = "";
	}
	create_define_list(f);
    }
    else {
	output_list(fp, &l_define);
	output_list(fp, &l_undefine);
    }

    /*
    ** Nested include files aren't handled by our compilers in the same way
    ** that they are by most UNIX compilers. In particular an #include of
    ** something like "../foo/bar.h" will, on UNIX systems, base the
    ** relative spec on the directory where the file containing the
    ** #include resides. On OpenVMS this doesn't always happen.
    **
    ** One case of where it doesn't work as expected is when we are using
    ** our preinclude trick and compiling from SYS$INPUT. In this case
    ** we can work around the difference by including the directory where
    ** the source file resides in the include directory (-I) list.
    **
    ** Another case I've seen is when the source file is not in the current
    ** directory.
    **
    ** Both of these conditions we test for here and if found, we add the
    ** directory of the source file to the include directory list.
    **
    ** Which breaks the building applications that happen to have header
    ** files with the same name as a system header file in that directory,
    ** which is a common occurance.
    **
    ** Allow this to be disabled.  When disabled and actually needed,
    ** either the gnv_cc_no_module_first will not be set or the CC command
    ** redefined to be /NESTED_INCLUDE_DIRECTORY=(PRIMARY).
    */

    if ((files_list != NULL) && !gnv_cc_no_inc_primary) {
	char *dn = dirname(strdup(files_list->str));
	if ( l_preinclude_files.list || strcmp(dn,".") ) {
	    /* condition is met, we need the work-around */
	    if (*dn == '/')
		append_last(&l_include_dir, strdup(dn));
	    else {
		char iname[MAX_FILENAME_BUF];
		sprintf(iname,"./%s",dn);
		append_last(&l_include_dir, strdup(iname));
	    }
	}
    }

    /*
    ** We need to perform the following fixups on any include file spec:
    **
    **   . -> ./
    **   .. -> ../
    **   if no slash present, then prefix ./
    */
    if (l_include_dir.list) {
        lptr = l_include_dir.list;
        while (lptr) {
	    char fixed[256];
            if (strcmp(".", lptr->str) == 0)
                sprintf(fixed, "./");
            else if (strcmp("..", lptr->str) == 0)
                sprintf(fixed, "../");
            else if (strchr(lptr->str,'/') == 0)
                sprintf(fixed, "./%s", lptr->str);
            else
		fixed[0] = '\0';

	    if (fixed[0] != '\0') {
		free((char *)lptr->str);
		lptr->str = strdup(fixed);
	    }

            lptr = lptr->next;
        }
    }

    /*
    ** Do we have too many -I include directories for the command line?
    */
    many_includes = use_pre_include_file();

    /*
    ** Output the include (-I) directories if they're going on the command
    ** line.
    */
    if ((l_include_dir.list) && (!many_includes)) {
	fprintf(fp, "/inc=(-\n");
	lptr = l_include_dir.list;
	while (lptr) {
	    if (lptr != l_include_dir.list)
		fprintf(fp, ",-\n");
	    fprintf(fp, "\"%s\"", lptr->str);
	    lptr = lptr->next;
	}
	fprintf(fp, ")-\n");
    }

    if (preprocess) {

        if (!is_cxx && !use_cxx)
            fprintf(fp, "/stand=common -\n");

        sprintf(prep_file_name, "%si", cmd_proc_name_ptr); /* append an i */
        fprintf(fp, "/prep=%s -\n",
          prep_file_name[0] == '/' ?            /* If UNIX format... */
            unix_to_vms(prep_file_name, FALSE): /* ... then convert to VMS */
            &prep_file_name[0]);                /* ... else leave it as is */

       }


/*
** This is the new pre-include code. We also use the "compile sys$input" trick
** if we have too many -I directories and need to use the "pragma
** include_directory" instead.
*/

    if (files_list) {
        /* Specify the output file */
        if (!preprocess) {
            if (no_object == 1) {
                fprintf(fp, "/noobject -\n");
            } else if (outfile && !linkx) {
                fprintf(fp, "/object=%s -\n", unix_to_vms(outfile, FALSE));
            } else {
                fprintf(fp, "/object=%s -\n",
                        unix_to_vms(basename(
			    new_suffix3(files_list->str,
					default_obj_suffix)), FALSE));
	    }
        }

	if (l_preinclude_files.list || many_includes) {
	    char tmpname[MAX_FILENAME_BUF];

	    /* Compile SYS$INPUT */
            fprintf(fp, "sys$input:%s\n",basename((char *)files_list->str));

	    /*
	    ** Write all the -I directories first, if they're not already
	    ** on the command line.
	    */
	    if (many_includes) {
		lptr = l_include_dir.list;
		while (lptr != 0) {
		    fprintf(fp, "#pragma include_directory \"%s\"\n",lptr->str);
		    lptr = lptr->next;
		}
	    }

	    /* Include all the pre-include header files next */
            lptr = l_preinclude_files.list;
            while (lptr) {
	        char *p = unix_to_vms_exp(lptr->str, FALSE);
                fprintf(fp, "#include \"%s\" /* -include %s */\n",
			p, lptr->str);
                free(p);
                lptr = lptr->next;
            }

	    /* Then include all the source files */
            lptr = files_list;
            while (lptr) {
	        char *p = unix_to_vms_exp(lptr->str, FALSE);
                fprintf(fp, "#include \"%s\" /* %s */\n", p, lptr->str);
                lptr = lptr->next;
	        free(p);
            }
	} else {
	    /* Specify all the source files */
            lptr = files_list;
            while (lptr) {
                fprintf(fp, "%s", unix_to_vms(lptr->str, FALSE));
                lptr = lptr->next;
                if (lptr)
                    fprintf(fp, "+-\n");
	        else
                    fprintf(fp, "\n");
            }
        }
    } else {
        fprintf(fp, "\n");
    }
}

/*
 *  To avoid needing to modify makefiles, if the user has not specified
 *  an option file, we add a special one that we look up using the execname.
 *  It needs to be prefixed because otherwise the Configure scripts
 *  sometimes attempt to delete ones required for them to pass if we put
 *  them in the default directory.
 */

void add_special_opt_file(const char *execname)
{
    char *bname;
    const char *suf;
    char optname[1024];

    if (!gnv_opt_dir)
        return;

    bname = basename((char*)execname);
    sprintf(optname, "%s/%s%s", gnv_opt_dir,"gnv$", new_suffix3(bname, ".opt"));

    if (file_exists(optname)) {
        printf("Adding special opt file %s to link\n", optname);
        append_last(&l_optfiles, strdup(optname));

	return;
    }

     /* Fall back to old name */
    /*-----------------------*/
    sprintf(optname, "%s/%s", gnv_opt_dir, new_suffix3(bname, ".opt"));

    if (file_exists(optname)) {
        printf("Adding special opt file %s to link\n", optname);
        append_last(&l_optfiles, strdup(optname));
    }
}

/*
** Generate the command to link
**
*/

void do_link(FILE *fp, FILE *ofp, const char * opt_name_ptr)
{
    const char *curfile;
    char curbuf[1024];
    const char *outfile_suffix;
    int i;
    list_t lptr;
    int need_fixup = 0;
    int lib_load_all = 0;
    int obj_count = 0;

    if (link_incr == 0) {

	do_outfile_cmdfile(fp, "link");

#ifndef __ia64
        /* VAX and Alpha use CXXLINK for C++ binaries */
	if (is_cxx || (force_src == force_cxx)) {
	    if (gnv_cxx_set_command)
        	fprintf(fp, "$%s\n", gnv_cxx_set_command);

	    fprintf(fp, "$cxxlink -\n");
	}
	else
#endif
        {
	    fprintf(fp, "$link -\n");
        }

	if (debug_link)
	    fprintf(fp, "%s-\n", "/DEBUG");

	output_list(fp, &l_linker);
	output_list(fp, &l_extra_link);

    }
    else {
	fprintf(fp, "$copy/conc -\n");
    }

    if (!outfile)
	if (gnv_suffix_mode && !strcasecmp(gnv_suffix_mode, "vms"))
            outfile = DEFAULT_VMS_EXE;
	else
	    outfile = DEFAULT_UNIX_EXE;

    outfile_suffix = get_suffix(outfile);

    /* If no suffix specified, use default. Do not let link add the default. */

    if (outfile_suffix[0] == '\0')
        outfile = new_suffix(outfile,
                             outfile_suffix - outfile,
                             default_exe_suffix);

    /* If pretending to incremental link, we are done. */
    if (link_incr != 0) {
	debug = 0;
	sharedx = 0;
	auto_symvecx = 0;
    }
    else {
        if (!sharedx) {
	    /* If the output file type is ".so" then link/shared */
	    char * suffix;
            suffix = strrchr(outfile, '.');
	    if (suffix != NULL) {
                if (strncasecmp(suffix, ".so", 4) == 0) {
		    sharedx = 1;
		}
	    }
        }
	fprintf(fp,
	    "/%s=%s-\n", sharedx ? "shar" : "exec",
	    unix_to_vms(outfile, FALSE));

	if (gnv_link_map) {
	    char *mapfile;
	    mapfile = new_suffix3(outfile, ".map");

	    fprintf(fp,
	        "/MAP=%s-\n", unix_to_vms(mapfile, FALSE));
	}
    }

#ifndef __VAX
    if (debug && !debug_link) {
	static char opt_buf[1024];
	const char *dsf_suffix;

	strcpy(opt_buf, outfile);
	dsf_suffix = get_suffix(opt_buf);
	strcpy((char *)dsf_suffix, ".DSF");
	fprintf(fp, "/DSF=%s-\n", unix_to_vms (opt_buf, FALSE));
    }
#endif

    /*
    ** This is the start of the processing for the auto-generated
    ** linker options file which contains the SYMBOL_VECTOR statements.
    */
#ifndef __VAX
    if (sharedx && auto_symvecx) {

	char *file_auto_symvec;

	/* create the auto-symvec file name */
	file_auto_symvec = new_suffix3(outfile, "_symvec.opt");

	/* If the file is present then read in the existing symbols */
	/* If the file is not present then create it now */

	if (file_exists(file_auto_symvec)) {
	    time_t now = time((time_t *) NULL);
	    symvecRead(file_auto_symvec);
	    f_auto_symvec = fopen(file_auto_symvec, "a");
	    if (!f_auto_symvec) {
		errmsg("Unable to update auto_symvec file %s",
		    file_auto_symvec);
		exit(EXIT_FAILURE);
	    }
	    sprintf(symvecHeadingDate,
		"! \n! Auto-updated by GNV LD on %s", ctime(&now));
	}
	else {
	    int status;
	    unsigned long val;
	    struct _generic_64 curTime;
	    time_t now = time((time_t *) NULL);
	    f_auto_symvec = fopen(file_auto_symvec, "w");
	    if (!f_auto_symvec) {
		errmsg("Unable to create auto_symvec file %s",
		    file_auto_symvec);
		exit(EXIT_FAILURE);
	    }
	    /*
	    ** Generate a unique major/minor ident. We take the middle 32
	    ** bits of ths system time and then use the top 8 bits for the
	    ** major ident and the bottom 24 bits for the minor ident.
	    */
	    status = sys$gettim(&curTime) ;
	    val = (curTime.gen64$w_word[2] << 16) | curTime.gen64$w_word[1];
	    fprintf(f_auto_symvec,
		"! *** DO NOT EDIT THIS FILE ***\n"
		"! Auto-created by GNV LD on %s"
		"GSMATCH=EQUAL,%d,%d\n"
		"case_sensitive=YES\n",
		  ctime(&now),
		  (val & 0xFF000000) >> 24, val & 0xFFFFFF );
	}

	/*
	** Print the options file name now so it appears before any other
	** options files. We need to do this since it may not contain a
	** directory name and we don't want to get bitten by file stickyness.
	*/
	fprintf(fp, " %s/opt,-\n",unix_to_vms(file_auto_symvec, FALSE));

    }
#endif

    if (link_incr == 0) {

        if (opt_name_ptr == NULL) {
	    fprintf(fp, " sys$input:/opt");
	} else {
	    fprintf(fp, " %s/opt", unix_to_vms_exp(opt_name_ptr, FALSE));
	}

	/*
	 *  Add the user opt files after the compiler opt files. We create
	 *  a special opt file is specified.
	 */
	add_special_opt_file(outfile);

	for (lptr = l_optfiles.list; lptr; lptr = lptr->next)
	    fprintf(fp, ",-\n %s/opt", unix_to_vms_exp(lptr->str, FALSE));

	fprintf(fp, "\n");

        if (opt_name_ptr == NULL) {
	    fprintf(fp, "$DECK\n");
	}
    }

    /* Philippe Vouters - start of linking order fix. */

    /*
     * Compute the number of files which are either object files or archives.
     */
    obj_count=0;
    for (lptr = l_objfiles.list; lptr; lptr = lptr->next) {
         if (test_is_object(lptr->str,strlen(lptr->str)) ||
             test_is_library(lptr->str,strlen(lptr->str)))
             obj_count++;
    }
    /*
     * Rework the link list of files to link with.
     */
    i=0;

    /* Hack to skip the rework if only one obj_count */
    if (obj_count == 1) {
	i = 2;
    }
    lptr=l_objfiles.list;
    /*
     * Move down all shared libraries.
     */
    while (lptr && i < obj_count){
       if (test_is_object(lptr->str,strlen(lptr->str)) ||
           test_is_library(lptr->str,strlen(lptr->str))){
           lptr = lptr->next;
           i++;
           continue;
       }
       if (test_is_shared_library(lptr->str,strlen(lptr->str))){
           /*
            * move down library.
            */
           list_t lptr_first;
           char *str;

           lptr_first = lptr->next;
           if (!lptr_first)
               break;
           if (test_is_shared_library(lptr_first->str,
                                      strlen(lptr_first->str))){
               lptr = lptr_first;
               continue;
           }
           str=(char *)lptr->str;
           lptr->str=lptr_first->str;
           lptr_first->str = (const char *)str;
           i=0;
           lptr = l_objfiles.list;
      }
    }

    /* Philippe Vouters - end of linking order fix. */

    obj_count = 0;
    for (lptr = l_objfiles.list; lptr; lptr = lptr->next) {
        int len;

	if (!strcmp(lptr->str, &ld_token_all[0])) {
	    lib_load_all = 1;
	    continue;
	}

	if (!strcmp(lptr->str, &ld_token_none[0])) {
	    lib_load_all = 0;
	    continue;
	}

        if (*lptr->str == '/') {
            curfile = lptr->str;
            need_fixup = 1;
        }
	else {
            if (need_fixup && (strncasecmp(lptr->str, "gnv$lib", 7) != 0 )) {
                sprintf(curbuf, "%s/%s", curdir, lptr->str);
                curfile = curbuf;
            }
	    else
                curfile = lptr->str;

            if (!strcmp(lptr->str, basename((char*)lptr->str)))
                need_fixup = 0;
            else
                need_fixup = 1;
        }

        len = strlen(curfile);

	/* Will have to change link_incr to create object libraries to
	 * support this if this case comes up.
	 */
        if ((link_incr == 0) && test_is_library(curfile, len)) {
	    if (lib_load_all) {
		unsigned long s;
		/* library file - include symbols */
		if (f_auto_symvec)
/*
** Steve, there is a process_olb_file_ia64 in ELF.C, but we don't use it.
** Why not? Because its identical to process_olb_file except for the fact
** that we call the ia64 version of the action routine, so we always call
** process_olb_file here, and he decides which action routine to specify.
** We could call process_olb_file_ia64 here; its swings and roundabouts...
*/
		    process_olb_file(unix_to_vms(curfile, FALSE));
		s = lib_list(unix_to_vms(curfile, FALSE),lib_action_rtn);
		if(!$VMS_STATUS_SUCCESS(s)) {
		    printf(
			"** Error obtaining module list from %s, status=%d\n",
			curfile, s);
		    /* force a linker error */
        	    fprintf(ofp, "%s/INCLUDE=SEE_ERROR_ABOVE\n",
			    unix_to_vms(curfile, FALSE));
		}
	    } else {
	        int acc_stat;
		acc_stat = access(curfile, F_OK);
		if (acc_stat != 0) {
		    printf("** Error library %s does not exist!\n", curfile);
		    fprintf(ofp, "%s/FAIL=MISSING_FILE\n", curfile);
		}
		fprintf(ofp, "%s/lib\n", unix_to_vms(curfile, FALSE));
	    }
	} else if ((link_incr == 0) && test_is_shared_library(curfile, len)) {

	    /* Only include a shared library once */
	    int no_translate;
	    char * lastdot;
	    int acc_stat;
	    list_t tptr;
	    for (tptr = l_objfiles.list; tptr != lptr; tptr = tptr->next) {
		if (strcasecmp(tptr->str,lptr->str) == 0) {
        	    fprintf(ofp, "!");
		    break;
		} else {
		    /* Make sure that the file exists */
		    acc_stat = access(curfile, F_OK);
		    if ((acc_stat != 0) &&
			(strncasecmp(curfile, "gnv$lib", 7) == 0)) {
			/* Ignore this check for gnv$lib imaages */
			/* They may not exist yet */
			acc_stat = 0;
		    }
		    if (acc_stat != 0) {
			printf("** Error shared image %s does not exist!\n",
			       curfile);
			fprintf(ofp, "%s/FAIL=MISSING_FILE\n", curfile);
		    }
		}
	    }
	    lastdot = strrchr(curfile, '.');
	    no_translate = 0;
	    if (lastdot == NULL) {
		/* Check to see if this is our special logical name */
		if (strncasecmp("gnv$lib", curfile, 7) == 0) {
		    no_translate = 1;
		}
	    }
	    if (no_translate == 1) {
		fprintf(ofp, "%s/share\n", curfile);
	    } else {
		char * gnv_lib_file;
		gnv_lib_file = lib_to_gnv_name(curfile);
		if (gnv_lib_file != NULL) {
		    fprintf(ofp, "%s/share\n", gnv_lib_file);
		    free(gnv_lib_file);
		} else {
		    fprintf(ofp, "%s/share\n", unix_to_vms(curfile, FALSE));
		}
	    }
	} else {
	    /* object file - include symbols */
	    if ((link_incr == 0) && f_auto_symvec) {
/*
** Steve, if you ever support cross-linking in GNV then the compile
** time test below to determine which version of process_obj_file to
** call will have to become a run time test.
*/
#ifdef __ia64
		process_obj_file_ia64((char *) curfile);
#else
		process_obj_file((char *) curfile);
#endif
	    }
	    if (link_incr == 0) {
		fprintf(ofp, "%s\n", unix_to_vms(curfile, FALSE));
	    } else {
		obj_count++;
		if (obj_count > 1) {
		    fprintf(ofp, ",%s-\n", unix_to_vms(curfile, FALSE));
		} else {
		    fprintf(ofp, "%s-\n", unix_to_vms(curfile, FALSE));
                }
	    }
	}
    }

    /* If pretending to incremental link, we are done. */
    if (link_incr != 0) {
	fprintf(ofp, " %s\n", unix_to_vms(outfile, FALSE));
	return;
    }

    for (i = 0; i < DIM(sysshr_libs); i++ ) {
	if (sysshr_libs[i].seen) {
	    fprintf(ofp, "%s/share\n",
		getenv(sysshr_libs[i].lognam) ?
		    sysshr_libs[i].lognam :
		    sysshr_libs[i].file
	    );
	}
    }

    /* If we were writing to the auto-generated symbol options file, close it */
    if (f_auto_symvec)
	fclose(f_auto_symvec);

    /*
    ** Linker generated symbol table only valid for shared images.
    */
    if (sharedx && linker_symvecx)
	fprintf(ofp, "SYMBOL_VECTOR=AUTO_GENERATE\n");

    if (opt_name_ptr == NULL) {
	fprintf(fp, "$EOD\n");
    }
}


/*
 * For one MMS Dependency file, translate it from
 * VMS to Unix style filespecs.
 *
 * Its lines are in the form:
 *
 *	hello.OBJ :     STP:[TEST]hello.c
 *
 * And we need to process that final filespec.
 */
void postProcessMMSOne (char * filename)
{
int infile, outfile;
char line1 [MAX_FILENAME_BUF], line2 [MAX_FILENAME_BUF];
int i;
int count;
char *str;

infile = open (filename, O_RDONLY, 0);
if (infile < 0)
    return;

outfile = open (filename, O_WRONLY | O_TRUNC, 0);

while (count = read (infile, line1, MAX_FILENAME_BUF), count != 0)
   {
    char *pin, *pout;

    line1 [count-1] = 0;

    /* Find the object name and translate that */
    pin = strstr(line1," :");
    if (pin == NULL) {
        printf("Bad format in MMS file\n");
        continue;
    }
    *pin = '\0'; /* for now, terminate string after the filespec */
    str = vms_to_unix (&line1[0]);
    strcpy (&line2[0], str);
    free (str);
    *pin = ' '; /* put line back to how it was */

    /* Copy the " :" and then copy the white space */
    pout = &line2[strlen(line2)];
    *pout++ = *pin++;
    *pout++ = *pin++;
    while ( (*pin == ' ') || (*pin == '	'))	/* One space, one tab */
        *pout++ = *pin++;

    /* Translate the source filespec from VMS to Unix */
    str = vms_to_unix (pin);
    strcpy (pout, str);
    free (str);

    /* SYS$INPUT can show up as a source file when doing the pre-include
       trick - ignore it */
    if (strncasecmp(pout,"/SYS$INPUT/",11))
        write (outfile, line2, strlen (line2));
    }
close (infile);
close (outfile);
}

/*
 * For the MMS Dependency file, we need to go translate the
 * VMS filespecs to Unix style filespecs.
 *
 * Even if there are many C and CXX sourcefiles, there is only
 * one MMS file created by the compiler. By default it is named
 * the same as the first source file, but with a .MMS file type.
 */
void postProcessMMS () {
    char *f;
    list_t lptr = NULL;

    /*
    ** We need the name of the MMS file. If it was given on the
    ** command line, that's easy. Otherwise its the name of the
    ** first source file, but with .MMS as a file type.
    */
    if (mms_file) {
	postProcessMMSOne (mms_file);
    }
    else {
	if (l_cfiles.list) {
	    lptr = l_cfiles.list;
	}
	else {
	    if (l_cxxfiles.list) {
		lptr = l_cxxfiles.list;
	    }
	}
	if (lptr) {
	    postProcessMMSOne (new_suffix3(lptr->str,".MMS"));
	}
    }
}

int main(int argc, char *argv[])
{
    int i, out_is_err;
    int status;
    int seen_undef = 0;
    FILE *fp;
    int fd;
    char *p;
    char *suffix;
    int posix_compliant_pathnames;

    /* Replicate these settings to C/C++ compilers */
    sys_crelnm("DECC$ARGV_PARSE_STYLE", "ENABLE");
    sys_crelnm("DECC$DISABLE_POSIX_ROOT", "DISABLE");
    sys_crelnm("DECC$EFS_CHARSET", "ENABLE");
    sys_crelnm("DECC$READDIR_DROPDOTNOTYPE", "ENABLE");
    sys_crelnm("DECC$UNIX_PATH_BEFORE_LOGNAME", "ENABLE");
    sys_crelnm("DECC$FILENAME_UNIX_NO_VERSION", "ENABLE");

    gnv_cc_debug	= enabled("GNV_CC_DEBUG");
    gnv_crtl_sup	= enabled("GNV_CRTL_SUP");
    gnv_link_debug	= enabled("GNV_LINK_DEBUG");
    gnv_link_map	= enabled("GNV_LINK_MAP");
    gnv_cc_warn_info    = enabled("GNV_CC_WARN_INFO");
    gnv_cc_warn_info_all = enabled("GNV_CC_WARN_INFO_ALL");
    gnv_cc_no_posix_exit = enabled("GNV_CC_NO_POSIX_EXIT");
    gnv_cc_main_posix_exit = enabled("GNV_CC_MAIN_POSIX_EXIT");

#if __CRTL_VER >= 80200000 && !defined(__VAX)
    gnv_cc_no_use_std_stat = enabled("GNV_CC_NO_STD_STAT");
#else
    gnv_cc_no_use_std_stat = 1; /* It is not there to use */
#endif
    gnv_cxx_no_use_std_iostream = enabled("GNV_CXX_NO_STD_IOSTREAM");
    gnv_cc_sockaddr_len = enabled("GNV_CC_SOCKADDR_LEN");
    gnv_cc_module_first_dir = getenv("GNV_CC_MODULE_FIRST_DIR");
    gnv_cc_no_module_first = enabled("GNV_CC_NO_MODULE_FIRST");
    gnv_cc_no_inc_primary = enabled("GNV_CC_NO_INC_PRIMARY");
    module_first[0] = '\0';
    gnv_link_missing_lib_error = enabled("GNV_LINK_MISSING_LIB_ERROR");
    gnv_link_lib_over_shared = enabled("GNV_LINK_LIB_OVER_SHARED");
    gnv_link_auto_symvec_nodata = enabled("GNV_LINK_AUTO_SYMVEC_NODATA");
    gnv_link_auto_symvec_nodups = enabled("GNV_LINK_AUTO_SYMVEC_NODUPS");
    gnv_link_no_undef_error	= enabled("GNV_LINK_NO_UNDEF_ERROR");

    gnv_suffix_mode	= getenv("GNV_SUFFIX_MODE");
    gnv_cc_set_command	= getenv("GNV_CC_SET_COMMAND");
    gnv_cxx_set_command	= getenv("GNV_CXX_SET_COMMAND");
    gnv_cc_qualifiers = getenv("GNV_CC_QUALIFIERS");
    gnv_cxx_qualifiers = getenv("GNV_CXX_QUALIFIERS");
    gnv_link_qualifiers = getenv("GNV_LINK_QUALIFIERS");
    gnv_cxxlink_qualifiers = getenv("GNV_CXXLINK_QUALIFIERS");
    gnv_opt_dir         = getenv("GNV_OPT_DIR");


    p = getenv("GNV_CC_DEFINE_LENGTH_MAX");
    gnv_cc_define_length_max = p ? atoi(p) : DEFAULT_DEFINE_LEN_MAX;

    p = getenv("GNV_CC_INCLUDE_LENGTH_MAX");
    gnv_cc_include_length_max = p ? atoi(p) : DEFAULT_INCLUDE_LEN_MAX;

    p = getenv("GNV_LD_OBJECT_LENGTH_MAX");
    gnv_ld_object_length_max = p ? atoi(p) : DEFAULT_OBJECT_LEN_MAX;

    p = getenv("GNV_LINK_LIB_TYPES");
    if (p == NULL)
	p = strdup(lib_types_defs);
    lib_types[0] = strtok(p,lib_types_seps);
    i=0;
    while (lib_types[++i] = strtok(NULL,lib_types_seps)) {}

    posix_compliant_pathnames = 0;
#if __CRTL_VER >= 80300000
    i = decc$feature_get_index ("DECC$POSIX_COMPLIANT_PATHNAMES");
    if ( i >= 0 ) {
	posix_compliant_pathnames = decc$feature_get_value (i, 1);
    }
#endif
    set_program_name(argv[0]);
    p = basename(program_name);
    if (strncasecmp(p, "gnv$", 4) == 0) {
        p = p + 4;
    }
    if (strncasecmp(p, "debug-", 6) == 0) {
        p = p + 6;
    }
    strncpy(actual_name, p, MAX_NAME);
    actual_name[MAX_NAME] = 0;
    suffix = strrchr(actual_name, '.');
    if (suffix != NULL) {
        suffix[0] = 0;
    }
    init_utils(actual_name);
    if ((strncasecmp(p, "cxx", 3) == 0) ||
        (strncasecmp(p, "g++", 3) == 0) ||
        (strncasecmp(p, "c++", 3) == 0)) {
        is_cxx = 1;
    } else {
	if (strncasecmp(p, "cpp", 3) == 0) {
	    preprocess = 1;
	    linkx = 0;
	    objfile = 0;
            is_cc = 1;
	}
    }
    if ((strncasecmp(p, "cc", 2) == 0) ||
        (strncasecmp(p, "gcc", 3) == 0)) {
        is_cc = 1;
    }
    if (strncasecmp(p, "ld", 2) == 0) {
        is_ld = 1;
    }

    getcwd(curdir, sizeof(curdir) - 1, 0);

    /*
     *  Set the suffix mode.
     */

    if (gnv_suffix_mode && !strcasecmp(gnv_suffix_mode, "vms")) {
        default_obj_suffix = ".obj";
        default_exe_suffix = ".exe";
    } else {
        default_obj_suffix = ".o";
	if ( ! posix_compliant_pathnames)
	    default_exe_suffix = ".";
	    else default_exe_suffix = "";
    }

    /* The command line provided from Bash or other Unix tools is
     * pre-parsed.  From DCL it has not been parsed, but only has
     * the favor of removing multiple whitespace, and some of the
     * quotes that were significant in parsing.
     *
     * The various strings that have been separated may in fact be
     * single options spread over more than one argv[n], or multiple
     * options can be contained in one string.
     *
     * To properly parse it, we must reconstitute it back to a single
     * string with a space separating each component.
     */

    command_line_len = 0;
    if (argc > 1) {
	for (i = 1; i < argc; i++) {
	    command_line_len += strlen(argv[i]) + 1;
	}
	if (command_line_len > 0) {
	    int j;
	    int max_cmd_line;

	    /* Add space for optional quotes */
	    max_cmd_line = command_line_len + ((argc - 1) * 4);
	    command_line = malloc(max_cmd_line);
	    if (command_line == NULL) {
		perror("? malloc(max_cmd_line)");
		exit(EXIT_FAILURE);
	    }

	    /* Any argument has double quotes or whitespace it means that
	     * the argument was originally quoted, so for now restore the
	     * single quotes.  On UNIX you can actually pass any character
	     * here, we do not handle that at this time since we would have
	     * to figure out how to hand that off to the DCL session.
	     */
	    j = 0;
	    command_line[0] = 0;
	    for (i = 1; i < argc; i++) {
		char * test_str1;
		char * test_str2;
		int sq = 0;
		test_str1 = strchr(argv[i], '"');
		test_str2 = strpbrk(argv[i],
				    "\" \t,()!#~`@%^&+{}[]:;|<?>");
		if (strncmp(argv[i], "-W", 2) == 0) {
		    /* No known -Wx, needs single quoting... */
		    test_str2 = NULL;
		}
		if ((test_str1 == NULL) && (test_str2 != NULL)) {
		    command_line[j] = '\'';
		    j++;
		    command_line[j] = 0;
		    sq = 1;
		}
		strcpy(&command_line[j], argv[i]);
		j = strlen(command_line);
		if (sq) {
		    command_line[j] = '\'';
		    j++;
		}
		if (argv[i+1] != NULL) {
		    command_line[j] = ' ';
		    j++;
		}
		command_line[j] = 0;
	    }
	}
    }



    /*
     *  Scan the command line for any switches that could effect the
     *  overall behavior. Reset them to NULL to indicate they have
     *  already been processed.
     */

    for (i = 0; i < command_line_len; i++) {
    int j;

	/* Get rid of leading whitespace */
	while (isspace(command_line[i]) && command_line[i])
	    i++;

	j = i;
	if (command_line[i] == '-' && command_line[i+1] == 'x') {

	    /* white space after the -x is optional */
	    j += 2 ;

	    /* Beware of quoted arguments changing the command line */
	    while(isspace(command_line[j]) && (command_line[j] != '\0'))
		j++;

	    if (command_line[j] != 0) {
		if (!strncmp(&command_line[j], "c++", 3)) {
		    j = j+3;
		    force_src = force_cxx;
		} else if (!strncmp(&command_line[j], "cxx", 3)) {
		    j = j+3;
		    force_src = force_cxx;
		    errmsg("Deprecated, use -x c++ instead.");
		} else if (!strncmp(&command_line[j], "c", 1) &&
			   ((command_line[j+1] == 0) ||
			    (command_line[j+1] == ' '))) {
		    j++;
		    force_src = force_c;
		} else {
		int k;
		char * tch;
		    k = j;
		    while(!isspace(command_line[k])&&(command_line[k] != '\0'))
		       k++;

		    tch = NULL;
		    if (k > j) {
			tch = malloc(k - j + 1);
			if (tch) {
			   strncpy(tch, &command_line[j], (k-j));
			   tch[k-j] = 0;
			}
			else
			    k = j;
		    }
		    if (tch == NULL)
			errmsg("Switch -x requires an argument");
		    else {
			errmsg("Unrecognized switch -x %s", tch);
			free(tch);
			j = k;
		    }
		}
	    }
	    else
		errmsg("Switch -x requires an argument");

	    /* Find the end of the processed command */
	    while(!isspace(command_line[j]) && (command_line[j] != '\0'))
		j++;

	    /* Blank out the processed command */
	    while( i < j) {
		command_line[i] = ' ';
		i++;
	    }
        }
	else {
	    /* Skip this command */
	    if (command_line[i] == '\'') {
		i++;
		while ((command_line[i] != '\'') && command_line[i]) {
		    i++;
		}
	    } else {
		while (!isspace(command_line[i]) && command_line[i]) {
		    i++;
		}
	    }
	}
    }


    /*
     *  Scan command line arguments
     */

    for (i = 0; i < command_line_len; i++) {
	char curr_arg[MAX_DCL_LINE_LENGTH + 1000]; /* should be big enough */
	int single_quoted;

	/* skip over leading white space */
	while(isspace(command_line[i]) && (command_line[i] != '\0'))
	   i++;

	/* Skip over a leading single quote, but set the flag */
	single_quoted = 0;
	if (command_line[i] == '\'') {
	    single_quoted = 1;
	    i++;
	}

	/*
	 * Whenever we see a hyphen, process what follows it as option text
	 * except in the special case where the hyphen is followed by space
	 * characters or nothing. In which case it should be interpreted as
	 * an input file from stdin.
	 */
	if ((command_line[i] == '-') &&
            (!isspace(command_line[i+1])) && (command_line[i+1] != '\0')) {
	    int j;
	    ccopt_t *cc_opt;
	    phase_t phase;
	    char *str_ptr;
	    char *value_ptr;
	    int add_option;
	    arglist_t *list_ptr;
	    char quote_seen;

	    i++;

	    /* HACK to make -W work as close what is expected.
	     *
	     * Only a limited number of options are passed through to
	     * all subsystems of CC/GCC.  The rest are passed through
	     * with a -W command where the letter immediately following
	     * designates which system gets them, as there could be options
	     * with the same name but different meanings.  With this wrapper
	     * no duplicates are implemented.  So the simple thing would be to
	     * ignore the -Wl or -Wc and continue parsing.
	     *
	     * Except that the -Wc and -Wl have a GNV specific function added
	     * to them where they expect a VMS parameter.
	     *
	     * So now if we find a "-Wl" or a "-Wc" followed by "," and "-",
	     * then discard it, and process the phase specific options as
	     * normal.  Otherwise let it get processed as a VMS option.
	     *
	     * Fixing this to do it right would take major a re-write of this
	     * wrapper.
	     */

	    if (!strncmp(&command_line[i], "Wl,-", 4) ||
		!strncmp(&command_line[i], "Wc,-", 4)) {
		i = i + 4;
	    }


	    /* Find the delimiters of the token */
	    quote_seen = 0;
	    if (single_quoted) {
		quote_seen = '\'';
	    }
	    j = i;
	    while (command_line[j] != 0) {
		/* May have quotes in it.
		 * DCL invocatons may have double quotes but probably
		 * not single quoted ones.
		 * Bash/make invocations could have single quoted.
		 */
		if (quote_seen) {
		    if (command_line[j] == quote_seen) {
			quote_seen = 0;
		    }
		} else {
		    if ((command_line[j] == ',') || isspace(command_line[j])) {
			break;
		    }
		    if ((command_line[j] == '\'') ||
			(command_line[j] == '\"')) {

			quote_seen = command_line[j];
		    }
		}
		j++;
	    }
	    if ((j-i) >= sizeof curr_arg) {
		errmsg
		("Buffer overflow in reading option %20s", &command_line[i]);
		i = j;
		continue;
	    }

	    /* Make a copy of the substring */
	    strncpy(curr_arg, &command_line[i], j - i);
	    curr_arg[j-i] = 0;

	    /* Remove the trailing single quote */
	    if (single_quoted) {
		int kk;
		kk = j - i - 1;
		curr_arg[kk] = 0;
	    }

	    /* Original parser assumed whitespace separates values on the
	     * command line which only works for arguments that are more
	     * than one character, so check the old way first.
	     */
	    cc_opt = lookup_arg(curr_arg);

	    /* The above could fail with single character options that
	     * take an argument because there may not be any delimiters
	     * between the character and the argument, so try again if needed.
	     */
	    if (!cc_opt && ((j-i) > 1)) {
		curr_arg[1] = '\0';
		cc_opt = lookup_arg(curr_arg);
	    }

	    if (!cc_opt) {
		errmsg("Unrecognized switch %s", curr_arg);
		continue;
	    }

	    i += strlen(cc_opt->arg_str);
	    j = i;
	    phase = cc_opt->phase;
	    switch (cc_opt->parse) {
		case X_noval:	/* Value not in command line */
		    value_ptr = NULL;
		    break;

		case X_xval:	/* Value part of argument */ /* or */
		case X_val:	/* value in next argument */
		    {
		      int k,kk;

		      /* Index to value in command_line */
		      k = i;
		      quote_seen = 0;
		      if (single_quoted) {
			  quote_seen = '\'';
		      }

		      /* skip over delimiter */
		      if (command_line[k] == ',') {
			if (curr_arg[0] != 'W') /* part of "-W" vms hack */
			  k++;
		      } else {
			while (isspace(command_line[k]) &&
			       (command_line[k] != 0)) {
			    k++;
			}
		      }

		      j = k;

		      /* ran out of input? */
		      if (command_line[k] == 0) {
			errmsg("Argument -%s requires a value", curr_arg);
			i = k;
			continue;
		      }

		      /* Find the end of the value string */
		      while((command_line[k] != 0)) {
			  /* May have quotes in it */
			  if (quote_seen) {
			      if (command_line[k] == quote_seen) {
				  quote_seen = 0;
			      }
			  } else {
			      if ((command_line[k] == ',') ||
			          isspace(command_line[k])) {
			          break;
			      }
			      if ((command_line[k] == '\'') ||
				  (command_line[k] == '\"')) {
			          quote_seen = command_line[k];
				  command_line[k] = '\"';
                              }
			  }
			  k++;
		      }

		      /* Make a copy of the substring */
		      value_ptr = malloc((k-j)+3);
		      kk = 0;
		      if (single_quoted) {
			  value_ptr[kk] = '"';
			  kk++;
		      }
		      strncpy(&value_ptr[kk], &command_line[j], k - j);
		      kk = kk + (k-j);
		      value_ptr[kk] = 0;

		      /* Replace the trailing single quote */
		      if (single_quoted) {
			  value_ptr[kk - 1] = '"';
		      }

		      i = k;
		    }
		    break;

		default:	/* Invalid value for parse */
		    errmsg
	("Parse table error: Entry for argument -%s has invalid parse type %d",
			curr_arg, cc_opt->parse);
		    value_ptr = NULL;
	    }

	    str_ptr = cc_opt->str;

	    if (!str_ptr) {
		if (cc_opt->parse == X_val)
		    errmsg("No support for switch -%s %s", curr_arg, value_ptr);
		else
		    errmsg("No support for switch -%s", curr_arg);
		continue;
	    }

	    add_option = 1;	/* Assume that if it is not a list entry */
				/* it will be added to command buffer */
	    list_ptr = NULL;

	    switch (cc_opt->rule) {
		case 0:
		    break;

		case X_lookup:
		    value_ptr = lookup_val(value_ptr, str_ptr);
		    str_ptr = NULL;
		    break;

		case X_help:
		    help = 1;
		    add_option = 0;
		    preprocess = 0;
		    linkx = 0;
		    objfile = 0;
                    break;

                case X_verbose:
                    verbose = 1;
		    version = 1;
                    add_option = 0;
		    preprocess = 0;
		    linkx = 0;
		    objfile = 0;
                    break;

		case X_version:
		    preprocess = 0;
		    linkx = 0;
		    objfile = 0;
		    version = 1;
		    break;

		case X_p_multiarch:
		    preprocess = 0;
		    linkx = 0;
		    objfile = 0;
		    version = 2;
		    break;

		case X_p_searchdirs:
		    preprocess = 0;
		    linkx = 0;
		    objfile = 0;
		    version = 3;
		    break;

		case X_makelib:		/* ??? I think -c make an archive */
		    linkx = 0;
		    add_option = 0;
		    break;

		case X_crtlsup:
		    gnv_crtl_sup = TRUE;
		    add_option = 0;
		    break;

		case X_cxx:
		    is_cxx = TRUE;
		    add_option = 0;
		    break;

		case X_preprocess:
		    preprocess = 1;
		    linkx = 0;
		    objfile = 0;
		    break;

		case X_mms:		/* -MD - Generate MMS dependency
					   file... Don't generate object */
		    preprocess = 0;
		    mms_dep_file = 1;
		    break;

		case X_mms_noobject:	/* -M - Generate MMS dependency
					   file... DO generate object */
		    preprocess = 0;
		    linkx = 0;
		    objfile = 0;
		    no_object = 1;
		    mms_dep_file = 1;
		    break;

		case X_mms_file: 	/* -MF - Generate MMS dependency file,
					   with specified filename */
		    mms_file = strdup(value_ptr);
		    add_option = 0;
		    break;

		case X_mms_nosys:	/* -MM - Generate MMS dependency file,
					   NOSYSTEM_INCLUDE_FILES */
		    mms_nosys = 1;
		    break;

		case X_noobject:
		    linkx = 0;
		    objfile = 0;
		    no_object = 1;
		    break;

		case X_retain:
		    remove_temps = FALSE;
		    add_option = 0;
		    break;

		case X_first_include:
		    list_ptr = &l_preinclude_files;
		    break;

		case X_include_dir:
		    list_ptr = &l_include_dir;
		    break;

		case X_define:
		    list_ptr = &l_define;
		    break;

		case X_undefine:
		    list_ptr = &l_undefine;
		    break;

		case X_compile_foreign:
		    list_ptr = &l_extra_compile;
		    if (*value_ptr == ',')
			*value_ptr = '/';
		    break;

		case X_link_foreign:
		    list_ptr = &l_extra_link;
		    if (*value_ptr == ',')
			*value_ptr = '/';
		    break;

		case X_show:
		    list_ptr = &l_show;
		    break;

		case X_check:
		    list_ptr = &l_check;
		    break;

		case X_optimize:
		    list_ptr = &l_optimize;
		    optimize = 1;
		    break;

		case X_debug_noopt:
		    del_list(&l_optimize);
		    optimize = 0;
		    debug = 1;
		    break;

		case X_debug:
		    debug = 1;
		    break;

		case X_nodebug:
		    debug = 0;
		    break;

		case X_output:
		    outfile = strdup(value_ptr);
		    add_option = 0;
		    break;

		case X_warn:
		    list_ptr = &l_warn;
		    break;

		case X_nowarn:
		    del_list(&l_warn);
		    break;

		case X_accept:	/* Single keywords or list of keywords */
		    list_ptr = &l_accept;
		    break;

		case X_assume:	/* Single keywords or list of keywords */
		    list_ptr = &l_assume;
		    break;

		case X_library:
		    {
			char *lib = lookup_lib(l_libdir.list, value_ptr);
			if (lib) {
			    value_ptr = lib;
			    list_ptr = &l_objfiles;
			}
			else {
			    int i;
			    for (i = 0; i < DIM(sysshr_libs); i++) {
				if (!strcasecmp(value_ptr,
                                                sysshr_libs[i].lib)) {
				    sysshr_libs[i].seen = TRUE;
				    add_option = 0;
				    break;
				}
			    }

			    if (i == DIM(sysshr_libs) ) {
				if (gnv_link_missing_lib_error) {
				    errmsg("Error: library \"%s\" not found",
                                           value_ptr);
				    exit(GNV_ERROR_STATUS);
				}
				else {
				    errmsg("Warning: library \"%s\" not found",
                                           value_ptr);
				    continue;
				}
			    }
			}

		    	break;
		    }

		case X_libdir:
		    list_ptr = &l_libdir;
		    break;

		case X_proto:
		    list_ptr = &l_proto;
		    break;

		case X_shared:
		    sharedx = 1;
		    add_option = 0;
		    break;

		case X_link_all:
		    append_last(&l_objfiles, &ld_token_all[0]);
		    break;

		case X_link_incr:
		    link_incr = 1;
		    break;

		case X_link_none:
		    append_last(&l_objfiles, &ld_token_none[0]);
		    break;

		case X_linker_symvec:
		    linker_symvecx = 1;
		    add_option = 0;
		    break;

		case X_auto_symvec:
		    auto_symvecx = 1;
		    add_option = 0;
		    break;

                case X_input_to_ld:
                    append_last(&l_optfiles,value_ptr);
                    add_option = 0;
                    break;

	    }

	    if (list_ptr) {
		value_ptr = make_opt(str_ptr, value_ptr);
	        append_last(list_ptr, value_ptr);
	    }
	    else if (add_option && (value_ptr = make_opt(str_ptr, value_ptr))) {
		switch (phase) {
		    case X_compile:
			list_ptr = &l_compile;
			break;
		    case X_linker:
		    	list_ptr = &l_linker;
			break;
		    default:
			errmsg("Parsererror - Unsupported phase %d", phase);
		}
		if (list_ptr)
		    append_arg(list_ptr, value_ptr);
	    }
        }
	else {
	    char *value_ptr;
	    int len;
	    int j;

	    /* Find the delimiters of the token */
	    j = i;

	    if (single_quoted) {
		while ((command_line[j] != '\'') && (command_line[j] != 0)) {
		    j++;
		}
	    } else {
		while(!isspace(command_line[j]) && (command_line[j] != 0)) {
		    if (command_line[j] == ',')
			break;
		    j++;
		}
	    }
	    if ((j-i) >= sizeof curr_arg) {
		errmsg
		("Buffer overflow in reading option %20s", &command_line[i]);
		i = j;
		continue;
	    }

	    /* Make a copy of the substring */
	    strncpy(curr_arg, &command_line[i], j - i);
	    curr_arg[j-i] = 0;
	    i = j;
	    if (single_quoted) {
		j++;
	    }

	    /*
	     * Check to see if we are processing stdin (i.e. user typed a
	     * standalone hyphen as part of the compilation command line).
	     * If we are processing stdin, then generate the special file
	     * name for the file that will be used to hold the contents
	     * from stdin. Then generate the file from stdin.
	     */
	    if (strcmp(curr_arg, "-")) {
	        value_ptr = strdup(curr_arg);
	    } else {
		process_stdin = 1;
		strcpy(gnv_cc_stdin_file, "gnv$cc_stdin_XXXXXX");
		mktemp(&gnv_cc_stdin_file[13]);
		if (force_src == force_c) {
		    strcat(gnv_cc_stdin_file, ".c");
		} else if (force_src == force_cxx) {
		    strcat(gnv_cc_stdin_file, ".cxx");
		} else {
		    if (preprocess) strcat(gnv_cc_stdin_file, ".c");
		}
		value_ptr = strdup(gnv_cc_stdin_file);
		generate_stdin_file(gnv_cc_stdin_file);
	    }

            len = strlen(value_ptr);

            if (test_is_library(value_ptr, len))
                append_last(&l_objfiles, value_ptr);

	    else if (test_is_shared_library(value_ptr, len))
                append_last(&l_objfiles, value_ptr);

	    else if (test_is_object(value_ptr, len))
                append_last(&l_objfiles, value_ptr);

	    else if (test_suffix(value_ptr, len, ".opt"))
                append_last(&l_optfiles, value_ptr);

	    else {
                enum suffix_type typ;

                typ = filename_suffix_type(value_ptr, len);

                if (typ == suffix_c || typ == suffix_cxx ||
                    force_src != force_none) {
                    char *bname;
		    char *dname;

		    if (!process_stdin) {
			bname = basename(strdup(value_ptr));
			dname = dirname(strdup(value_ptr));
		    } else {
			dname = strdup(".");
			if (typ == suffix_c) {
			    bname = strdup("cc_stdin.c");
			} else if (typ == suffix_cxx) {
			    bname = strdup("cc_stdin.cxx");
			} else {
			    bname = strdup("cc_stdin.");
			}
		    }

		    /* Several modules may need module specific hacks
		     * in order to build correctly on VMS.  This
		     * provides a backdoor.
		     */
		    if (!gnv_cc_no_module_first && (module_first[0]=='\0')) {
		    struct stat mf_stat;
		    int res;

			/* A gnv$ prefix is needed because Configure scripts
			 * tend to do wildcard deletes of the temporary files
			 */
			if (gnv_cc_module_first_dir) {
			    strcpy(module_first, gnv_cc_module_first_dir);
			} else {
			    strcpy(module_first, dname);
			}
			strcat(module_first, "/gnv$");
			strcat(module_first, bname);
			strcat(module_first, "_first");

			res = stat(module_first, &mf_stat);
			if (res != 0) {

			    /* Try for a global first include file. */

			    /* Not used with forced extensions */
			    if ((gnv_opt_dir) && (force_src == force_none)) {

				strcpy(module_first, gnv_opt_dir);
				strcat(module_first, "/gnv$first_include.h");
				res = stat(module_first, &mf_stat);
			    }
			}
			if (res == 0) {
			list_t new;
			    /* Put it first as it may have to use the
			     * #pragma directives to include files and
			     * they only work on the first file.
			     * Since this file is manually tweaked it
			     * can be tuned to compensate anything
			     * else that needs to be done.
			     * With 7.3-2, longer command lines make it
			     * less likely other first includes will be
			     * needed.
			     */
			    new = (list_t) malloc(sizeof(struct list));
			    new->str = strdup(module_first);
			    new->next = l_preinclude_files.list;
			    l_preinclude_files.list = new;
			    if (l_preinclude_files.last == NULL)
				l_preinclude_files.last = (list_t *) new;
			}
			else {
			    module_first[0] = '\0';
			}
		    }

                    if (force_src == force_c)
                        append_last(&l_cfiles, value_ptr);

                    else if (force_src == force_cxx || typ == suffix_cxx )
                        append_last(&l_cxxfiles, value_ptr);

                    else if (typ == suffix_c)
                        append_last(&l_cfiles, value_ptr);

                    else
                        assert(0); /* should not happen */

		    /*
		     * Generate the obj module name using the actual stdin
		     * file when processing stdin.
		     */
		    if (process_stdin) {
			free(bname);
			bname = basename(strdup(gnv_cc_stdin_file));
		    }
                    append_last(&l_objfiles,
                                new_suffix3(bname, default_obj_suffix));
		    free(dname);
		    free(bname);
                }
		else {
		    errmsg("Unrecognized file %s", value_ptr);
		    free(value_ptr);
		}
            }
        }
    }

    /*
     *  Check things controllable through environment variables after
     *  processing command line in case it has been overridden.
     */

    if (gnv_cc_debug)
        remove_temps = FALSE;

    /*
     * If there are any special defaults apply them now (note that -V implies
     * no compiling and therefore no need for the defaults).
     */

    if (!version) {
        if (gnv_cc_warn_info_all)
            append_last(&l_warn, "INFO=ALL");

        if (!gnv_cc_no_posix_exit)
            append_last(&l_define, "_POSIX_EXIT");

	if (gnv_cc_main_posix_exit)
	    append_last(&l_extra_compile, "/MAIN=POSIX_EXIT");

        if (!gnv_cc_no_use_std_stat)
            append_last(&l_define, "_USE_STD_STAT");
        if (!gnv_cxx_no_use_std_iostream)
            append_last(&l_define, "__USE_STD_IOSTREAM");
        if (gnv_cc_sockaddr_len)
            append_last(&l_define, "_SOCKADDR_LEN");
    }

    /*
     * If using the CRTL supplemental library, you need the includes
     * and the library.
     */

    if (gnv_crtl_sup) {
        char *lib;

        append_last(&l_include_dir, "/gnu/include");
        append_last(&l_libdir, "/gnu/lib");

        lib = lookup_lib(l_libdir.list, "crtl_sup");

        if (!lib)
            errmsg("Warning: Supplementary C RTL library %s not found",
                   "crtl_sup");
        else
            append_last(&l_objfiles, lib);
    }

    /* Give us one last chance to override the build scripts for VMS */
    if (is_cxx) {
	if (gnv_cxx_qualifiers)
	    append_last(&l_extra_compile, gnv_cxx_qualifiers);
	if (gnv_cxxlink_qualifiers)
	    append_last(&l_extra_link, gnv_cxxlink_qualifiers);
    }
    else {
	if (gnv_cc_qualifiers)
	    append_last(&l_extra_compile, gnv_cc_qualifiers);
	if (gnv_link_qualifiers)
	    append_last(&l_extra_link, gnv_link_qualifiers);
    }
    if (help) {
        printf("GNV %s %s\n", __DATE__, __TIME__);
	output_help();

	return 0;
    }

    if (verbose || version) {
        char * argv0_copy;
        switch (version) {
        case 2:
           /* New multiarch string support hacked on to version processing */
#ifdef __vax
           puts("vax-vms-hp");
#else
#ifdef __alpha
           puts("alpha-vms-hp");
#else
#ifdef __ia64
           puts("ia64-vms-hp");
#else
           puts("unknown-vms-hp");
#endif /* __ia64 */
#endif /* __alpha */
#endif /* __vax */
	   return 0;
        case 3:
           /* Print search dirs debugging support */
           /* TODO: Actually look up the values that are used. */
           argv0_copy = dirname(argv[0]);
           printf("install: %s\n", argv0_copy);
           printf("programs: %s\n", argv0_copy);
           printf("libraries: %s\n", "/usr/lib:/lib:/sys$library");
           return 0;
        default:
            printf("GNV %s %s-%s %s %s\n", actual_name,
                   PACKAGE_VERSION, VMS_ECO_LEVEL, __DATE__, __TIME__);
        }
    }

    if (!version && !l_cfiles.list &&  !l_cxxfiles.list && !linkx) {
	/*
	** Nothing to do
	*/
	return 0;
    }

    sprintf(cmd_proc_name, "cc_tempXXXXXX");

    cmd_proc_name_ptr = tmpnam(cmd_proc_name);

    /* Prefer to open in record context */
    cmd_proc = fopen(cmd_proc_name, "w", "rfm=var", "rat=cr");

    /* Pick up the real filename */
    cmd_proc_name_ptr = fgetname (cmd_proc, cmd_proc_name);


    if (gnv_cc_debug)
	fprintf(cmd_proc, "$verify = 'f$verify(1)\n");

    fprintf(cmd_proc, "$on e then goto exit\n");
    fprintf(cmd_proc, "$set mess /faci/iden/seve/text\n");

#ifndef __VAX
#ifdef PPROP$C_TOKEN
    fprintf(cmd_proc, "$set process/token=extended\n");
#endif
#ifdef PPROP$C_PARSE_STYLE_TEMP
    fprintf(cmd_proc, "$set process/parse_style=extended\n");
#endif
#endif

    if (version && !l_cxxfiles.list && !l_cfiles.list && !l_objfiles.list &&
        (is_cxx || is_cc))
        do_compile(cmd_proc, NULL, is_cxx);

    if (l_cfiles.list)
        do_compile(cmd_proc, l_cfiles.list, FALSE);

    if (l_cxxfiles.list)
        do_compile(cmd_proc, l_cxxfiles.list, TRUE);

    object_opt_name_ptr = NULL;
    if (linkx) {
	/*
	** If environment variable  GNV_LINK_DEBUG is enabled and
	** compiled with /DEBUG then use LINK with /DEBUG.
	*/
	if (debug && gnv_link_debug)
	    debug_link = TRUE;

        if (list_size(&l_objfiles) > gnv_ld_object_length_max) {
	    object_opt_name_ptr = tmpnam(object_opt_name);

	    /* Prefer to open in record context */
	    object_opt = fopen(object_opt_name, "w", "rfm=var", "rat=cr");

	    /* Pick up the real filename */
	    object_opt_name_ptr = fgetname (object_opt, object_opt_name);

        } else {
            object_opt = cmd_proc;
        }

        do_link(cmd_proc, object_opt, object_opt_name_ptr);
    }

    if (gnv_cc_debug)
	fprintf(cmd_proc, "$exit:\n$exit '$STATUS' + (0*f$verify(verify)')\n");
    else
	fprintf(cmd_proc, "$exit:\n$exit\n");

    fclose(cmd_proc);
    if (object_opt_name_ptr != NULL) {
        fclose(object_opt);
    }

    sprintf (cmd, "$@%s", unix_to_vms(cmd_proc_name, FALSE));

    fp = popen(cmd, "r");

    if (!fp) {
        perror("? popen");
        exit(EXIT_FAILURE);
    }


    /*
    ** Output goes to stderr as well as stdout if they are different.
    */
    out_is_err = stdout_is_stderr();
    fgets(buf, sizeof(buf), fp);
    while (!feof(fp) ) {
        int len = strlen(buf);
	if (!gnv_link_no_undef_error && !seen_undef) {
	    if (strncmp(buf,"%LINK-W-NUDFSYMS",16) == 0) {
		seen_undef = 1;
		buf[6] = 'E';
	    }
	    if (strncmp(buf, "%ILINK-W-NUDFSYMS",17) == 0) {
		seen_undef = 1;
		buf[7] = 'E';
	    }
	}
        fputs(buf, stdout);
	if (!out_is_err)
            fputs(buf, stderr);
        fgets(buf, sizeof(buf), fp);
    }

    status = pclose(fp);
    if (status == -1) {
        perror("? pclose");
        exit(EXIT_FAILURE);
    }

    /*
     * Remove the special source file used for processing stdin as well as the
     * associated object module.
     */
    if (process_stdin) {
	char *lastdot;
	char *newfile;
	int file_len;
	file_len = strlen(gnv_cc_stdin_file);
	newfile = malloc(file_len + 10);
	strcpy(newfile, gnv_cc_stdin_file);
	lastdot = strrchr(newfile, '.');
	remove(gnv_cc_stdin_file);
	if (lastdot != NULL) {
	    if (linkx) {
		strcpy(lastdot, default_obj_suffix);
		remove(gnv_cc_stdin_file);
	    } else {
		char obj_name[1024];
		obj_name[0] = 'a';
		obj_name[1] = '\0';
		strcat(obj_name, default_obj_suffix);
		strcpy(lastdot, default_obj_suffix);
		rename(gnv_cc_stdin_file, obj_name);
	    }
	}
    }

    if (preprocess)
        output_file(stdout, prep_file_name, remove_temps);

    /*
     *  Must remove cmd_proc last since it is the name
     *  which other temps are based on.
     */
    if (remove_temps) {
        remove(cmd_proc_name);
	if (object_opt_name_ptr != NULL) {
	    remove(object_opt_name_ptr);
	}
	if (define_file != NULL) {
	    remove(define_file);
	}
    }

    /*
     *  If MMS dependency file is generated, we need to do a
     *  post-processing phase, to translate VMS to Unix style
     *  filespecs.
     */
    if (mms_dep_file) postProcessMMS ();

    /*
    ** If this was a compile only, and GNV_CC_WARN_INFO is defined,
    ** then downgrade any warning status to informational.
    */
    if (!linkx && gnv_cc_warn_info) {
	if ($VMS_STATUS_SEVERITY(status) == STS$K_WARNING)
	    status = status & ~STS$M_SEVERITY | STS$K_INFO;
    }

    /*
    ** If we saw an "undefined sysmbol" error fly by and the status is only
    ** WARNING, then force an ERROR status. Note that we would only set the
    ** undefined flag if the user had NOT set GNV_LINK_NO_UNDEF_ERROR.
    */
    if (seen_undef) {
	if ($VMS_STATUS_SEVERITY(status) == STS$K_WARNING)
	    status = status & ~STS$M_SEVERITY | STS$K_ERROR;
    }

    test_status_and_exit(status);

    return 0;
}
