@echo off

echo Creating file type and associate it with LC3 files
regedit /s c:\lc3\bin\lc3_filetypes.reg
REM ------ OBSOLETE: Now we use registers, because it is more flexible -------
REM assoc .obj
REM assoc .obj=LC3_object
REM ftype LC3_object="c:\lc3\bin\lc3Upload.cmd" "%%1"
REM ftype LC3_object="c:\lc3\bin\lc3Terminal.exe" "%%1"
REM 
REM assoc .asm=LC3_source
REM assoc .c=LC3_source
REM ftype LC3_source="c:\lc3\bin\lc3compile.cmd" "%%1"
REM --------------------------------------------------------------------------


echo Creating "SendTo" menu item
copy /y c:\lc3\bin\lc3Terminal.lnk "%USERPROFILE%\SendTo\"
copy /y c:\lc3\bin\lc3Upload.lnk "%USERPROFILE%\SendTo\"

echo Done
pause