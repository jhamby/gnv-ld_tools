$! File: test_ld_tools.com
$!
$! Use test_cc foreign command to prevent recursion
$!--------------------------------------------------
$ if f$search("gnv$gnu:[bin]cc.exe") .eqs. ""
$ then
$   ! Acctual installed location
$   test_cc = "gnv$gnu:[usr.bin]cc.exe"
$   test_cc_cmd = "/usr/bin/cc"
$   ld = "gnv$gnu:[usr.bin]ld.exe"
$   ld_cmd = "/usr/bin/ld"
$ else
$   ! Staged location is preferred
$   test_cc = "gnv$gnu:[bin]cc.exe"
$   test_cc_cmd = "/bin/cc"
$   ld = "gnv$gnu:[bin]ld.exe"
$   ld_cmd = "/bin/ld"
$ endif
$! test_cc = "sys$disk:[]gnv$ld.exe"
$! test_cc_cmd = "./ld"
$ execv := $sys$disk:[]execv_symbol.exe
$!
$ arch_type = f$getsyi("ARCH_NAME")
$ arch_code = f$extract(0, 1, arch_type)
$!
$! Initialize counts.
$ fail = 0
$ test = 0
$ pid = f$getjpi("", "pid")
$!
$gosub clean_params
$!
$! Start with testing the version
$!--------------------------------
$ gosub version_test
$!
$! Then multi-arch
$ gosub print_multiarch_test
$ gosub print_search_compile
$!
$!
$! Simple CC tests
$!---------------
$ gosub simple_compile
$ gosub compile_root_log_dash_c_dash_o
$ gosub compile_log_dash_c_dash_o
$ gosub compile_stdin
$ if arch_code .nes. "V"
$ then
$   gosub compile_dot_in_directory
$   gosub compile_dot_in_name
$ endif
$!
$! STD tests
$!-----------
$ gosub std_c99_compile
$ gosub std_c90_compile
$ if arch_code .nes. "V"
$ then
$   gosub std_gnu90_compile
$ endif
$!
$!
$! flag tests
$!---------------
$ gosub strict_aliasing_compile
$ gosub no_strict_aliasing_compile
$ gosub fstack_protector_compile
$ gosub fvisibility_compile
$ gosub fpic_compile
$ gosub wall_compile
$ gosub wextra_compile
$ gosub wl_as_needed_compile
$ gosub wl_rpath_compile
$ gosub wl_export_dyn_compile
$ gosub wl_no_undef_compile
$ gosub wl_version_compile
$ gosub pedantic_compile
$!
$! Quoted define
$!----------------
$ gosub quoted_define
$ gosub quoted_define2
$ gosub numeric_define
$ gosub string_define
$!
$! Missing objects/images
$ if arch_code .nes. "V"
$ then
$   !VAX does not get warning status from compiler.
$   gosub missing_objects
$   gosub missing_shared_images
$   gosub missing_library
$ endif
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
$REPORT:
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
$ write sys$output "Version test"
$ test = test + 1
$ out_file = "sys$scratch:test_cc_version.out"
$ efile = "a.out"
$ cflags = "--version"
$ expect_cc_out = "GNV ld"
$ gosub version_test_driver
$ return
$!
$! --print-multiarch test
$!------------------------
$print_multiarch_test:
$!
$ write sys$output "print multiarch test"
$ test = test + 1
$ lcl_fail = 0
$ expect_cc_status = 1
$ out_file = "sys$scratch:test_cc_version.out"
$ efile = "a.out"
$ cflags = "--print-multiarch"
$ cmd = "/bin/ld"
$ expect_cc_out = "unknown-vms-hp"
$ if arch_code .eqs. "V" then expect_cc_out = "vax-vms-hp"
$ if arch_code .eqs. "A" then expect_cc_out = "alpha-vms-hp"
$ if arch_code .eqs. "I" then expect_cc_out = "ia64-vms-hp"
$ cc_out_file = "test_cc.out"
$ gosub version_test_driver
$ return
$!
$!
$print_search_compile:
$ write sys$output "Compile -print-search-dirs"
$ test = test + 1
$ cmd = "/bin/ld"
$ efile = "a.out"
$ cflags = "-print-search-dirs"
$ expect_c_out = "install: "
$ gosub version_test_driver
$ return
$
$! Driver for compile commands that do not produce an object
$!----------------------------------------------------------
$version_test_driver:
$ if f$search(out_file) .nes. "" then delete 'out_file';*
$ if f$search(efile) .nes. "" then delete 'efile';*
$ cmd_symbol = "''cmd' ''cflags'"
$ cmd_symbol = f$edit(cmd_symbol, "trim")
$ write sys$output "  command: """, cmd_symbol, """"
$ define/user sys$output 'out_file'
$ execv 'test_cc' cmd_symbol
$ cc_status = '$status' .and. (.not. %x10000000)
$ if cc_status .ne. expect_cc_status
$ then
$   write sys$output "  ** CC status was ''cc_status'  not ''expect_cc_status'!"
$   lcl_fail = lcl_fail + 1
$ endif
$ if f$search(out_file) .nes. ""
$ then
$   open/read xx 'out_file'
$   read xx line_in
$   close xx
$   if f$locate(expect_cc_out, line_in) .ne. 0
$   then
$       write sys$output "  ** ''expect_cc_out' not found in output"
$       lcl_fail = lcl_fail + 1
$       type 'out_file'
$   endif
$   delete 'out_file';*
$ else
$    write sys$output "  ** Output file not created"
$    lcl_fail = lcl_fail + 1
$ endif
$ if f$search(efile) .nes. ""
$ then
$    write sys$output -
         "  ** An object file was created and should not have been!"
$    lcl_fail = lcl_fail + 1
$    delete 'efile';*
$ endif
$ if lcl_fail .ne. 0 then fail = fail + 1
$ gosub clean_params
$ return
$!
$!
$simple_compile:
$ write sys$output "Simple Compile"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ gosub compile_driver
$ return
$!
$!
$! Use a concealed logical root for device name
$! assume single level directory.
$!
$compile_root_log_dash_c_dash_o:
$ write sys$output "Compile rooted logical -c -o"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "test_dash_o.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ my_default = f$environment("default")
$ my_dev = f$parse(my_default,,,"device")
$ my_dir = f$parse(my_default,,,"directory") - "[" - "]" - "<" - ">"
$ test_root = "test_''pid'_root"
$ cflags = "-c -o /''test_root'/''my_dir'/''efile'"
$ noexe = 1
$ out_file = "test_cc_prog.out"
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
$ write sys$output "Compile logical -c -o"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "test_dash_o.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ my_default = f$environment("default")
$ test_log = "test_''pid'_log"
$ cflags = "-c -o /''test_log'/''efile'"
$ noexe = 1
$ define 'test_log' 'my_default'
$ gosub compile_driver
$ deas 'test_log'
$ return
$!
$!
$compile_stdin:
$ write sys$output "Compile stdin"
$ test = test + 1
$ file = "-"
$ cfile = "-"
$ ofile = file + ".o"
$ efile = "a.out"
$ noexe = 1
$ cflags = "-xc -c -o ''efile'"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ out_file = ""
$ gosub compile_driver
$ return
$!
$!
$compile_dot_in_directory:
$ write sys$output "Compile with dot in directory"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "[.dot^.in_dir]test_hello."
$ cflags = "-o ./dot.in_dir/test_hello"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ create/dir sys$disk:[.dot^.in_dir]/prot=o:rwed"
$ gosub compile_driver
$ dfile = "sys$disk:[.dot^.in_dir]*.*;*"
$ if f$search(dfile) .nes. "" then delete 'dfile'
$ delete sys$disk:[]dot^.in_dir.dir;
$set nover
$ return
$!
$!
$compile_dot_in_name:
$ write sys$output "Compile with dot in filename"
$ test = test + 1
$ file = "test^.hello"
$ gosub create_test_hello_c
$ efile = "test.hello"
$ cflags = "-o test.hello"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ old_efs = f$trnlnm("DECC$EFS_CHARSET", "LNM$PROCESS_TABLE")
$ if old_efs .nes. "" then deas decc$efs_charset
$ define DECC$EFS_CHARSET disable
$ gosub compile_driver
$ deas decc$efs_charset
$ if old_efs .nes. ""
$ then
$    define DECC$EFS_CHARSET 'old_efs'
$ endif
$ return
$!
$!
$fstack_protector_compile:
$ write sys$output "Compile -fstack-protector"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-fstack-protector"
$ gosub compile_driver
$ return
$!
$!
$fvisibility_compile:
$ write sys$output "Compile -fvisibility=hidden"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-fvisibility=hidden"
$ gosub compile_driver
$ return
$!
$!
$fpic_compile:
$ write sys$output "Compile -fPIC"
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-fPIC"
$ gosub compile_driver
$ return
$!
$!
$std_c99_compile:
$ write sys$output "Compile -std=c99"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-std=c99"
$ search_listing = "__STDC__=1"
$ gosub compile_driver
$ return
$!
$!
$std_c90_compile:
$ write sys$output "Compile -std=c90"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-std=c99"
$ search_listing = "__STDC__=1"
$ gosub compile_driver
$ return
$!
$!
$std_gnu90_compile:
$ write sys$output "Compile -std=gnu90"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-std=gnu90"
$ search_listing = "__STDC__=2"
$ expect_uopt = 1
$ gosub compile_driver
$ return
$!
$!
$strict_aliasing_compile:
$ write sys$output "Compile -fstrict-aliasing"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-fstrict-aliasing"
$ search_listing = ""
$ expect_uopt = 1
$ gosub compile_driver
$ return
$!
$!
$no_strict_aliasing_compile:
$ write sys$output "Compile -fno-strict-aliasing"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-fno-strict-aliasing"
$ search_listing = ""
$ expect_uopt = 1
$ gosub compile_driver
$ return
$!
$!
$wall_compile:
$ write sys$output "Compile -Wall"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-Wall"
$ gosub compile_driver
$ return
$!
$!
$wextra_compile:
$ write sys$output "Compile -Wextra"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-Wextra"
$ gosub compile_driver
$ return
$!
$!
$wl_as_needed_compile:
$ write sys$output "Compile -Wl,-as-needed"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-Wl,-as-needed"
$ gosub compile_driver
$ return
$!
$!
$wl_rpath_compile:
$ write sys$output "Compile -Wl,-rpath"
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-Wl,-rpath=."
$ gosub compile_driver
$ return
$!
$!
$wl_export_dyn_compile:
$ write sys$output "Compile -Wl,-export-dynamic"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-Wl,-export-dynamic"
$ gosub compile_driver
$ return
$!
$!
$wl_no_undef_compile:
$ write sys$output "Compile -Wl,-no-undefined"
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-Wl,-no-undefined"
$ gosub compile_driver
$ return
$!
$!
$wl_version_compile:
$ write sys$output "Compile -Wl,--version-script"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-Wl,--version-script"
$ gosub compile_driver
$ return
$!
$!
$pedantic_compile:
$ write sys$output "Compile -pedantic"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-pedantic"
$ gosub compile_driver
$ return
$!
$!
$quoted_define:
$!set ver
$ write sys$output "Compile quoted define"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-DTEST_DEFINE=""foo bar"""
$ expect_prog_out = "foo bar"
$ gosub compile_driver
$set nover
$ return
$!
$!
$!
$quoted_define2:
$ write sys$output "Compile quoted define2"
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-DNDEBUG -DPy_BUILD_CORE -DABIFLAGS=""M"""
$ expect_prog_out = "flags=M"
$ gosub compile_driver
$ return
$!
$!
$numeric_define:
$ write sys$output "Compile numeric define"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-DTest_NDEFINE=0x03500"
$ expect_prog_out = "13568"
$ gosub compile_driver
$ return
$!
$!
$string_define:
$ write sys$output "Compile string define"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-DTEST_TXT_DEFINE=puts"
$ expect_prog_out = "Hello World!"
$ gosub compile_driver
$ return
$!
$!
$missing_objects:
$ write sys$output "Compile missing objects"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ more_files = "missing.o"
$ expect_uf = 1
$ cflags = ""
$ expect_cc_status = %x35a011
$ out_file = "test_cc_prog.out"
$ gosub compile_driver
$ return
$!
$missing_shared_images:
$ write sys$output "Compile missing shared images"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ more_files = "missing.so"
$ expect_uf = 1
$ cflags = ""
$ expect_cc_status = %x35a011
$ cc_out_file = "test_cc.out"
$ gosub compile_driver
$ return
$!
$missing_library:
$ write sys$output "Compile missing library"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "a.out"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ more_files = "missing.a"
$ expect_uf = 1
$ cflags = ""
$ expect_cc_status = %x35a011
$ gosub compile_driver
$ return
$!
$!
$cc_command_file:
$!set ver
$ write sys$output "Compile cc_command_file"
$ test = test + 1
$ lcl_fail = 0
$ gosub create_test_hello_c
$ efile = "test_hello.o"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cflags = "-o test_hello.o"
$ expect_cc_out = "test gnv$test_hello_cc.com"
$ cc_cmd_file = "gnv$test_hello_cc.com"
$ if f$search(cc_cmd_file) .nes. "" then delete 'cc_cmd_file';*
$ create 'cc_cmd_file'
$DECK
$ write sys$output "test gnv$test_hello_cc.com"
$EOD
$ gosub compile_driver
$ if f$search(cc_cmd_file) .nes. "" then delete 'cc_cmd_file';*
$set nover
$ return
$!
$!
$ld_gnv_shared_logical:
$ write sys$output "Compile ld gnv_shared_logical"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "test_hello.exe"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cc/object=test_hello_shr.o 'cfile'
$ link/share=test_hello_shr.exe test_hello_shr.o
$ define gnv$libtest_hello_shr sys$disk:[]test_hello_shr.exe
$ cflags = "-o test_hello.exe -ltest_hello_shr"
$ gosub compile_driver
$ obj_file = file + "_shr.o"
$ if f$search(obj_file) .nes. "" then delete 'obj_file';*
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
$ write sys$output "Compile ld gnv_shared_logical2"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "test_hello.exe"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
$ cc/object=test_hello_shr.o 'cfile'
$ link/share=test_hello_shr.exe test_hello_shr.o
$ define gnv$libtest_hello_shr sys$disk:[]test_hello_shr.exe
$ cflags = "-o test_hello.exe libtest_hello_shr.a"
$ gosub compile_driver
$ obj_file = file + "_shr.o"
$ if f$search(obj_file) .nes. "" then delete 'obj_file';*
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
$! on warning then set ver
$ if f$type(noexe) .eqs. "" then noexe = 0
$ cmd_symbol = test_cc_cmd
$ unix_c_file = cfile - "^"
$ if cflags .nes. "" then cmd_symbol = cmd_symbol + " " + cflags
$ if unix_c_file .nes. "" then cmd_symbol = cmd_symbol  + " " + unix_c_file
$ if more_files .nes. "" then cmd_symbol = cmd_symbol + " " + more_files
$ cmd_symbol = f$edit(cmd_symbol, "trim")
$ write sys$output "  command: """, cmd_symbol, """"
$ if cfile .eqs. "-"
$ then
$   ! Can't use cc_out_file here for now.
$   cc_out_file = ""
$   simple_c = "int main(int argc, char **argv) {return 0;}"
$   pipe write sys$output simple_c -
| execv 'test_cc' cmd_symbol
$ else
$   if cc_out_file .nes. "" then define/user sys$output 'cc_out_file'
$   execv 'test_cc' cmd_symbol
$ endif
$ cc_status = '$status' .and. (.not. %x10000000)
$ gnv_temp_file = f$search("gnv$cc_stdin_*.*")
$ if gnv_temp_file .nes. ""
$ then
$set ver
$   write sys$output "** ''gnv_temp_file' temp file left behind."
$   lcl_fail = lcl_fail + 1
$   dir gnv$cc_stdin*.*
$   del gnv$cc_stdin_*.*;*
$set nover
$ endif
$ if cc_status .ne. expect_cc_status
$ then
$   show log sys$output
$   write sys$output "  ** CC status ''cc_status' is not ''expect_cc_status'!"
$   lcl_fail = lcl_fail + 1
$ endif
$ if f$search(lstfile) .nes. "" .and. search_listing .nes. ""
$ then
$   old_msg = f$enviroment("message")
$   set message/noident/nofac/nosev/notext
$   search/exact/out=nla0: 'lstfile' "''search_listing'"
$   severity = '$severity'
$   set message'old_msg'
$   if severity .ne. 1
$   then
$      write sys$output "  ** Did not find ''search_listing' in listing."
$      lcl_fail = lcl_fail + 1
$   endif
$ endif
$ if cc_out_file .nes. ""
$ then
$   if f$search(cc_out_file) .nes. ""
$   then
$       open/read xx 'cc_out_file'
$       line_in = ""
$       read/end=read_xx1 xx line_in
$read_xx1:
$       close xx
$       if expect_cc_out .nes. ""
$       then
$           if f$locate(expect_cc_out, line_in) .ne. 0
$           then
$               write sys$output -
                    "  ** ''expect_cc_prog_out' not found in output."
$               lcl_fail = lcl_fail + 1
$               type 'cc_out_file'
$           endif
$       endif
$       if .not. expect_uf
$       then
$           if f$locate("Unrecognized file", line_in) .lt. f$length(line_in)
$           then
$               write sys$output "  ** Unrecognized file seen in output."
$               lcl_fail = lcl_fail + 1
$               type 'cc_out_file'
$           endif
$       endif
$       if .not. expect_usw
$       then
$           if f$locate("Unrecognized switch", line_in) .lt. f$length(line_in)
$           then
$               write sys$output "  ** Unrecognized switch seen in output."
$               lcl_fail = lcl_fail + 1
$               type 'cc_out_file'
$           endif
$       endif
$       if .not. expect_uopt
$       then
$           if f$locate("unrecognized option", line_in) .lt. f$length(line_in)
$           then
$               write sys$output "  ** Unrecognized option seen in output."
$               lcl_fail = lcl_fail + 1
$               type 'cc_out_file'
$           endif
$       endif
$       delete 'cc_out_file';*
$   else
$       write sys$output "  ** Output file not created"
$       lcl_fail = lcl_fail + 1
$   endif
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
$        write sys$output "  ** Executable not produced!"
$        lcl_fail = lcl_fail + 1
$    endif
$ else
$   if noexe .eq. 0
$   then
$       if out_file .nes. "" then define/user sys$output 'out_file'
$       on severe_error then goto exe_fatal
$       exe_file = f$parse(efile, "sys$disk:[]")
$       mcr 'exe_file'
$exe_fatal:
$       prog_status = '$status'
$       if prog_status .nes. expect_prog_status
$       then
$           write sys$output -
   "  ** Program status ''proj_status' is not ''expect_prog_status'!"
$           lcl_fail = lcl_fail + 1
$       endif
$       if out_file .nes. ""
$       then
$           if f$search(out_file) .nes. ""
$           then
$               open/read xx 'out_file'
$               line_in = ""
$               read/end=read_xx2 xx line_in
$read_xx2:
$               close xx
$               if f$locate(expect_prog_out, line_in) .ne. 0
$               then
$                write sys$output "  ** ''expect_prog_out' not found in output"
$                  lcl_fail = lcl_fail + 1
$                  write sys$output "  ** Got -''line_in'-"
$               endif
$               delete 'out_file';*
$           else
$               write sys$output "  ** Output file not created"
$               lcl_fail = lcl_fail + 1
$           endif
$       endif
$   endif
$   delete 'efile';*
$ endif
$ if f$search(ofile) .nes. "" then delete 'ofile';*
$ if f$search(lstfile) .nes. "" then delete 'lstfile';*
$ if f$search(mapfile) .nes. "" then delete 'mapfile';*
$ if f$search(dsffile) .nes. "" then delete 'dsffile';*
$
$ if lcl_fail .ne. 0 then fail = fail + 1
$ set nover
$ ! cleanup
$ gosub test_clean_up
$ gosub clean_params
$ return
$!
$clean_params:
$ cmd = ld_cmd
$ file = "test_hello"
$ cfile = file + ".c"
$ more_files = ""
$ expect_cc_out = ""
$ out_file = "test_cc_prog.out"
$ cc_out_file = "test_cc.out"
$ lcl_fail = 0
$ expect_cc_status = 1
$ expect_prog_status = 1
$ expect_uf = 0
$ expect_usw = 0
$ expect_uopt = 0
$ efile = "a.out"
$ cflags = ""
$ expect_cc_out = ""
$ noexe = 0
$ expect_prog_out = "Hello World"
$ search_listing = ""
$ return
$!
$test_clean_up:
$ noexe = 1
$ if f$search(cfile) .nes. "" then delete 'cfile';
$ return
$!
$! Create a test file
$create_test_hello_c:
$ cfile = file + ".c"
$ if f$search(cfile) .nes. "" then delete 'cfile';
$ create 'cfile'
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
#ifdef TEST_TXT_DEFINE
    TEST_TXT_DEFINE("Hello World!");
#else
# if !defined(TEST_DEFINE) && !defined(Test_NDEFINE) && !defined(NDEBUG)
    puts("Hello World!");
    return 0;
# endif
#endif
}
$ return
