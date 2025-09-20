# Пересжатие обычных .gz с максимальным уровнем
#!/usr/bin/env bash
# Пережатие .vcf.gz с сохранением совместимости

set -euo pipefail

infile="$1"
outfile="${2:-${infile%.vcf.gz}.repacked.vcf.gz}"

# Проверяем, bgzip ли это
if gunzip -c "$infile" 2>/dev/null | head -n 1 | grep -q '^##'; then
    # Сжатие именно bgzip
    echo "Файл $infile выглядит как VCF, сжатый bgzip."
    echo "Пересжимаем через bgzip..."
    bgzip -@ 8 -l 9 -c "$infile" > "$outfile"
else
    # Обычный gzip (не bgzip)
    echo "Файл $infile обычный .gz, не bgzip."
    echo "Пересжимаем через gzip -9..."
    gzip -c -9 <(gunzip -c "$infile") > "$outfile"
fi

echo "Готово: $outfile"
