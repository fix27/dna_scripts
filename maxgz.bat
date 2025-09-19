@echo off
setlocal enabledelayedexpansion

echo Пересжатие обычных .gz ...

for /r "%TMPDIR%" %%F in (*.gz) do (
    set "fname=%%~nxF"
    set "ext=%%~xF"
    set "name=%%~nF"
    set "path=%%~dpF"

    rem пропускаем vcf.gz, fa.gz, fasta.gz, bed.gz
    if /i not "%%~nxF"=="*.vcf.gz" if /i not "%%~nxF"=="*.fa.gz" if /i not "%%~nxF"=="*.fasta.gz" if /i not "%%~nxF"=="*.bed.gz" (
        echo   Пересжимаю: %%F
        rem распаковать
        7z x -y "%%F" -o"%path%" >nul
        del "%%F"

        rem снова упаковать в gzip с макс. сжатием
        7z a -tgzip -mx=9 "%path%!name!.gz" "%path%!name!" >nul

        rem удалить временный распакованный файл
        del "%path%!name!"
    )
)

echo Готово.
