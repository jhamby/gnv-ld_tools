$! File: test_ld_tools.com
$!
$! Use test_cc foreign command to prevent recursion
$!--------------------------------------------------
$ if f$search("gnv$gnu:[bin]cc.exe") .eqs. ""
$ then
$   ! Acctual installed location
$   test_cc := $gnv$gnu:[usr.bin]cc.exe
$   ld := $gnv$gnu:[usr.bin]ld.exe
$ else
$   ! Staged location is preferred
$   test_cc := $gnv$gnu:[bin]cc.exe
$   ld := $gnv$gnu:[bin]ld.exe
$ endif
$! test_cc := $sys$disk:[]gnv$debug-ld.exe
$!
$! Initialize counts.
$ fail = 0
$ test = 0
$ pid = f$getjpi("", "pid")
$!
$! Start with testing the version
$!--------------------------------
$ gosub version_test
$!
$! Then multi-arch
$ gosub print_multiarch_test
$!
$!
$! Simple CC tests
$!---------------
$ gosub simple_compile
$ gosub compile_root_log_dash_c_dash_o
$ gosub compile_log_dash_c_dash_o
$!
$!
$! flag tests
$!---------------
$ gosub wextra_compile
$ gosub pedantic_compile
$!
$! Quoted define
$!----------------
$ gosub quoted_define
$ gosub quoted_define2
$ gosub numeric_define
$!
$! Missing objects/images
$ gosub missing_objects
$ gosub missing_shared_images
$ gosub missing_library
$!
$! Command file helpers
$ gosub cc_command_file
$!
$! Special GNV shared logical
$ gosub ld_gnv_shared_logical
$ gosub ld_gnv_shared_logical2
$!
$! Sumarize the run
$!---------------------
$ gosub test_clean_up
$ write sys$output "''test' tests run."
$ if fail .ne. 0
$ then
$    write sys$output "''fail' tests failed!"
$ else
$    write sys$output "All tests passed!"
$ endif
$!
$!
$exit
$!
$!
$! Basic Version test
$!------------------------
$version_test:
$!
$ test = test + 1
$ lcl_fail = 0
$ expect_cc_status = 1
$ out_file = "sys$scratch:test_cc_version.out"
$ obj_file = "a.out"
$ cflags = "--version"
$ expected_cc_output = "GNV ld"
$ cmd = "ld"
$ cc_out_file = ""
$ gosub version_test_driver
$ return
$!
$! --print-multiarch test
$!------------------------
$print_multiarch_test:
$!
$ test = test + 1
$ lcl_fail = 0
$ expect_cc_status = 1
$ out_file = "sys$scratch:test_cc_version.out"
$ obj_file = "a.out"
$ cflags = "--print-multiarch"
$ arch_type = f$getsyi("ARCH_NAME")
$ arch_code = f$extract(0, 1, arch_type)
$ expected_cc_output = "unknown-vms-hp"
$ if arch_code .eqs. "V" then expected_cc_output = "vax-vms-hp"
$ if arch_code .eqs. "A" then expected_cc_output = "alpha-vms-hp"
$ if arch_code .eqs. "I" then expected_cc_output = "ia64-vms-hp"
$ cc_out_file = ""
$ gosub version_test_driver
$ return
$
$! Driver for compile commands that do not produce an object
$!----------------------------------------------------------
$version_test_driver:
$ if f$search(out_file) .nes. "" then delete 'out_file';*
$ if f$search(obj_file) .nes. "" then delete 'obj_file';*
$ define/user sys$output 'out_file'
$ 'cmd' 'cflags'
$ cc_status = '$status'
$ if cc_status .ne. expect_cc_status
$ then
$   write sys$output "CC status was not ''expect_cc_status'!"
$   lcl_fail = lcl_fail + 1
$ endif
$ if f$search(out_file) .nes. ""
$ then
$   open/read xx 'out_file'
$   read xx line_in
$   close xx
$   if f$locate(expected_cc_output, line_in) .ne. 0
$   then
$       write sys$output "''expected_cc_output' not found in output"
$       lcl_fail = lcl_fail + 1
$   endif
$   delete 'out_file';*
$ else
$    write sys$output "Output file not created";
$    lcl_fail = lcl_fail + 1
$ endif
$ if f$search(obj_file) .nes. ""
$ then
$    write sys$output "An object file was created and should not have been!"
$    lcl_fail = lcl_fail + 1
$    delete 'obj_file';*
$ endif
$ if lcl_fail .ne. 0 then fail = fail + 1
$ return
$!
$!
$simple_compile:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = ""
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "Hello World"
$ cc_out_file = ""
$ gosub compile_driver
$ return
$!
$!
$!
$! Use a concealed logical root for device name
$! assume single level directory.
$!
$compile_root_log_dash_c_dash_o:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "test_dash_o.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ my_default = f$environment("default")
$ my_dev = f$parse(my_default,,,"device")
$ my_dir = f$parse(my_default,,,"directory") - "[" - "]" - "<" - ">"
$ test_root = "test_''pid'_root"
$ cflags = "-c -o /''test_root'/'my_dir'/''efile'"
$ noexe = 1
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "Hello World"
$ cc_out_file = ""
$ define/trans=conc 'test_root' 'my_dev'
$ gosub compile_driver
$ deas 'test_root'
$ return
$!
$!
$! Use a concealed logical root for device name
$! assume single level directory.
$!
$compile_log_dash_c_dash_o:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "test_dash_o.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ my_default = f$environment("default")
$ test_log = "test_''pid'_log"
$ cflags = "-c -o /''test_log'/''efile'"
$ noexe = 1
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "Hello World"
$ cc_out_file = ""
$ define 'test_log' 'my_default'
$ gosub compile_driver
$ deas 'test_log'
$ return
$!
$!
$wextra_compile:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-Wextra"
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "Hello World"
$ cc_out_file = ""
$ gosub compile_driver
$ return
$!
$pedantic_compile:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-pedantic"
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "Hello World"
$ cc_out_file = ""
$ gosub compile_driver
$ return
$!
$!
$quoted_define:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-DTEST_DEFINE=""foo bar"""
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "foo bar"
$ cc_out_file = ""
$ gosub compile_driver
$ return
$!
$!
$!
$quoted_define2:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-DNDEBUG -DPy_BUILD_CORE -DABIFLAGS=""M"""
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "flags=M"
$ cc_out_file = ""
$ gosub compile_driver
$ return
$!
$!
$numeric_define:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-DTest_NDEFINE=0x03500"
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "13568"
$ cc_out_file = ""
$ gosub compile_driver
$ return
$!
$!
$missing_objects:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ more_files = "missing.o"
$ clfags = ""
$ expect_prog_status = 1
$ expect_cc_status = %x35a011
$ out_file = "test_cc_prog.out"
$ expect_prog_out = ""
$ cc_out_file = ""
$ write sys$output "**** Expecting LINK/DCL errors and warnings below."
$ gosub compile_driver
$ write sys$output "**** Expected LINK/DCL errors and warnings above."
$ return
$!
$missing_shared_images:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ more_files = "missing.so"
$ clfags = ""
$ expect_prog_status = 1
$ expect_cc_status = %x35a011
$ out_file = "test_cc_prog.out"
$ expect_prog_out = ""
$ cc_out_file = ""
$ write sys$output "**** Expecting LINK/DCL errors and warnings below."
$ gosub compile_driver
$ write sys$output "**** Expected LINK/DCL errors and warnings above."
$ return
$!
$missing_library:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ more_files = "missing.a"
$ clfags = ""
$ expect_prog_status = 1
$ expect_cc_status = %x35a011
$ out_file = "test_cc_prog.out"
$ expect_prog_out = ""
$ cc_out_file = ""
$ write sys$output "**** Expecting LINK/DCL errors and warnings below."
$ gosub compile_driver
$ write sys$output "**** Expected LINK/DCL errors and warnings above."
$ return
$!
$!
$cc_command_file:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "test_hello.o"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ more_files = ""
$ cflags = "-o test_hello.o"
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ cc_out_file = "test_cc.out"
$ expect_cc_out = "test gnv$test_hello_cc.com"
$ cc_cmd_file = "gnv$test_hello_cc.com"
$ if f$search(cc_cmd_file) .nes. "" then delete 'cc_cmd_file';*
$ create 'cc_cmd_file'
$DECK
$ write sys$output "test gnv$test_hello_cc.com"
$EOD
$ gosub compile_driver
$ if f$search(cc_cmd_file) .nes. "" then delete 'cc_cmd_file';*
$ return
$!
$!
$ld_gnv_shared_logical:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "test_hello.exe"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ more_files = ""
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "Hello World"
$ cc_out_file = "test_cc.out"
$ expect_cc_out = ""
$ cc/object=test_hello_shr.o test_hello.c
$ link/share=test_hello_shr.exe test_hello_shr.o
$ define gnv$libtest_hello_shr sys$disk:[]test_hello_shr.exe
$ cflags = "-o test_hello.exe -ltest_hello_shr"
$ gosub compile_driver
$ exe_shr1 = file + "_shr.exe"
$ if f$search(exe_shr1) .nes. "" then delete 'exe_shr1';*
$ map_shr1 = file + "_shr.map"
$ if f$search(map_shr1) .nes. "" then delete 'map_shr1';*
$ dsf_shr1 = file + "_.dsf"
$ if f$search(dsf_shr1) .nes. "" then delete 'dsf_shr1';*
$ deassign gnv$libtest_hello_shr
$ return
$!
$ld_gnv_shared_logical2:
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ file = "test_hello"
$ cfile = file + ".c"
$ efile = "test_hello.exe"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ more_files = ""
$ expect_prog_status = 1
$ expect_cc_status = 1
$ out_file = "test_cc_prog.out"
$ expect_prog_out = "Hello World"
$ cc_out_file = "test_cc.out"
$ expect_cc_out = ""
$ cc/object=test_hello_shr.o test_hello.c
$ link/share=test_hello_shr.exe test_hello_shr.o
$ define gnv$libtest_hello_shr sys$disk:[]test_hello_shr.exe
$ cflags = "-o test_hello.exe libtest_hello_shr.a"
$ gosub compile_driver
$ exe_shr1 = file + "_shr.exe"
$ if f$search(exe_shr1) .nes. "" then delete 'exe_shr1';*
$ map_shr1 = file + "_shr.map"
$ if f$search(map_shr1) .nes. "" then delete 'map_shr1';*
$ dsf_shr1 = file + "_.dsf"
$ if f$search(dsf_shr1) .nes. "" then delete 'dsf_shr1';*
$ deassign gnv$libtest_hello_shr
$ return
$!
$! Generic Compiler driver
$!-------------------------
$compile_driver:
$ if f$type(noexe) .eqs. "" then noexe = 0
$ if cc_out_file .nes. "" then define/user sys$output 'cc_out_file'
$ test_cc 'cflags' 'cfile' 'more_files'
$ cc_status = '$status' .and. (.not. %x10000000)
$ if cc_status .ne. expect_cc_status
$ then
$   write sys$output "CC status ''cc_status' is not ''expect_cc_status'!"
$   lcl_fail = lcl_fail + 1
$ endif
$ if cc_out_file .nes. ""
$ then
$    if f$search(cc_out_file) .nes. ""
$    then
$        open/read xx 'cc_out_file'
$        line_in = ""
$        read/end=read_xx1 xx line_in
$read_xx1:
$        close xx
$        if f$locate(expect_cc_out, line_in) .ne. 0
$        then
$            write sys$output "''expect_cc_prog_out' not found in output"
$            lcl_fail = lcl_fail + 1
$        endif
$        delete 'cc_out_file';*
$    else
$        write sys$output "Output file not created";
$        lcl_fail = lcl_fail + 1
$    endif
$ endif
$ unix_status = 0
$ if (cc_status .and. %x35a000) .eq. %x35a000
$ then
$    unix_status = (cc_status / 8) .and. 255
$ endif
$ if  f$search(efile) .eqs. ""
$ then
$    if unix_status .eq. 0
$    then
$        write sys$output "Executable not produced!"
$        lcl_fail = lcl_fail + 1
$    endif
$ else
$    if noexe .eq. 0
$    then
$        define/user sys$output 'out_file'
$        mcr sys$disk:[]'efile'
$        prog_status = '$status'
$        if prog_status .nes. expect_prog_status
$        then
$            write sys$output -
   "Program status ''proj_status' is not ''expect_prog_status'!"
$            lcl_fail = lcl_fail + 1
$        endif
$        if f$search(out_file) .nes. ""
$        then
$            open/read xx 'out_file'
$            line_in = ""
$            read/end=read_xx2 xx line_in
$read_xx2:
$            close xx
$            if f$locate(expect_prog_out, line_in) .ne. 0
$            then
$                write sys$output "''expect_prog_out' not found in output"
$                lcl_fail = lcl_fail + 1
$		 write sys$output "Got -''line_in'"
$            endif
$            delete 'out_file';*
$        else
$            write sys$output "Output file not created";
$            lcl_fail = lcl_fail + 1
$        endif
$    endif
$    delete 'efile';*
$ endif
$ if f$search(ofile) .nes. "" then delete 'ofile';*
$ if f$search(lstfile) .nes. "" then delete 'lstfile';*
$ if f$search(mapfile) .nes. "" then delete 'mapfile';*
$ if f$search(dsffile) .nes. "" then delete 'dsffile';*
$
$ if lcl_fail .ne. 0 then fail = fail + 1
$ return
$!
$test_clean_up:
$ noexe = 1
$ file = "test_hello.c"
$ if f$search(file) .nes. "" then delete 'file';
$ return
$!
$! Create a test file
$create_test_hello_c:
$ file = "test_hello.c"
$ if f$search(file) .nes. "" then delete 'file';
$ create 'file'
#include <stdio.h>

int main(int argc, char **argv) {
#ifdef Test_NDEFINE
    printf("%d\n", Test_NDEFINE);
    return 1;
#endif
#ifdef TEST_DEFINE
    puts(TEST_DEFINE);
    return 1;
#endif
#if defined(NDEBUG) && defined(Py_BUILD_CORE)
    printf("flags=%s\n", ABIFLAGS);
    return 1;
#endif
#if !defined(TEST_DEFINE) && !defined(Test_NDEFINE) && !defined(NDEBUG)
    puts("Hello World!");
    return 0;
#endif
}
$ return
