param(
    [int]$Rounds = 150,
    [string]$ProjectDir = $PSScriptRoot,
    [string]$CaseDir = "",
    [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'

function Resolve-CaseDir {
    param([string]$Preferred, [string]$ProjectDir)

    if ($Preferred -and (Test-Path -LiteralPath $Preferred)) {
        return (Resolve-Path -LiteralPath $Preferred).Path
    }

    $candidates = @(
        (Join-Path $ProjectDir 'tmp\cases'),
        'D:\C++\HIS_C\qa_xitong\tmp'
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "找不到测试用例目录。请指定 -CaseDir。"
}

$projectDirResolved = (Resolve-Path -LiteralPath $ProjectDir).Path
$dataDir = Join-Path $projectDirResolved 'data'
$tmpDir = Join-Path $projectDirResolved 'tmp'
$exePath = Join-Path $projectDirResolved 'his_system.exe'
$summaryPath = Join-Path $tmpDir 'stress_summary.txt'
$baselineDir = Join-Path $tmpDir 'baseline_data_stress'

if (!(Test-Path -LiteralPath $dataDir)) { throw "缺少 data 目录：$dataDir" }
if (!(Test-Path -LiteralPath $tmpDir)) { New-Item -ItemType Directory -Path $tmpDir | Out-Null }

if ($Rebuild) {
    mingw32-make -C $projectDirResolved clean all | Out-Host
}
if (!(Test-Path -LiteralPath $exePath)) { throw "找不到可执行文件：$exePath" }

$caseDirResolved = Resolve-CaseDir -Preferred $CaseDir -ProjectDir $projectDirResolved

$preferredCases = @(
    't_baseline_exit.in',
    'invalid_main_input_then_exit.in',
    'admin_wrong_password.in',
    'add_record_invalid_patient.in',
    'modify_nonexistent_record.in',
    'delete_record_twice.in',
    'query_invalid_date.in',
    'query_reversed_date.in',
    'import_mixed_file.in'
)

$cases = @()
foreach ($name in $preferredCases) {
    $full = Join-Path $caseDirResolved $name
    if (Test-Path -LiteralPath $full) { $cases += $full }
}
if ($cases.Count -eq 0) {
    $cases = Get-ChildItem -LiteralPath $caseDirResolved -Filter '*.in' -File | Select-Object -ExpandProperty FullName
}
if ($cases.Count -eq 0) { throw "未找到 .in 用例文件：$caseDirResolved" }

$importMixed = Join-Path $caseDirResolved 'import_mixed.txt'
if (Test-Path -LiteralPath $importMixed) {
    Copy-Item -LiteralPath $importMixed -Destination (Join-Path $tmpDir 'import_mixed.txt') -Force
}

if (Test-Path -LiteralPath $baselineDir) {
    Remove-Item -LiteralPath $baselineDir -Recurse -Force
}
New-Item -ItemType Directory -Path $baselineDir | Out-Null
Copy-Item -LiteralPath (Join-Path $dataDir '*') -Destination $baselineDir -Force

Get-ChildItem -LiteralPath $tmpDir -Filter 'stress*_run_*.out.txt' -File -ErrorAction SilentlyContinue | Remove-Item -Force

$fails = New-Object System.Collections.Generic.List[string]
for ($i = 1; $i -le $Rounds; $i++) {
    Copy-Item -LiteralPath (Join-Path $baselineDir '*') -Destination $dataDir -Force

    $casePath = $cases[($i - 1) % $cases.Count]
    $outPath = Join-Path $tmpDir ("stress150_run_{0:D3}.out.txt" -f $i)

    $cmd = '"' + $exePath + '" < "' + $casePath + '" > "' + $outPath + '"'
    cmd /c $cmd | Out-Null
    $code = $LASTEXITCODE

    if ($code -ne 0) {
        $fails.Add(("RUN {0:D3} EXIT={1} CASE={2}" -f $i, $code, [IO.Path]::GetFileName($casePath)))
        continue
    }

    $text = Get-Content -LiteralPath $outPath -Raw -ErrorAction SilentlyContinue
    if ($text -match 'Access violation|Segmentation fault|Assertion failed|stack smashing') {
        $fails.Add(("RUN {0:D3} CRASH_TEXT CASE={1}" -f $i, [IO.Path]::GetFileName($casePath)))
    }
}

$summary = @(
    "Project=$projectDirResolved",
    "CaseDir=$caseDirResolved",
    "Rounds=$Rounds",
    "Fails=$($fails.Count)",
    "Time=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
)
if ($fails.Count -gt 0) {
    $summary += '---- Failures ----'
    $summary += $fails
}
Set-Content -LiteralPath $summaryPath -Value $summary -Encoding UTF8

$summary | ForEach-Object { Write-Output $_ }

if ($fails.Count -gt 0) { exit 1 }
exit 0
