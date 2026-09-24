$ErrorActionPreference = 'Stop'
$7z     = "C:\Users\fix12\scoop\shims\7z.exe"
$zstd   = "C:\Users\fix12\scoop\shims\zstd.exe"
$fqpack = "C:\Users\fix12\AppData\Local\Temp\opencode\fqpack-build\fqpack.exe"
$root   = "E:\tmp\fq"
$work   = "$root\work"
$zdir   = "$root\zstd"
$fdir   = "$root\fqpack"
$vdir   = "$root\verify"
$log    = "$root\compress2.log"

function Log([string]$m) { Add-Content -Path $log -Encoding ASCII -Value ("[{0}] {1}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $m) }

function Count-Lines([string]$p) {
    $lineCount = [int64]0
    $reader = [System.IO.StreamReader]::new($p)
    try {
        while ($reader.ReadLine() -ne $null) { $lineCount++ }
    } finally {
        $reader.Dispose()
    }
    return $lineCount
}

function HashFile([string]$p) {
    return (Get-FileHash -Path $p -Algorithm SHA256).Hash
}

New-Item -ItemType Directory -Path $work,$zdir,$fdir,$vdir -Force | Out-Null
Log "=== ORCHESTRATOR START ==="

function Process-File([string]$gz, [string]$base) {
    Log "--- FILE $base ---"
    $fq    = "$work\$base.fq"
    $outZ  = "$zdir\$base.fq.zst"
    $outF  = "$fdir\$base.fqp"
    $ver   = "$vdir\$base.fq"

    try {
        if (-not (Test-Path $fq)) {
            Log "decompress $base"
            $cmd = '"{0}" x -so "{1}" > "{2}"' -f $7z, $gz, $fq
            cmd /c $cmd | Out-Null
            if (-not (Test-Path $fq)) { throw "decompress produced no file" }
        } else { Log "work .fq already present, reuse" }
        $rawSize = (Get-Item $fq).Length
        Log "raw $base.fq bytes=$rawSize"

        if (-not (Test-Path $outZ) -or (Get-Item $outZ).Length -eq 0) {
            Log "zstd -15 compress $base (start)"
            & $zstd -15 -T0 -q --no-progress $fq -o $outZ
            if ($LASTEXITCODE -ne 0) { throw "zstd failed rc=$LASTEXITCODE" }
            Log "zstd done bytes=$((Get-Item $outZ).Length)"
        } else { Log "zstd output exists, skip" }
        Log "zstd -t integrity test"
        & $zstd -t -q $outZ
        if ($LASTEXITCODE -ne 0) { throw "zstd -t failed" }
        Log "zstd integrity OK"

        if (-not (Test-Path $outF) -or (Get-Item $outF).Length -eq 0) {
            Log "fqpack compress $base (start)"
            & $fqpack -f $fq $outF
            if ($LASTEXITCODE -ne 0) { throw "fqpack failed rc=$LASTEXITCODE" }
            Log "fqpack done bytes=$((Get-Item $outF).Length)"
        } else { Log "fqpack output exists, skip" }

        if (Test-Path $ver) { Remove-Item $ver -Force }
        Log "fqpack decompress verify $base (start)"
        & $fqpack -d -f $outF $ver
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $ver)) { throw "fqpack decompress verify failed" }
        $rawHash   = HashFile $fq
        $verHash   = HashFile $ver
        $verLines  = Count-Lines $ver
        $rawLines  = Count-Lines $fq
        Log "verify ${base}: rawSHA=$rawHash"
        Log "verify ${base}: outSHA=$verHash"
        Log "verify ${base}: lines raw=$rawLines out=$verLines records=$([math]::Floor([int64]$verLines/4)) equal=$($rawHash -eq $verHash)"
        if ($rawHash -ne $verHash) { throw "HASH MISMATCH for $base" }

        Remove-Item $ver -Force -ErrorAction SilentlyContinue
        Remove-Item $fq -Force -ErrorAction SilentlyContinue
        Log "--- FILE $base DONE (cleanup ok) ---"
    } catch {
        Log "ERROR processing $base : $($_.Exception.Message)"
        Log "--- FILE $base FAILED ---"
    }
}

Process-File "$root\NG15946AK0_1.fq.gz" "NG15946AK0_1"
Process-File "$root\NG15946AK0_2.fq.gz" "NG15946AK0_2"
Log "=== ORCHESTRATOR COMPLETE ==="
