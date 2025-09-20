samtools index sample.cram

# делаем pileup (просмотр выравнивания) и вызываем варианты
bcftools mpileup -Ou -f reference.fa sample.cram | bcftools call -mv -Oz -o output.vcf.gz

# индексируем итоговый VCF
bcftools index output.vcf.gz