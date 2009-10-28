@echo off

echo Creating file type and associate it with *.obj and *.ser files
assoc .obj=LC3_object
assoc .ser=LC3_object
ftype LC3_object="c:\lc3\bin\lc3Upload.bat" "%%1"
ftype LC3_object="c:\lc3\bin\lc3Terminal.exe" "%%1"

echo Creating "SendTo" menu item
cp lc3Terminal.lnk "%USERPROFILE%\SendTo\"
cp lc3Upload.lnk "%USERPROFILE%\SendTo\"

