/*
 ********************************************************************
 * CCOPT.H                                                          *
 * Arguments accepted by the cc driver and the rules for            *
 * converting them. Arguments are case sensitive.                   *
 *                                                                  *
 *   noval  - argument does not take a value		            *
 *   xval   - value is part of switch   -Wextra	                    *
 *   val    - Value is in next argument -o outfile (space optional) *
 *   Zero in last 2 columns means switch is not supported           *
 ********************************************************************
 *
 * 06-Oct-2004	Steve Pitcher	Add --version... identical to -V.
 *
 */

/*
 * Miscellaneuos control options
 */

/* Parameters:
      option with out leading "-"
      phase - control = general command setup
              compile = compiler option
              profiler = profiler option
              linker = linker option
      parse - As above.
      rule  - internal rule.
      vms_opt - VMS option string if applicable.
      support - Support status
              - active: Supported command.
              - deprec: Not a real cc/ld/cxx command -warn.
              - conflict: deprec, plus conflict with real options. - warn
              - badimp: Not implmented as expected - warn.
              - unknown: No idea why this is here.
              - unsup: Unsupported on VMS need to warn.
              - ignore: Silently ignore that we do not do this.
       These warnings do not change the command exit status.

     It appears that some GCC options in use are not showing up
     in obvious documentation, so care should be used in removing code.
  */



_X_ARG( "c"			, control, noval, makelib	, "" , active ),
_X_ARG( "crtlsup"		, control, noval, crtlsup	, "" , deprec ),
_X_ARG( "cxx"                   , control, noval, cxx           , "" , deprec ),
_X_ARG( "-help"			, control, noval, help		, "" , active ),
_X_ARG( "h"			, control, noval, help		, "" , deprec ),
_X_ARG( "K"	 		, control, noval, retain	, 0  , deprec ),
_X_ARG( "o"			, control, val	, output	, "" , active ),
_X_ARG( "-print-multiarch"      , control, noval, p_multiarch   , "" , active ),
_X_ARG( "Q"		        , control, noval, help_opt      , 0  , unsup ),
_X_ARG( "spike"			, control, noval, 0		, 0  , deprec ),
_X_ARG( "v"  		        , control, noval, verbose       , 0  , active ),
_X_ARG( "x"			, control, val	, compile_type	, "" , ignore ),

/*
 * Compiler options
 */

_X_ARG( "B"		, compile, val	, bin_prefix , 0            , ignore ),
_X_ARG( "C"		, compile, noval, 0	     , "/COMM=AS_IS", active ),
_X_ARG( "D"		, compile, val	, define     , ""           , active ),
_X_ARG( "E"		, compile, noval, preprocess , "/PREP"      , active ),
_X_ARG( "FI"		, compile, xval	, first_include, ""         , deprec ),
_X_ARG( "Hc"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "I"		, compile, val	, include_dir , ""          , active ),
_X_ARG( "MD"		, compile, noval, mms	      , ""          , badimp ),
_X_ARG( "M"		, compile, noval, mms_noobject, ""          , badimp ),
_X_ARG( "MF"		, compile, val  , mms_file    , ""          , badimp ),
_X_ARG( "MM"		, compile, noval, mms_nosys   , ""          , badimp ),
_X_ARG( "O"		, compile, noval, optimize    ,
                                  "INL=AUTO,LEV=4,UNR=0,TUN=GENE"   , active ),
_X_ARG( "O0"		, compile, noval, nooptimize  , "/NOOPT"    , active ),
_X_ARG( "O1"		, compile, noval, optimize    , "LEV=1"     , active ),
_X_ARG( "O2"		, compile, noval, optimize    , "LEV=3"     , active ),
_X_ARG( "O3"		, compile, noval, optimize    , "LEV=4"     , active ),
_X_ARG( "O4"		, compile, noval, optimize    , "LEV=5"     , active ),
_X_ARG( "P"		, compile, noval, preprocess  , "/NOLINE"   , active ),
_X_ARG( "S"		, compile, noval, assemblero  , 0           , ignore ),
_X_ARG( "SD"		, compile, xval	, 0	      , 0           , unknown ),
_X_ARG( "-version"	, compile, noval, version     , "/VER"      , active ),
_X_ARG( "V"  		, compile, noval, version     , "/VER"      , deprec ),
_X_ARG( "W"		, compile, xval	, 0	      , 0           , ignore ),
_X_ARG( "Wc"		, compile, xval	, compile_foreign, ""       , conflict),
_X_ARG( "Zp"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "Zp1"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "Zp2"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "Zp4"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "Zp8"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "accept" 	, compile, val	, accept      , ""          , deprec ),
_X_ARG( "ansi_alias"	, compile, noval, 0	      , "/ANS"      , deprec ),
_X_ARG( "ansi_args"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "arch" 		, compile, val	, 0	      , "/ARC"      , deprec ),
_X_ARG( "assume" 	, compile, val	, assume      , ""          , deprec ),
_X_ARG( "c99"		, compile, noval, 0	      , "/STAN=C99" , deprec ),
_X_ARG( "check"  	, compile, warn	, 0	      , "ENA=LEVEL5", deprec ),
_X_ARG( "check_bounds"	, compile, noval, check	      , "BOU"       , deprec ),
_X_ARG( "check_omp"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "common"	, compile, noval, 0	      , "/STAN=COM" , deprec ),
_X_ARG( "double" 	, compile, noval, 0	      , "/PREC=DOU" , deprec ),
_X_ARG( "error_limit" 	, compile, val	, 0	      , "/ERR"      , deprec ),
_X_ARG( "fast"		, compile, noval, optimize    , "/OPT"      , deprec ),
_X_ARG( "feedback" 	, compile, val	, 0	      , 0           , unknown ),
_X_ARG( "float"		, compile, noval, 0	      , "/PREC=SIN" , deprec ),
_X_ARG( "float_const"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "fp_reorder"   	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "fprm" 		, compile, val	, lookup,
                            "|c=/ROU=C|d=/ROU=D|m=/ROU=M|n=/ROU=N"  , deprec ),
_X_ARG( "fptm" 		, compile, val	, lookup,
                            "|n=/IEEE=FAS|u=/IEEE=INEX"             , deprec ),
_X_ARG( "framepointer"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "g"		, compile, noval, debug_noopt ,
                                     "/DEB=ALL/NOOPT"               , active ),
_X_ARG( "g0"		, compile, noval, nodebug     , "/NODEB"    , deprec ),
_X_ARG( "g1"		, compile, noval, debug	      ,
                                     "/DEB=(NOSYM,TRA)"             , deprec ),
_X_ARG( "g2"		, compile, noval, debug	      ,
                                     "/DEB=ALL/NOOPT"               , deprec ),
_X_ARG( "g3"		, compile, noval, debug	      , "/DEB=ALL"  , deprec ),
_X_ARG( "gen_feedback"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "granularity"	, compile, val	, 0	      , "/GRA"      , deprec ),
_X_ARG( "hpath"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "ieee"		, compile, noval, 0	      , "/FL=IEEE"  , deprec ),
_X_ARG( "ifo"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "include"	, compile, val	, first_include	, ""        , active ),
_X_ARG( "inline"	, compile, val	, optimize    , "INL"       , deprec ),
_X_ARG( "intrinsics"  	, compile, noval, optimize    , "INTR"      , deprec ),
_X_ARG( "isoc94"	, compile, noval, 0	      ,
                                     "/STAN=ISOC94"                 , deprec ),
_X_ARG( "machine_code"	, compile, noval, 0	      , "/MAC"      , deprec ),
_X_ARG( "member_alignment" , compile, noval, 0	      , "/MEM"      , deprec ),
_X_ARG( "misalign"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "mp"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "ms" 		, compile, noval, 0	      , "/STAN=MS"  , deprec ),
_X_ARG( "msg_always"	, compile, val	, warn	      , "EMIT_A"    , deprec ),
_X_ARG( "msg_disable"	, compile, val	, warn	      , "DIS"       , deprec ),
_X_ARG( "msg_dump"	, compile, val	, warn	      , 0 	    , unknown ),
_X_ARG( "msg_enable"	, compile, val	, warn	      , "ENA"       , deprec ),
_X_ARG( "msg_error"	, compile, val	, warn	      , "ERR"       , deprec ),
_X_ARG( "msg_fatal"	, compile, val	, warn	      , "FATAL"     , deprec ),
_X_ARG( "msg_inform"	, compile, val	, warn	      , "INFO"      , deprec ),
_X_ARG( "msg_once"	, compile, val	, warn	      , "EMIT_O"    , deprec ),
_X_ARG( "msg_warn"	, compile, val	, warn	      , "WARN"      , deprec ),
_X_ARG( "names_as_is_short", compile, noval, 0        ,
                                     "/NAMES=(AS_IS,SHORT)"         , deprec ),
_X_ARG( "nestlevel="	, compile, xval	, 0	      , 0           , unknown ),
_X_ARG( "noansi_alias"	, compile, noval, 0	      , "/NOANS"    , deprec ),
_X_ARG( "noansi_args"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "nocheck_bounds", compile, noval, check	      , "NOBOU"     , deprec ),
_X_ARG( "nocurrent_include", compile, noval, 0	      , 0           , unknown ),
_X_ARG( "noerror_limit"	, compile, noval, 0	      , "/NOERR"    , deprec ),
_X_ARG( "nofp_reorder"  , compile, noval, 0	      , 0           , unknown ),
_X_ARG( "noinline" 	, compile, val	, optimize    , "NOINL"     , deprec ),
_X_ARG( "nointrinsics"  , compile, noval, optimize    , "NOINT"     , deprec ),
_X_ARG( "nomember_alignment", compile, noval, 0	      , "/MEM"      , deprec ),
_X_ARG( "nomisalign"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "noobject"	, compile, noval, noobject    , "/NOOBJ"    , deprec ),
_X_ARG( "nopg"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "nopids"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "oldcomment"	, compile, noval, 0	      , "/NOCOM"    , deprec ),
_X_ARG( "om"   		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "omp"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "pedantic"      , compile, noval, 0           , ""          , ignore ),
_X_ARG( "pg"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "pids"		, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "portable"	, compile, noval, warn	      , "ENA=PORT"  , deprec ),
_X_ARG( "preempt_module", compile, noval, 0	      , 0           , unknown ),
_X_ARG( "preempt_symbol", compile, noval, 0	      , 0           , unknown ),
_X_ARG( "prof_dir"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "prof_gen"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "prof_gen_noopt", compile, noval, 0	      , 0           , unknown ),
_X_ARG( "prof_use_feedback", compile, noval, 0	      , 0           , unknown ),
_X_ARG( "prof_use_om_feedback", compile, noval, 0     , 0           , unknown ),
_X_ARG( "protect_headers", compile, val , 0	      , 0           , unknown ),
_X_ARG( "protoi"	, compile, noval, proto	      , "ID"        , deprec ),
_X_ARG( "protos"	, compile, noval, proto	      , "ST"        , deprec ),
_X_ARG( "readonly_strings", compile, noval, assume    , "NOWRI"     , deprec ),
_X_ARG( "scope_safe"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "show"		, compile, val	, show	      , ""          , deprec ),
_X_ARG( "signed"	, compile, noval, 0	      , "/NOUNS"    , deprec ),
_X_ARG( "source_listing", compile, noval, 0	      , "/LIS"      , deprec ),
_X_ARG( "speculate" 	, compile, val	, 0	      , 0           , unknown ),
_X_ARG( "std"		, compile, noval, 0	      , "/STAN=REL" , deprec ),
_X_ARG( "std0" 		, compile, noval, 0	      , "/STAN=COM" , deprec ),
_X_ARG( "std1"		, compile, noval, 0	    , "/STAN=ANSI89", deprec ),
_X_ARG( "strong_volatile", compile, noval, assume     , "NOWEA"     , deprec ),
_X_ARG( "tc"		, compile, xval	, 0	      , 0           , unknown ),
_X_ARG( "traditional"	, compile, noval, 0	      , "/STAN=COM" , deprec ),
_X_ARG( "trapuv"	, compile, noval, check	      , "UNI"       , deprec ),
_X_ARG( "tune" 		, compile, val	, optimize    , "TUN"       , deprec ),
_X_ARG( "unroll" 	, compile, val	, optimize    , "UNR"       , deprec ),
_X_ARG( "unsigned"	, compile, noval, 0	      , "/UNS"      , deprec ),
_X_ARG( "varargs"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "vaxc"		, compile, noval, 0	      , "/STAN=VAXC", deprec ),
_X_ARG( "verbose"	, compile, noval, warn	      , "VERB"      , deprec),
_X_ARG( "volatile"	, compile, noval, 0	      , 0           , unknown),
_X_ARG( "w"		, compile, noval, warn	      , "ENA=LEVEL3", deprec ),
_X_ARG( "w0"		, compile, noval, warn	      , "ENA=LEVEL3", deprec ),
_X_ARG( "w1"		, compile, noval, nowarn      , "/NOWAR"    , deprec ),
_X_ARG( "w2"		, compile, noval, warn	      , "FAT=LEVEL3", deprec ),
_X_ARG( "w3"		, compile, noval, warn,
                                     "DIS=ALL,FAT=LEVEL3"           , deprec ),
_X_ARG( "warnprotos"	, compile, noval, 0	      , 0           , unknown ),
_X_ARG( "weak_volatile"	, compile, noval, assume      , "WEA"       , deprec ),
_X_ARG( "writable_strings", compile, noval, assume    , "WRI"       , deprec ),
_X_ARG( "xtaso"		, compile, noval, 0	      , "/POI=LON"  , deprec ),
_X_ARG( "xtaso_short"	, compile, noval, 0	      , "/POI=SHO"  , deprec ),

/*
 * Profiling
 */

_X_ARG( "p0"		, profile, noval, 0	      , 0           , deprec ),
_X_ARG( "p1"		, profile, noval, 0	      , 0           , deprec ),

/*
 * Linker arguments
 */

_X_ARG( "L"		, linker,  val	, libdir      , ""          , active ),
_X_ARG( "U"		, linker,  xval	, undefine    , ""          , unknown ),
_X_ARG( "WL"		, linker,  xval	, 0	      , 0           , unknown ),
_X_ARG( "Wl"		, linker,  val	, link_foreign, ""          , conflict),
_X_ARG( "all"		, linker,  noval, link_all    , ""          , deprec ),
_X_ARG( "auto_symvec"   , linker,  noval, auto_symvec , ""          , deprec ),
_X_ARG( "call_shared"   , linker,  noval, 0           , "/SYSSHR"   , deprec ),
_X_ARG( "check_registry", linker,  val	, 0           , 0           , unknown ),
_X_ARG( "compress"	, linker,  noval, 0	      , 0           , unknown ),
_X_ARG( "cord"		, linker,  noval, 0	      , 0           , unknown ),
_X_ARG( "exact_version" , linker,  noval, 0	      , 0           , unknown ),
_X_ARG( "expect_unresolved", linker,  val, 0	      , 0           , unknown ),
_X_ARG( "fini" 		, linker,  val	, 0	      , 0           , unknown ),
_X_ARG( "i"		, linker,  noval, link_incr   , ""          , active ),
_X_ARG( "init"		, linker,  val	, 0	      , 0           , unknown ),
_X_ARG( "input_to_ld"	, linker,  val	, input_to_ld , "/OPT"      , deprec ),
_X_ARG( "l"		, linker,  val	, library     , ""          , active ),
_X_ARG( "auto_linker_symvec", linker, noval,linker_symvec, ""       , deprec ),
_X_ARG( "noarchive"     , linker,  noval, 0           , "/NOSYSLIB" , deprec ),
_X_ARG( "non_shared"	, linker,  noval, 0	      , "/NOSYSSHR" , deprec ),
_X_ARG( "none"		, linker,  noval, link_none   , ""          , deprec ),
_X_ARG( "pthread"	, linker,  noval, 0	 , "/THREADS_ENABLE", deprec ),
_X_ARG( "r"		, linker,  noval, link_incr   , ""          , active ),
_X_ARG( "-relocatable"	, linker,  noval, link_incr   , ""          , active ),
_X_ARG( "rpath"		, linker,  val	, rpath	      , 0           , active ),
_X_ARG( "set_version" 	, linker,  val	, 0	      , 0           , unknown ),
_X_ARG( "shared"	, linker,  noval, shared      , "/SHARE"    , deprec ),
_X_ARG( "soname" 	, linker,  val	, 0	      , 0           , unknown ),
_X_ARG( "taso"		, linker,  noval, 0	      , 0           , unknown ),
_X_ARG( "threads"	, linker,  noval, 0	, "/THREADS_ENABLE" , deprec ),
_X_ARG( "update_registry", linker,  val	, 0	      , 0           , unknown),
