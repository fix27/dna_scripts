@echo off

REM Пережатие .vcf.gz файлов
REM Требуются bgzip.exe (из htslib) и gzip.exe (например, из GnuWin32 или Cygwin)

setlocal enabledelayedexpansion

for %%F in (*.vcf.gz) do (
    echo Обрабатываем: %%F

    REM Проверим, похоже ли содержимое на VCF
    gzip -cd "%%F" | findstr /B "##" >nul 2>&1
    if !errorlevel! == 0 (
        echo  -> Похоже на VCF, сжатый bgzip. Пересжимаем через bgzip...
        bgzip -@ 4 -l 9 -c "%%F" > "%%~nF.repacked.vcf.gz"
    ) else (
        echo  -> Обычный .gz. Пересжимаем через gzip -9...
        gzip -c -9 < "%%F" > "%%~nF.repacked.vcf.gz"
    )
)

echo Готово!
