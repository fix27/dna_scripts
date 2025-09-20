@echo off

REM Пережатие .vcf.gz файлов
REM Требуются bgzip.exe (из htslib) и gzip.exe (например, из GnuWin32 или Cygwin)

setlocal enabledelayedexpansion

for %%F in (*.vcf.gz) do (
    echo Обрабатываем: %%F

    REM Проверим, похоже ли содержимое на VCF
        echo  -> Похоже на VCF, сжатый bgzip. Пересжимаем через bgzip...
	bgzip -d "%%F"
)
for %%F in (*.vcf) do (

        bgzip -l 9 -c "%%F" > "%%~nF.vcf.gz"
)
echo Готово!

