$! File: backup_ld_tools_src.com
$!
$! Procedure to create backup save sets for installing in a PCSI kit.
$!
$! To comply with most Open Source licenses, the source used for building
$! a kit will be packaged with the distribution kit for the binary.
$!
$! Backup save sets are the only storage format that I can expect a
$! VMS system to be able to extract ODS-5 filenames and directories.
$!
$! The make_pcsi_kit_name.com needs to be run before this procedure to
$! properly name the files that will be created.
$!
$! This file is created from a template file for the purpose of making it
$! easier to port Unix code, particularly open source code to VMS.
$! Therefore permission is freely granted for any use.
$!
$! 23-Jul-2014 J. Malmberg
$!
$!===========================================================================
$!
$! Save default
$ default_dir = f$environment("DEFAULT")
$!
$ arch_type = f$getsyi("ARCH_NAME")
$ arch_code = f$extract(0, 1, arch_type)
$!
$ if arch_code .nes. "V"
$ then
$   set proc/parse=extended
$ endif
$!
$ ss_abort = 44
$ status = ss_abort
$!
$ kit_name = f$trnlnm("GNV_PCSI_KITNAME")
$ if kit_name .eqs. ""
$ then
$   write sys$output "@MAKE_PCSI_LD_TOOLS_KIT_NAME.COM has not been run."
$   goto all_exit
$ endif
$ producer = f$trnlnm("GNV_PCSI_PRODUCER")
$ if producer .eqs. ""
$ then
$   write sys$output "@MAKE_PCSI_LD_TOOLS_KIT_NAME.COM has not been run."
$   goto all_exit
$ endif
$ filename_base = f$trnlnm("GNV_PCSI_FILENAME_BASE")
$ if filename_base .eqs. ""
$ then
$   write sys$output "@MAKE_PCSI_LD_TOOL_KIT_NAME.COM has not been run."
$   goto all_exit
$ endif
$!
$ node_swvers = f$getsyi("NODE_SWVERS")
$ node_swvers_type = f$extract(0, 1, node_swvers)
$ node_swvers_vers = f$extract(1, f$length(node_swvers), node_swvers)
$ swvers_maj = f$element(0, ".", node_swvers_vers)
$ node_swvers_min_update = f$element(1, ".", node_swvers_vers)
$ swvers_min = f$element(0, "-", node_swvers_min_update)
$ swvers_update = f$element(1, "-", node_swvers_min_update)
$!
$ if swvers_update .eqs. "-" then swvers_update = ""
$!
$ vms_vers = f$fao("!2ZB!2ZB!AS", 'swvers_maj', 'swvers_min', swvers_update)
$!
$!
$!
$! If available make an interchange save set
$!-------------------------------------------
$ interchange = ""
$ if arch_code .eqs. "V"
$ then
$   interchange = "/interchange"
$ endif
$ if (swvers_maj .ges. "8") .and. (swvers_min .ges. 4)
$ then
$   interchange = "/interchange/noconvert"
$ endif
$!
$!
$! Put things back on error.
$ on warning then goto all_exit
$!
$ current_default = f$environment("DEFAULT")
$ my_dir = f$parse(current_default,,,"DIRECTORY") - "[" - "<" - ">" - "]"
$!
$ src_root = "src_root:"
$ if f$trnlnm("src_root1") .nes. "" then src_root = "src_root1:"
$ backup'interchange' 'src_root'[ld_tools...]*.*;0 -
           'filename_base'_original_src.bck/sav
$ status = $status
$!
$! There may be a VMS specific source kit
$!-----------------------------------------
$ vms_root = "vms_root:"
$ if f$trnlnm("vms_root1") .nes. "" then vms_root = "vms_root1:"
$ files_found = 0
$ define/user sys$error nl:
$ define/user sys$output nl:
$ directory 'vms_root'[...]*.*;*/exc=*.dir
$ if '$severity' .eq. 1 then files_found = 1
$!
$ if files_found .eq. 1
$ then
$   backup'interchange' 'vms_root'[ld_tools...]*.*;0 -
            'filename_base'_vms_src.bck/sav
$   status = $status
$ endif
$!
$all_exit:
$ set def 'default_dir'
$ exit
