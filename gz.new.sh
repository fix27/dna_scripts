# Пересжатие обычных .gz с максимальным уровнем
echo "Пересжатие обычных .gz ..."
find "$tmpdir" -type f -name '*.gz' \
  ! -iname '*.vcf.gz' \
  ! -iname '*.fa.gz' \
  ! -iname '*.fasta.gz' \
  ! -iname '*.bed.gz' \
  | while IFS= read -r gzfile; do
    base="${gzfile%.gz}"
    echo "  Пересжимаю: $gzfile"
    gunzip -c "$gzfile" | gzip -9 > "${base}.gz.new" && mv -f "${base}.gz.new" "$gzfile"
done
