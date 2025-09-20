@echo off

REM Пережатие .vcf.gz файлов
REM Требуются bgzip.exe (из htslib) и gzip.exe (например, из GnuWin32 или Cygwin)

setlocal enabledelayedexpansion

for %%F in (*.vcf.gz) do (
    echo Обрабатываем: %%F
    echo  ->  Пересжимаем через bgzip...
    bgzip -@ 4 -l 9 -c "%%F" > "%%~nF.repacked.vcf.gz"

)

echo Готово!
