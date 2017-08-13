$! File: test_ld_tools.com
$!
$! Use test_cc foreign command to prevent recursion
$!--------------------------------------------------
$ cc := "cc/list"
$ if f$search("gnv$gnu:[bin]cc.exe") .eqs. ""
$ then
$   ! Acctual installed location
$   test_cc = "gnv$gnu:[usr.bin]cc.exe"
$   test_cc_cmd = "/usr/bin/cc"
$   test_cpp_cmd = "/usr/bin/cpp"
$   ld = "gnv$gnu:[usr.bin]ld.exe"
$   ld_cmd = "/usr/bin/ld"
$ else
$   ! Staged location is preferred
$   test_cc = "gnv$gnu:[bin]cc.exe"
$   test_cc_cmd = "/bin/cc"
$   test_cpp_cmd = "/bin/cpp"
$   ld = "gnv$gnu:[bin]ld.exe"
$   ld_cmd = "/bin/ld"
$ endif
$ if p1 .eqs. "DEBUG"
$ then
$    test_cc = "sys$disk:[]gnv$debug-ld.exe"
$    test_cc_cmd = "./cc"
$    test_cpp_cmd = "./cpp"
$ endif
$ execv := $sys$disk:[]execv_symbol.exe
$!
$ arch_type = f$getsyi("ARCH_NAME")
$ arch_code = f$extract(0, 1, arch_type)
$!
$! Initialize counts.
$ fail = 0
$ test = 0
$ test_count = 51
$ pid = f$getjpi("", "pid")
$!
$ temp_fdl = "sys$disk:[]stream_lf.fdl"
$ if arch_code .nes. "V"
$ then
$  create sys$disk:[]test_output.xml/fdl="RECORD; FORMAT STREAM_LF;"
$ else
$  if f$search(temp_fdl) .nes. "" then delete 'temp_fdl';*
$  create 'temp_fdl'
RECORD
	FORMAT		stream_lf
$ continue
$ create sys$disk:[]test_output.xml/fdl='temp_fdl'
$ endif
$ open/append junit sys$disk:[]test_output.xml
$ write junit "<?xml version=""1.0"" encoding=""UTF-8""?>"
$ write junit "  <testsuite name=""ld_tools"""
$ write junit "   tests=""''test_count'"">"
$!
$gosub clean_params
$!
$! Start with testing the version
$!--------------------------------
$!goto first_fail
$ gosub version_test
$ gosub version_test1
$!
$! Then multi-arch
$ gosub print_multiarch_test
$ gosub print_search_compile
$!
$!
$! Simple CC tests
$!---------------
$ gosub simple_compile
$ gosub compile_fsyntax_only
$ gosub compile_root_log_dash_c_dash_o
$ if arch_code .nes. "V"
$ then
$   gosub compile_log_dash_c_dash_o
$   gosub compile_stdin
$   gosub compile_dot_in_directory
$   gosub compile_dot_in_name
$ else
$   skip_reason = "#logical name format not supported by VAX"
$   testcase_name = "compile_log_dash_c_dash_o"
$   gosub skipped_test_driver
$   skip_reason = "#pragma include_directory not supported by DECC/VAX"
$   testcase_name = "compile_stdin"
$   gosub skipped_test_driver
$   skip_reason = "OSS-5 support"
$   testcase_name = "compile_dot_in_directory"
$   gosub skipped_test_driver
$   testcase_name = "compile_dot_in_name"
$   gosub skipped_test_driver
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
$ gosub wl_expect_unresolved
$ gosub wl_update_registry
$ gosub wl_soname
$ gosub wl_hidden
$ gosub wl_input
$ gosub samba_options_1
$ gosub pedantic_compile
$!
$! Quoted define
$!----------------
$ gosub quoted_define
$ gosub quoted_define2
$ gosub numeric_define
$ gosub string_define
$ gosub single_quoted_define ! GNV Ticket 103
$!
$! LONG_DEFINE_LINES bug - GNV Ticket 101
$ if arch_code .nes. "V"
$ then
$   gosub cc_define_token_max
$ else
$   skip_reason = "DCL extended parse needed"
$   testcase_name = "cc_define_token_max"
$   gosub skipped_test_driver
$ endif
$ gosub cc_multiple_src
$ gosub cpp_cxx_default_opts
$!
$! gnv$xxx.* files and options
$!-----------------------------
$ gosub ld_auto_opt_file
$ gosub ld_link_map
$ gosub cc_first_module_dir
$!
$! Missing objects/images
$ if arch_code .nes. "V"
$ then
$   !VAX does not get warning status from compiler.
$   gosub missing_objects
$   gosub missing_shared_images
$   gosub missing_library
$ else
$   skip_reason = "VAX not returning error status"
$   testcase_name = "missing_objects"
$   gosub skipped_test_driver
$   testcase_name = "missing_shared_images"
$   gosub skipped_test_driver
$   testcase_name="missing_library"
$   gosub skipped_test_driver
$ endif
$!
$! Command file helpers
$ gosub cc_command_file
$!
$! Special GNV shared logical
$ gosub ld_gnv_shared_logical
$ gosub ld_gnv_shared_logical2
$ gosub cc_unknown_type1
$ gosub cc_unknown_type2
$!
$!
$! Sumarize the run
$!---------------------
$REPORT:
$!
$ write junit "</testsuite>"
$ close junit
$!
$!
$ gosub test_clean_up
$ write sys$output "''test' tests run."
$ if fail .ne. 0
$ then
$    write sys$output "''fail' tests failed!"
$ else
$    write sys$output "All tests passed!"
$ endif
$ if f$search(temp_fdl) .nes. "" then delete 'temp_fdl';*
$!
$!
$exit
$!
$!
$! Basic Version test
$!------------------------
$version_test:
$ testcase_name = "version_test"
$ test = test + 1
$ out_file = "sys$scratch:test_cc_version.out"
$ efile = "a.out"
$ cflags = "--version"
$ expect_cc_out = "GNV ld"
$ gosub version_test_driver
$ return
$!
$version_test1:
$ testcase_name = "version_test1"
$ test = test + 1
$ out_file = "sys$scratch:test_cc_version.out"
$ efile = "a.out"
$ cflags = "-version"
$ expect_cc_out = "GNV ld"
$ gosub version_test_driver
$ return
$!
$!
$! --print-multiarch test
$!------------------------
$print_multiarch_test:
$ testcase_name = "print_multiarch_test"
$ test = test + 1
$ lcl_fail = 0
$ expect_cc_status = 1
$ out_file = "sys$scratch:test_cc_version.out"
$ efile = "a.out"
$ cflags = "--print-multiarch"
$ cmd = ld_cmd
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
$ testcase_name = "print_search_compile"
$ test = test + 1
$ cmd = ld_cmd
$ efile = "a.out"
$ cflags = "-print-search-dirs"
$ expect_c_out = "install: "
$ gosub version_test_driver
$ return
$
$! Driver for compile commands that do not produce an object
$!----------------------------------------------------------
$version_test_driver:
$ write sys$output "''testcase_name'"
$ write junit "   <testcase name=""''testcase_name'"""
$ write junit "    classname=""ld_tools.version_test"">"
$ gosub directory_before
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
$   fail_type = "compile"
$   fail_msg = "CC status was ''cc_status'  not ''expect_cc_status'!"
$   gosub report_failure
$ endif
$ if f$search(out_file) .nes. ""
$ then
$   open/read xx 'out_file'
$   read xx line_in
$   close xx
$   if f$locate(expect_cc_out, line_in) .ne. 0
$   then
$       fail_type = "compile"
$       fail_msg = "''expect_cc_out' not found in output"
$       gosub report_failure
$       type 'out_file'
$   endif
$   delete 'out_file';*
$ else
$   fail_type = "compile"
$   fail_msg = "Output file not created"
$   gosub report_failure
$ endif
$ if f$search(efile) .nes. ""
$ then
$    fail_type = "compile"
$    fail_msg = "An object file was created and should not have been"
$    gosub report_failure
$    delete 'efile';*
$ endif
$ gosub directory_after
$ if lcl_fail .ne. 0 then fail = fail + 1
$ write junit "   </testcase>"
$ gosub clean_params
$ return
$!
$!
$simple_compile:
$ testcase_name = "simple_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ gosub compile_driver
$ return
$!
$!
$compile_fsyntax_only:
$ testcase_name = "compile_fsyntax_only"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-fsyntax-only"
$ noexe = 1
$ no_efile = 1
$ gosub compile_driver
$ return
$!
$!
$! Use a concealed logical root for device name
$! assume single level directory.
$!
$compile_root_log_dash_c_dash_o:
$ testcase_name = "compile_root_log_dash_c_dash_o"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "test_dash_o.out"
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
$ testcase_name = "compile_log_dash_c_dash_o"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "test_dash_o.out"
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
$ testcase_name = "compile_stdin"
$ test = test + 1
$ file = "-"
$ cfile = "-"
$ ofile = file + ".o"
$ efile = "a.out"
$ noexe = 1
$ cflags = "-xc -c -o ''efile'"
$ out_file = ""
$ gosub compile_driver
$ return
$!
$!
$compile_dot_in_directory:
$ testcase_name = "compile_dot_in_directory"
$ test = test + 1
$ gosub create_test_hello_c
$ set proc/parse=extended
$ efile = "[.dot^.in_dir]test_hello."
$ cflags = "-o ./dot.in_dir/test_hello"
$ create/dir sys$disk:[.dot^.in_dir]/prot=o:rwed"
$ gosub compile_driver
$ dfile = "sys$disk:[.dot^.in_dir]*.*;*"
$ if f$search(dfile) .nes. "" then delete 'dfile'
$ delete sys$disk:[]dot^.in_dir.dir;
$ return
$!
$!
$compile_dot_in_name:
$! write sys$output "Compile with dot in filename, also GNV Ticket 101"
$ testcase_name = "compile_dot_in_name"
$! SET PROC/PARSE=TRADITIONAL bug - GNV Ticket 101
$ test = test + 1
$ file = "test^.hello"
$ gosub create_test_hello_c
$ lstfile = "sys$disk:[]" + file + ".lis"
$ mapfile = "sys$disk:[]" + file + ".map"
$ dsffile = "sys$disk:[]" + file + ".dsf"
$ ofile = file + ".o"
$ efile = "test.hello"
$ cflags = "-o test.hello"
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
$ testcase_name = "fstack_protector_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-fstack-protector"
$ gosub compile_driver
$ return
$!
$!
$fvisibility_compile:
$ testcase_name = "visibility_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-fvisibility=hidden"
$ gosub compile_driver
$ return
$!
$!
$fpic_compile:
$ testcase_name = "fpic_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-fPIC"
$ gosub compile_driver
$ return
$!
$!
$std_c99_compile:
$ testcase_name = "std_c99_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-std=c99"
$ search_listing = "__STDC__=1"
$ gosub compile_driver
$ return
$!
$!
$std_c90_compile:
$ testcase_name = "std_c90_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-std=c99"
$ search_listing = "__STDC__=1"
$ gosub compile_driver
$ return
$!
$!
$std_gnu90_compile:
$ testcase_name = "std_gnu90_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-std=gnu90"
$ search_listing = "__STDC__=2"
$ expect_uopt = 1
$ gosub compile_driver
$ return
$!
$!
$strict_aliasing_compile:
$ testcase_name = "strict_aliasing_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-fstrict-aliasing"
$ search_listing = ""
$ expect_uopt = 1
$ gosub compile_driver
$ return
$!
$!
$no_strict_aliasing_compile:
$ testcase_name = "no_strict_aliasing_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-fno-strict-aliasing"
$ search_listing = ""
$ expect_uopt = 1
$ gosub compile_driver
$ return
$!
$!
$wall_compile:
$ testcase_name = "wall_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wall"
$ gosub compile_driver
$ return
$!
$!
$wextra_compile:
$ testcase_name = "wextra_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wextra"
$ gosub compile_driver
$ return
$!
$!
$wl_as_needed_compile:
$ testcase_name = "wl_as_needed_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,-as-needed"
$ gosub compile_driver
$ return
$!
$!
$wl_rpath_compile:
$ testcase_name = "wl_rpath_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,-rpath=."
$ gosub compile_driver
$ return
$!
$!
$wl_export_dyn_compile:
$ testcase_name = "wl_export_dyn_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,-export-dynamic"
$ gosub compile_driver
$ return
$!
$!
$wl_no_undef_compile:
$ testcase_name = "wl_no_undef_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,-no-undefined"
$ gosub compile_driver
$ return
$!
$!
$wl_version_compile:
$ testcase_name = "wl_version_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,--version-script"
$ gosub compile_driver
$ return
$!
$!
$wl_expect_unresolved:
$! write sys$output "Compile -Wl,-expect_unresolved '-Wl,*'"
$ testcase_name = "wl_expect_unresolved"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,-expect_unresolved '-Wl,*'"
$ gosub compile_driver
$ return
$!
$!
$wl_update_registry:
$! write sys$output "Compile -Wl,-update_registry -Wl,../../so_locations"
$ testcase_name = "wl_update_registry"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,-update_registry -Wl,../../so_locations"
$ gosub compile_driver
$ return
$!
$!
$wl_soname:
$! write sys$output "Compile -Wl,-soname -Wl,libkrb5support.so.0"
$ testcase_name = "wl_soname"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,-soname -Wl,libkrb5support.so.0"
$ gosub compile_driver
$ return
$!
$!
$wl_hidden:
$ testcase_name = "wl_hidden"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,-hidden"
$ gosub compile_driver
$ return
$!
$!
$wl_input:
$! write sys$output "Compile -Wl,-input,osf1.exports"
$ testcase_name = "wl_input"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-Wl,-input,osf1.exports"
$ gosub compile_driver
$ return
$!
$samba_options_1:
$ testcase_name = "samba_options1"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-pie -Wl,-z,relro,-z,now -Wl,--as-needed"
$ gosub compile_driver
$ return
$!
$!
$pedantic_compile:
$ testcase_name = "pedantic_compile"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-pedantic"
$ gosub compile_driver
$ return
$!
$!
$quoted_define:
$ testcase_name = "quoted_define"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-DTEST_DEFINE=""foo bar"""
$ expect_prog_out = "foo bar"
$ gosub compile_driver
$ return
$!
$!
$!
$quoted_define2:
$ testcase_name = "quoted_define2"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-DNDEBUG -DPy_BUILD_CORE -DABIFLAGS=""M"""
$ expect_prog_out = "flags=M"
$ gosub compile_driver
$ return
$!
$!
$numeric_define:
$ testcase_name = "numeric_define"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-DTest_NDEFINE=0x03500"
$ expect_prog_out = "13568"
$ gosub compile_driver
$ return
$!
$!
$string_define:
$ testcase_name = "string_define"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "-DTEST_TXT_DEFINE=puts"
$ expect_prog_out = "Hello World!"
$ gosub compile_driver
$ return
$!
$!
$single_quoted_define:
$! write sys$output "Compile single quoted define - Ticket 103"
$ testcase_name = "single_quoted_define"
$ test = test + 1
$ gosub create_test_hello_c
$ cflags = "'-DTEST_TXT_DEFINE=int i=puts'"
$ expect_prog_out = "Hello World!"
$ gosub compile_driver
$ return
$!
$cc_define_token_max:
$! write sys$output "Define Long Lines bug -  GNV Ticket 101"
$ testcase_name = "cc_define_token_max"
$ test = test + 1
$ gosub create_test_hello_c
$ dsffile = "a" + ".dsf"
$! A long define/include string from building SAMBA/Kerberos that
$! exposed bugs in the CC wrapper.
$ longf = "'-D_REENTRANT' '-D_POSIX_PTHREAD_SEMANTICS' '-g'" + -
  " '-DSTATIC_smbtorture_MODULES=torture_base_init,torture_raw_init," + -
  "torture_smb2_init,torture_winbind_init,torture_libnetapi_init," + -
  "torture_libsmbclient_init,"
$ longf = longf + "torture_rpc_init,torture_drs_init,torture_rap_init," + -
  "torture_dfs_init,torture_local_init,torture_krb5_init," + -
  "torture_nbench_init,torture_unix_init,torture_ldap_init," + -
  "torture_nbt_init,torture_net_init,torture_ntp_init,torture_vfs_init,"
$ longf = longf + "torture_libcli_echo_init,NULL'" + -
  " '-DSTATIC_smbtorture_MODULES_PROTO=_MODULE_PROTO(torture_base_init)" + -
  "_MODULE_PROTO(torture_raw_init)_MODULE_PROTO(torture_smb2_init)" + -
  "_MODULE_PROTO(torture_winbind_init)" + -
  "_MODULE_PROTO(torture_libnetapi_init)" + -
  "_MODULE_PROTO(torture_libsmbclient_init)" + -
  "_MODULE_PROTO(torture_rpc_init)_MODULE_PROTO(torture_drs_init)" + -
  "_MODULE_PROTO(torture_rap_init)_MODULE_PROTO(torture_dfs_init)" + -
  "_MODULE_PROTO(torture_local_init)_MODULE_PROTO(torture_krb5_init)"
$ longf = longf + "_MODULE_PROTO(torture_nbench_init)" + -
  "_MODULE_PROTO(torture_unix_init)_MODULE_PROTO(torture_ldap_init)" + -
  "_MODULE_PROTO(torture_nbt_init)_MODULE_PROTO(torture_net_init)" + -
  "_MODULE_PROTO(torture_ntp_init)_MODULE_PROTO(torture_vfs_init)" + -
  "_MODULE_PROTO(torture_libcli_echo_init)" + -
  "extern void __smbtorture_dummy_module_proto(void)'" + -
  " '-Idefault/source4/torture' '-I../source4/torture'" + -
  " '-Idefault/include/public' '-I../include/public' '-Idefault/source4'" + -
  " '-I../source4' '-Idefault/lib' '-I../lib' '-Idefault/source4/lib'"
$ longf = longf + " '-I../source4/lib' '-Idefault/source4/include'" + -
  " '-I../source4/include' '-Idefault/include' '-I../include' " + -
  " '-Idefault/lib/replace' '-I../lib/replace' '-Idefault' '-I..'" + -
  " '-Idefault/source3' '-I../source3' '-Idefault/source3/include'"
$ longf = longf + " '-I../source3/include' '-Idefault/source3/lib'" + -
  " '-I../source3/lib' '-Idefault/source4/heimdal/lib/com_err'" + -
  " '-I../source4/heimdal/lib/com_err'" + -
  " '-Idefault/source4/heimdal/lib/krb5' '-I../source4/heimdal/lib/krb5'" + -
  " '-Idefault/source4/heimdal/lib/gssapi'"
$ longf = longf + " '-I../source4/heimdal/lib/gssapi'" + -
  " '-Idefault/source4/heimdal_build' '-I../source4/heimdal_build'" + -
  " '-Idefault/bin/default/source4/heimdal/lib/asn1'" + -
  " '-Idefault/source4/heimdal/lib/asn1' '-Idefault/lib/tdb/include'" + -
  " '-I../lib/tdb/include'"
$ longf = longf + " '-Idefault/lib/tevent' '-I../lib/tevent'" + -
  " '-Idefault/lib/talloc' '-I../lib/talloc' '-Idefault/lib/param'" + -
  " '-I../lib/param' '-Idefault/source4/heimdal/lib/hcrypto/libtommath'" + -
  " '-I../source4/heimdal/lib/hcrypto/libtommath' '-Idefault/librpc'"
$ longf = longf + " '-I../librpc' '-Idefault/libcli/ldap'" + -
  " '-I../libcli/ldap' '-Idefault/third_party/popt'" + -
  " '-I../third_party/popt' '-Idefault/source4/heimdal/lib/roken'" + -
  " '-I../source4/heimdal/lib/roken' '-Idefault/source4/heimdal/include'" + -
  " '-I../source4/heimdal/include' '-Idefault/source4/libnet'" + -
  " '-I../source4/libnet' '-Idefault/source4/dsdb' '-I../source4/dsdb'" + -
  " '-Idefault/source4/lib/http' '-I../source4/lib/http'" + -
  " '-Idefault/source4/auth/ntlm' '-I../source4/auth/ntlm'"
$ longf = longf + " '-Idefault/lib/torture' '-I../lib/torture'" + -
  " '-Idefault/source4/librpc' '-I../source4/librpc'" + -
  " '-Idefault/libcli/auth' '-I../libcli/auth' '-Idefault/lib/addns'" + -
  " '-I../lib/addns' '-Idefault/auth/gensec' '-I../auth/gensec'" + -
  " '-Idefault/auth/credentials' '-I../auth/credentials'"
$ longf = longf + " '-Idefault/source4/auth' '-I../source4/auth'" + -
  " '-Idefault/source4/heimdal/base' '-I../source4/heimdal/base'" + -
  " '-Idefault/source4/libcli/ldap' '-I../source4/libcli/ldap'" + -
  " '-Idefault/libcli/registry' '-I../libcli/registry'"
$ longf = longf + " '-Idefault/lib/krb5_wrap' '-I../lib/krb5_wrap'" + -
  " '-Idefault/libcli/util' '-I../libcli/util'" + -
  " '-Idefault/source4/winbind' '-I../source4/winbind'" + -
  " '-Idefault/source4/auth/kerberos' '-I../source4/auth/kerberos'" + -
  " '-Idefault/source4/libcli/rap' '-I../source4/libcli/rap'"
$ longf = longf + " '-Idefault/source4/libcli' '-I../source4/libcli'" + -
  " '-Idefault/lib/socket' '-I../lib/socket' '-Idefault/source4/param'" + -
  " '-I../source4/param' '-Idefault/lib/util/charset'" + -
  " '-I../lib/util/charset' '-Idefault/source4/lib/events'"
$ longf = longf + " '-I../source4/lib/events' '-Idefault/lib/ldb'" + -
  " '-I../lib/ldb' '-Idefault/source3/lib/poll_funcs'" + -
  " '-I../source3/lib/poll_funcs' '-Idefault/lib/tdb' '-I../lib/tdb'" + -
  " '-Idefault/lib/async_req' '-I../lib/async_req'" + -
  " '-Idefault/source4/auth/gensec'"
$! These values not used as it exceeds what this DCL harness can deal
$! with in a symbol.  I have left this here in case we need to extend
$! the test harness to handle a larger string.
$ longfx = longf + " '-I../source4/auth/gensec' '-Idefault/libcli/netlogon'" + -
  " '-I../libcli/netlogon' '-Idefault/nsswitch/libwbclient'" + -
  " '-I../nsswitch/libwbclient' '-Idefault/lib/ldb-samba' " + -
  " '-I../lib/ldb-samba' '-Idefault/source4/heimdal/lib/asn1'"
$ longfx = longf + " '-I../source4/heimdal/lib/asn1'" + -
  " '-Idefault/source4/heimdal/lib/hcrypto'" + -
  " '-I../source4/heimdal/lib/hcrypto' '-Idefault/source4/heimdal/lib'" + -
  " '-I../source4/heimdal/lib' '-Idefault/python' '-I../python'" + -
  " '-Idefault/auth/kerberos' '-I../auth/kerberos'"
$ longfx = " '-Idefault/libcli/nbt' '-I../libcli/nbt'" + -
  " '-Idefault/source4/heimdal/lib/hx509' '-I../source4/heimdal/lib/hx509'" + -
  " '-Idefault/lib/dbwrap' '-I../lib/dbwrap'" + -
  " '-Idefault/source4/lib/socket' '-I../source4/lib/socket'" + -
  " '-Idefault/source4/lib/samba3' '-I../source4/lib/samba3'" + -
  " '-Idefault/lib/tdr' '-I../lib/tdr' '-Idefault/source4/cluster'" + -
  " '-I../source4/cluster' '-Idefault/source3/lib/pthreadpool'" + -
  " '-I../source3/lib/pthreadpool' '-Idefault/libcli/security'" + -
  " '-I../libcli/security' '-Idefault/nsswitch'"
$ longfx = longfx + " '-I../nsswitch' '-Idefault/auth/ntlmssp'" + -
  " '-I../auth/ntlmssp' '-Idefault/lib/ldb/include'" + -
  " '-I../lib/ldb/include' '-Idefault/libcli/drsuapi'" + -
  " '-I../libcli/drsuapi' '-Idefault/lib/tsocket'" + -
  " '-I../lib/tsocket' '-Idefault/source4/heimdal/lib/wind'"
$ longfx = longfx + " '-I../source4/heimdal/lib/wind'" + -
  " '-Idefault/source4/lib/cmdline' '-I../source4/lib/cmdline'" + -
  " '-Idefault/source4/lib/tls' '-I../source4/lib/tls'" + -
  " '-Idefault/source4/heimdal/lib/gssapi/gssapi'" + -
  " '-I../source4/heimdal/lib/gssapi/gssapi'" + -
  " '-Idefault/source4/heimdal/lib/gssapi/spnego'" + -
  " '-I../source4/heimdal/lib/gssapi/spnego'" + -
  " '-Idefault/source4/heimdal/lib/gssapi/krb5'" + -
  " '-I../source4/heimdal/lib/gssapi/krb5'" + -
  " '-Idefault/source4/heimdal/lib/gssapi/mech'"
$ longfx = longfx + " '-I../source4/heimdal/lib/gssapi/mech'" + -
  " '-Idefault/libds/common' '-I../libds/common'" + -
  " '-Idefault/source4/libcli/smb2' '-I../source4/libcli/smb2'" + -
  " '-Idefault/source4/lib/messaging' '-I../source4/lib/messaging'" + -
  " '-Idefault/source3/librpc' '-I../source3/librpc'"
$ longfx = longfx + " '-Idefault/source4/lib/registry'" + -
  " '-I../source4/lib/registry' '-Idefault/libcli/cldap'" + -
  " '-I../libcli/cldap' '-Idefault/auth' '-I../auth'" + -
  " '-Idefault/libcli/smb' '-I../libcli/smb'" + -
  " '-Idefault/source4/libcli/wbclient' '-I../source4/libcli/wbclient'"
$ longfx = longfx + " '-Idefault/dynconfig' '-I../dynconfig'" + -
  " '-Idefault/source3/param' '-I../source3/param'" + -
  " '-Idefault/lib/compression' '-I../lib/compression'" + -
  " '-Idefault/source3/lib/unix_msg' '-I../source3/lib/unix_msg'" + -
  " '-Idefault/lib/crypto'"
$ longfx = longfx + " '-I../lib/crypto' '-Idefault/third_party/zlib'" + -
  " '-I../third_party/zlib' '-Idefault/libcli/smbreadline'" + -
  " '-I../libcli/smbreadline' '-Idefault/lib/smbconf'" + -
  " '-I../lib/smbconf' '-Idefault/libcli/samsync'" + -
  " '-I../libcli/samsync' '-Idefault/libcli/lsarpc'"
$ longfx = longfx + "'-I../libcli/lsarpc' '-Idefault/source4/lib/stream'" + -
  " '-I../source4/lib/stream' '-I/usr/local/include' '-D_SAMBA_BUILD_=4'" + -
  " '-DHAVE_CONFIG_H=1' '-D_GNU_SOURCE=1' '-D_XOPEN_SOURCE_EXTENDED=1'"
$ cflags = longf + " -DTEST_TXT_DEFINE=puts"
$ expect_prog_out = "Hello World!"
$ gosub compile_driver
$ return
$!
$cc_multiple_src:
$ testcase_name = "cc_multiple_src"
$ ! cc -o calc calc.c calc-lex.c calc-main.c
$ ! %link-f-opening calc-lex.o
$ gosub create_test_hello_c
$ gosub create_hello_sub_c
$ cflags = "-o test_hello hello_sub.c"
$ efile = "test_hello."
$ gosub compile_driver
$ dfile = "hello_sub.o"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ dfile = "hello_sub.lis"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ dfile = "hello_sub.c"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ return
$!
$cpp_cxx_default_opts:
$ testcase_name = "cpp_cxx_default_opts"
$ ! cpp cxxtest.cc
$ ! %DCL-W-IVKEYW - unrecognized NOVAXC_KEYWORDS
$ test = test + 1
$ gosub create_test_hello_cc
$ cmd = test_cpp_cmd
$ noexe = 1
$ no_efile = 1
$ options = "GNV_CC_QUALIFIERS=/ACCEPT=NOVAXC_KEYWORDS"
$ gosub compile_driver
$ return
$!
$!
$ld_auto_opt_file:
$! write sys$output "LD with gnv$<xxx>.opt file"
$ testcase_name = "ld_auto_opt_file"
$ test = test + 1
$ gosub create_hello_sub_c
$ cc hello_sub
$ gosub create_test_hello_c
$ efile = file
$ create gnv$'file'.opt
sys$disk:[]hello_sub.obj
$ cflags = "-DTEST_TXT_DEFINE=hello_sub -o ''efile'"
$ options = "GNV_OPT_DIR=."
$ gosub compile_driver
$ dfile = "hello_sub.obj"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ dfile = "hello_sub.lis"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ dfile = "hello_sub.c"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ dfile = "gnv$''file'.opt"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ return
$!
$ld_auto_opt2_file:
$! write sys$output "LD with gnv$link_options.opt file"
$ testcase_name = "ld_auto_opt2_file"
$ test = test + 1
$ gosub create_hello_sub_c
$ cc hello_sub
$ gosub create_test_hello_c
$ efile = file
$ create gnv$link_options.opt
sys$disk:[]hello_sub.obj
$ cflags = "-DTEST_TXT_DEFINE=hello_sub -o ''efile'"
$ options = "GNV_OPT_DIR=."
$ gosub compile_driver
$ dfile = "hello_sub.obj"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ dfile = "hello_sub.lis"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ dfile = "hello_sub.c"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ dfile = "gnv$''file'.opt"
$ if f$search(dfile) .nes. "" then delete 'dfile';*
$ return
$!
$!
$ld_link_map:
$ testcase_name = "ld_link_map"
$ test = test + 1
$ gosub create_test_hello_c
$ link := link
$ options = "GNV_LINK_MAP=1"
$ expect_link_map = 1
$ efile = "test_hello"
$ cflags = "-o test_hello"
$ gosub compile_driver
$ delete/symbol/local link
$ return
$!
$!
$cc_first_module_dir:
$ testcase_name = "cc_first_module_dir"
$ test = test + 1
$ gosub create_test_hello_c
$ options = "GNV_CC_FIRST_INCLUDE=."
$ gosub compile_driver
$ return
$!
$!
$missing_objects:
$ testcase_name = "missing_objects"
$ test = test + 1
$ gosub create_test_hello_c
$ more_files = "missing.o"
$ expect_uf = 1
$ no_efile = 1
$ cflags = ""
$ expect_cc_status = %x35a011
$ out_file = "test_cc_prog.out"
$ gosub compile_driver
$ return
$!
$missing_shared_images:
$ testcase_name = "missing_shared_images"
$ test = test + 1
$ gosub create_test_hello_c
$ more_files = "missing.so"
$ expect_uf = 1
$ no_efile = 1
$ cflags = ""
$ expect_cc_status = %x35a011
$ cc_out_file = "test_cc.out"
$ gosub compile_driver
$ return
$!
$missing_library:
$ testcase_name = "missing_library"
$ test = test + 1
$ gosub create_test_hello_c
$ more_files = "missing.a"
$ expect_uf = 1
$ no_efile = 1
$ cflags = ""
$ expect_cc_status = %x35a011
$ gosub compile_driver
$ return
$!
$!
$cc_command_file:
$ testcase_name = "cc_command_file"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "test_hello.o"
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
$ return
$!
$!
$ld_gnv_shared_logical:
$ testcase_name = "ld_gnv_shared_logical"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "test_hello.exe"
$ cc/object=test_hello_shr.o 'cfile'
$ link/share=test_hello_shr.exe test_hello_shr.o
$ define gnv$libtest_hello_shr sys$disk:[]test_hello_shr.exe
$ cflags = "-o test_hello.exe -ltest_hello_shr"
$ lstfile = "sys$disk:[]" + file + ".lis"
$ if f$search(lstfile) .nes. "" then delete 'lstfile';*
$ gosub compile_driver
$ obj_file = "sys$disk:[]" + file + "_shr.o"
$ if f$search(obj_file) .nes. "" then delete 'obj_file';*
$ exe_shr1 = "sys$disk:[]" + file + "_shr.exe"
$ if f$search(exe_shr1) .nes. "" then delete 'exe_shr1';*
$ map_shr1 = "sys$disk:[]" + file + "_shr.map"
$ if f$search(map_shr1) .nes. "" then delete 'map_shr1';*
$ dsf_shr1 = "sys$disk:[]" + file + "_shr.dsf"
$ if f$search(dsf_shr1) .nes. "" then delete 'dsf_shr1';*
$ deassign gnv$libtest_hello_shr
$ return
$!
$ld_gnv_shared_logical2:
$ testcase_name = "ld_gnv_shared_logical2"
$ test = test + 1
$ gosub create_test_hello_c
$ efile = "test_hello.exe"
$ cc/object=test_hello_shr.o 'cfile'
$ link/share=test_hello_shr.exe test_hello_shr.o
$ define gnv$libtest_hello_shr sys$disk:[]test_hello_shr.exe
$ cflags = "-o test_hello.exe libtest_hello_shr.a"
$ lstfile = "sys$disk:[]" + file + ".lis"
$ if f$search(lstfile) .nes. "" then delete 'lstfile';*
$ gosub compile_driver
$ obj_file = "sys$disk:[]" + file + "_shr.o"
$ if f$search(obj_file) .nes. "" then delete 'obj_file';*
$ exe_shr1 = "sys$disk:[]" + file + "_shr.exe"
$ if f$search(exe_shr1) .nes. "" then delete 'exe_shr1';*
$ map_shr1 = "sys$disk:[]" + file + "_shr.map"
$ if f$search(map_shr1) .nes. "" then delete 'map_shr1';*
$ dsf_shr1 = "sys$disk:[]" + file + "_shr.dsf"
$ if f$search(dsf_shr1) .nes. "" then delete 'dsf_shr1';*
$ deassign gnv$libtest_hello_shr
$ return
$!
$cc_unknown_type1:
$ testcase_name = "cc_unknown_type1"
$ test = test + 1
$ gosub create_test_hello_s
$ ! Unknown types with no helper file
$ cmd = test_cc_cmd
$ cflags = "-o test_hello.o -I. -Iinclude -DHAVE_CONFIG.H -c"
$ expect_uf = 1
$ noexe = 1
$ no_efile = 1
$ gosub compile_driver
$return
$!
$cc_unknown_type2:
$ testcase_name = "cc_unknown_type2"
$ test = test + 1
$ gosub create_test_hello_s
$ ! Unknown types with a helper file
$ ! CCAS = cc
$ ! DEFS = "-DHAVE_CONFIG_H"
$ ! DEFAULT_INCLUDES = "-I. -I$(srcdir)
$ ! INCLUDES = ""
$ ! AM_CPPFLAGS = "-I. -I$(top_srcdir)/include -Iinclude -I$(top_srcdir)/src"
$ ! CPPFLAGS = ""
$ ! AM_CASFLAGS = $(AM_CPPFLAGS)
$ ! CASFLAGS = ""
$ !CPPASCOMPILE = $(CCAS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
$!        $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CCASFLAGS) $(CCASFLAGS)
$!LTCPPASCOMPILE = $(LIBTOOL) $(AM_V_lt) $(AM_LIBTOOLFLAGS) \
$!        $(LIBTOOLFLAGS) --mode=compile $(CCAS) $(DEFS) \
$!        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
$!        $(AM_CCASFLAGS) $(CCASFLAGS)
$!
$! $(AM_V_CPPAS)$(LTCPPASCOMPILE) -c -o $@ $<
$ builder = "sys$disk:[]gnv$test_hello_o.com"
$ create 'builder'
$ open/append bxx 'builder'
$ write bxx "$write sys$output " + -
  """cc/obj=sys$disk:test_hello.o sys$disk:[]test_hello.s"""
$ close bxx
$ cmd = test_cc_cmd
$ options = "GNV_EXT_BUILDER=1"
$ cflags = "-o test_hello.o -I. -Iinclude -DHAVE_CONFIG.H -c"
$ noexe = 1
$ no_efile = 1
$ gosub compile_driver
$ if f$search(builder) .eqs. "" then delete 'builder';*
$return
$!
$! Generic Compiler driver
$!-------------------------
$compile_driver:
$ write sys$output "''testcase_name'"
$ write junit "   <testcase name=""''testcase_name'"""
$ write junit "    classname=""ld_tools.test"">"
$ gosub directory_before
$! on warning then set ver
$ if f$type(noexe) .eqs. "" then noexe = 0
$ cmd_symbol = cmd
$ unix_c_file = cfile - "^"
$ if cflags .nes. "" then cmd_symbol = cmd_symbol + " " + cflags
$ if unix_c_file .nes. "" then cmd_symbol = cmd_symbol  + " " + unix_c_file
$ if more_files .nes. "" then cmd_symbol = cmd_symbol + " " + more_files
$ cmd_symbol = f$edit(cmd_symbol, "trim")
$ cmd_symbol_trunc = f$extract(0, 80, cmd_symbol)
$! show symbol cmd_symbol
$ write sys$output "  command: """, cmd_symbol_trunc, """"
$ if cfile .eqs. "-"
$ then
$   ! Can't use cc_out_file here for now.
$   cc_out_file = ""
$   simple_c = "int main(int argc, char **argv) {return 0;}"
$   pipe write sys$output simple_c -
| execv 'test_cc' cmd_symbol
$ else
$   if arch_code .nes. "V" then set process/parse=traditional
$   if cc_out_file .nes. "" then define/user sys$output 'cc_out_file'
$   if options .nes. ""
$   then
$       execv 'test_cc' cmd_symbol options
$   else
$       execv 'test_cc' cmd_symbol
$   endif
$ endif
$ cc_status = '$status' .and. (.not. %x10000000)
$! type 'lstfile'
$ if arch_code .nes. "V" then set process/parse=extended
$ gnv_temp_list_file = f$search("gnv$cc_stdin_*.lis")
$ if gnv_temp_list_file .nes. ""
$ then
$   delete 'gnv_temp_list_file'
$ endif
$ gnv_temp_file = f$search("gnv$cc_stdin_*.*")
$ if gnv_temp_file .nes. ""
$ then
$   fail_type = "cleanup"
$   fail_msg = "''gnv_temp_file' temp file left behind"
$   gosub report_failure
$   dir gnv$cc_stdin*.*
$   del gnv$cc_stdin_*.*;*
$ endif
$ if cc_status .ne. expect_cc_status
$ then
$   fail_type = "compile"
$   fail_msg = "CC status ''cc_status' is not ''expect_cc_status'!"
$   gosub report_failure
$   type 'cc_out_file'
$ endif
$ if f$search("*.c_defines") .nes. ""
$ then
$   fail_type = "compile"
$   fail_msg = "Found leftover *.c_defines file!"
$   gosub report_failure
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
$      fail_type = "compile"
$      fail_msg = "Did not find ''search_listing' in listing."
$      gosub report_failure
$   endif
$ endif
$ if cc_out_file .nes. ""
$ then
$   if f$search(cc_out_file) .nes. ""
$   then
$       if p1 .eqs. "DEBUG"
$       then
$           type 'cc_out_file'
$       endif
$       open/read xx 'cc_out_file'
$       line_in = ""
$       read/end=read_xx1 xx line_in
$read_xx1:
$       close xx
$       if expect_cc_out .nes. ""
$       then
$           if f$locate(expect_cc_out, line_in) .ne. 0
$           then
$               fail_type = "compile"
$               fail_msg = "''expect_cc_prog_out' not found in output"
$               gosub report_failure
$               type 'cc_out_file'
$           endif
$       endif
$       if .not. expect_uf
$       then
$           if f$locate("Unrecognized file", line_in) .lt. f$length(line_in)
$           then
$               fail_type = "compile"
$               fail_msg = "Unrecognized file seen in output"
$               gosub report_failure
$               type 'cc_out_file'
$           endif
$       endif
$       if .not. expect_usw
$       then
$           if f$locate("Unrecognized switch", line_in) .lt. f$length(line_in)
$           then
$               fail_type = "compile"
$               fail_msg = "Unrecognized switch seen in output"
$               gosub report_failure
$               type 'cc_out_file'
$           endif
$       endif
$       if .not. expect_uopt
$       then
$           if f$locate("unrecognized option", line_in) .lt. f$length(line_in)
$           then
$               fail_type = "compile"
$               fail_msg = "Unrecognized option seen in output"
$               gosub report_failure
$               type 'cc_out_file'
$           endif
$       endif
$       if lcl_fail .gt. 1 then type 'cc_out_file'
$       delete 'cc_out_file';*
$   else
$       fail_type = "compile"
$       fail_msg = "Output file not created"
$       gosub report_failure
$   endif
$ endif
$ unix_status = 0
$ if (cc_status .and. %x35a000) .eq. %x35a000
$ then
$    unix_status = (cc_status / 8) .and. 255
$ endif
$ if f$search(efile) .eqs. ""
$ then
$    if unix_status .eq. 0 .and. no_efile .eq. 0
$    then
$       fail_type = "link"
$       fail_msg = "Executable not produced"
$       gosub report_failure
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
$       set noon
$       if prog_status .nes. expect_prog_status
$       then
$           fail_type = "link"
$           fail_msg = -
                  "Program status ''prog_status' is not ''expect_prog_status'"
$           gosub report_failure
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
$                  fail_type = "link"
$                  fail_msg = "''expect_prog_out' not found in output"
$                  gosub report_failure
$                  write sys$output "  ** Got -''line_in'-"
$               endif
$               delete 'out_file';*
$           else
$               fail_type = "link"
$               fail_msg = "Output file not created"
$               gosub report_failure
$           endif
$       endif
$       if expect_link_map .and. f$search(mapfile) .eqs. ""
$       then
$           fail_type = "link"
$           fail_msg = "Map file not created"
$           gosub report_failure
$       endif
$   endif
$   if no_efile
$   then
$       fail_type = "compile"
$       fail_msg = "Object produced when none expected"
$       gosub report_failure
$   endif
$   delete 'efile';*
$ endif
$ if f$search(ofile) .nes. "" then delete 'ofile';*
$ if f$search(lstfile) .nes. "" then delete 'lstfile';*
$ if f$search(mapfile) .nes. "" then delete 'mapfile';*
$ if f$search(dsffile) .nes. "" then delete 'dsffile';*
$ if f$search("hello_sub.obj") .eqs. ""
$ then
$   extra_file = "hello_sub.o"
$   if f$search(extra_file) .nes. "" then delete 'extra_file';*
$   extra_file = "hello_sub.lis"
$   if f$search(extra_file) .nes. "" then delete 'extra_file';*
$ endif
$ gosub directory_after
$ if lcl_fail .ne. 0 then fail = fail + 1
$ write junit "   </testcase>"
$ ! cleanup
$ gosub test_clean_up
$ gosub clean_params
$ return
$!
$report_failure:
$ write sys$output "  ** ''fail_msg', type: ''fail_type'"
$ write junit "      <failure message=""''fail_msg'"" type=""''fail_type'"" >"
$ write junit "      </failure>
$ lcl_fail = lcl_fail + 1
$ return
$!
$skipped_test_driver:
$ write sys$output "Skipping test ''testcase_name' reason: ''skip_reason'."
$ write junit "   <testcase name=""''testcase_name'"""
$ write junit "    classname=""ld_tools.test"">"
$ write junit "      <skipped/>"
$ write junit "   </testcase>"
$ return
$!
$clean_params:
$ cmd = ld_cmd
$ file = "test_hello"
$ cfile = file + ".c"
$ lstfile = file + ".lis"
$ mapfile = file + ".map"
$ dsffile = file + ".dsf"
$ ofile = file + ".o"
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
$ options = ""
$ expect_cc_out = ""
$ expect_link_map = 0
$ noexe = 0
$ no_efile = 0
$ expect_prog_out = "Hello World"
$ search_listing = ""
$ if f$search("*.c_defines") .nes. "" then delete *.c_defines;*
$ return
$!
$test_clean_up:
$ noexe = 1
$ if f$search(cfile) .nes. "" then delete 'cfile';
$ return
$!
$!
$!
$directory_before:
$!
$ if f$search("sys$disk:test_ld_before.txt") .nes. ""
$ then
$   delete sys$disk:test_ld_before.txt;*
$ endif
$ if f$search("sys$disk:test_ld_after.txt") .eqs. ""
$ then
$   ! Add a file so that the directory compare matches
$   create sys$disk:test_ld_after.txt
$ endif
$ directory := directory
$ directory/out=sys$disk:test_ld_before.txt sys$disk:[...];
$ return
$!
$directory_after:
$!
$ if f$search("sys$disk:test_ld_after.txt") .nes. ""
$ then
$   delete sys$disk:test_ld_after.txt;*
$ endif
$ directory := directory
$ directory/out=sys$disk:test_ld_after.txt sys$disk:[...];
$!
$ diff := diff
$ define/user sys$output nla0:
$ diff sys$disk:test_ld_before.txt sys$disk:test_ld_after.txt
$ diff_status = $severity
$ if diff_status .ne. 1
$ then
$   fail_type = "cleanup"
$   fail_msg = "Directory contents change after test."
$   gosub report_failure
$   diff sys$disk:test_ld_before.txt sys$disk:test_ld_after.txt
$ endif
$!
$ if f$search("sys$disk:test_ld_before.txt") .nes. ""
$ then
$   delete sys$disk:test_ld_before.txt;*
$ endif
$ if f$search("sys$disk:test_ld_after.txt") .nes. ""
$ then
$   delete sys$disk:test_ld_after.txt;*
$ endif
$ return
$!
$!
$! Create a test c file
$create_test_hello_c:
$ cfile = file + ".c"
$ if f$search(cfile) .nes. "" then delete 'cfile';
$ create 'cfile'
#include <stdio.h>

int hello_sub(const char * str);

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
#   ifndef TEST_DEFINE2
      puts("Hello World!");
      return 0;
#   endif
# endif
#endif
}
$ return
$!
$!
$! Create a test subroutine file
$create_hello_sub_c:
$ tcfile = "hello_sub.c"
$ if f$search(tcfile) .nes. "" then delete 'tcfile';
$ create 'tcfile'
#include <stdio.h>

int hello_sub(const char * str) {
    puts(str);
    return 0;
}
$ return
$!
$! Create a test cc file
$create_test_hello_cc:
$ cfile = file + ".cc"
$ if f$search(cfile) .nes. "" then delete 'cfile';
$ create 'cfile'
#include <iostream.h>

int main(int argc, char **argv) {
#ifdef Test_NDEFINE
    cout << Test_NDEFINE;
    return 1;
#endif
#ifdef TEST_DEFINE
    cout << TEST_DEFINE;
    return 1;
#endif
#ifdef TEST_TXT_DEFINE
    TEST_TXT_DEFINE << "Hello World!";
#else
# if !defined(TEST_DEFINE) && !defined(Test_NDEFINE) && !defined(NDEBUG)
#   ifndef TEST_DEFINE2
      cout << "Hello World!";
      return 0;
#   endif
# endif
#endif
}
$ return
$!
$!
$!
$!
$! Create a test c file
$create_test_hello_s:
$ cfile = file + ".s"
$ if f$search(cfile) .nes. "" then delete 'cfile';
$ create 'cfile'
#include <stdio.h>

int main(int argc, char **argv) {
    puts("Hello World!");
    return 0;
}
$ return
