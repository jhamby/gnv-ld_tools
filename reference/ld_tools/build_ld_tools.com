$! File: build_ld_tools.com
$!
$!
$ if p1 .eqs. "CLEAN" then goto clean
$ if p1 .eqs. "REALCLEAN" then goto realclean
$!
$ if "''cc'" .eqs. ""
$ then
$   cc = "cc"
$ else
$   cc = cc
$ endif
$
$ cc = cc + "/DEFINE=(__COE_SYSTEM,_POSIX_EXIT,_VMS_WAIT)"
$ on warning then goto no_main_qual
$ msgsetting = f$environment("message")
$ ! hush, we are just testing.
$ set message /nofacility/noseverity/noidentification/notext
$ cc /OBJECT=NL: as.c /MAIN=POSIX_EXIT
$ cc = cc + "/MAIN=POSIX_EXIT"
$ cc = cc + "/names=(as_is,shortened)/repository=sys$disk:[]"
$ no_main_qual:
$ on error then exit	! restore default
$ set message 'msgsetting'	! restore message
$
$ if p1 .eqs. "DEBUG"
$ then
$   write sys$output "Adding /noopt/debug to CC command"
$   cc = cc + "/NOOPT/DEBUG"
$   write sys$output "Adding /debug to LINK command"
$   link = "LINK/DEBUG"
$ endif
$ on error then goto done
$ set verify
$ cc ld.c
$ if f$getsyi ("arch_name") .eqs. "IA64"
$ then
$   cc /point=long /define=(__NEW_STARLET) elf.c
$ endif
$ cc utils.c
$ if f$search ("vms_crtl_init.obj") .eqs. ""
$ then
$   cc vms_crtl_init.c/obj=vms_crtl_init.obj
$ endif
$ if f$search ("elf.obj") .nes. ""
$ then
$   link/exe=gnv$ld.exe ld,elf,utils,vms_crtl_init
$   link/debug/exe=gnv$debug-ld.exe ld,elf,utils,vms_crtl_init
$ else
$   link/exe=gnv$ld.exe ld,utils,vms_crtl_init
$   link/debug/exe=gnv$debug-ld.exe ld,utils,vms_crtl_init
$ endif
$done:
$ set noverify
$!
$exit
$!
$realclean:
$clean:
$  file="gnv$ld.exe"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="gnv$ld.map"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="gnv$debug-ld.exe"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="gnv$debug-ld.map"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="ld.obj"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="ld.lis"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="utils.obj"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="utils.lis"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="elf.obj"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="elf.lis"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="vms_crtl_init.obj"
$  if f$search(file) .nes. "" then delete 'file';*
$  file="vms_crtl_init.lis"
$  if f$search(file) .nes. "" then delete 'file';*
$exit
