# Пересжатие обычных .gz с максимальным уровнем
#!/usr/bin/env bash
# Пережатие .vcf.gz с сохранением совместимости

set -euo pipefail

infile="$1"
outfile="${2:-${infile%.vcf.gz}.repacked.vcf.gz}"

echo "Пересжимаем через bgzip..."
bgzip -@ 8 -l 9 -c "$infile" > "$outfile"

echo "Готово: $outfile"
