:: tested on windows 10
:: example: extrachain-make-archive.bat C:\Qt\5.15.2\msvc2019_64 C:\build

@echo off
echo Copy libs...
call "C:\Program Files (x86)\CQtDeployer\1.5\cqtdeployer.bat" -bin %2\extrachain-console.exe -qmake %1\bin\qmake.exe -fileLog cqtdeployer.log -verbose 0
:: copy %2\extrachain.dll .\DistributionKit\extrachain.dll
copy %2\libsodium.dll .\DistributionKit\libsodium.dll
copy %2\mpir.dll .\DistributionKit\mpir.dll
copy %2\sqlite3.dll .\DistributionKit\sqlite3.dll
echo Done

echo Generate archive...
ren DistributionKit ExtraChainConsole
"C:\Program Files\7-Zip\7z.exe" a -tzip ExtraChainConsole.zip ./ExtraChainConsole
rd /s /q ExtraChainConsole
del /q cqtdeployer.log

echo Generated ExtraChainConsole.zip
