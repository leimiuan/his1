param(
    [string]$ProjectDir = "d:\C++\his1-1\last_his-4-7-15-27"
)

$ErrorActionPreference = 'Stop'

$project = (Resolve-Path -LiteralPath $ProjectDir).Path
Set-Location $project

$tmpRoot = Join-Path $project 'tmp_fulltest_restart'
$outDir = Join-Path $tmpRoot 'outputs'
$caseDir = Join-Path $tmpRoot 'cases'
$backupDir = Join-Path $tmpRoot 'backup_data'
$reportPath = Join-Path $tmpRoot 'fulltest_report_restart.txt'
$exePath = Join-Path $project 'his_system_fulltest.exe'
$dataDir = Join-Path $project 'data'

if (!(Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
if (!(Test-Path -LiteralPath $caseDir)) { New-Item -ItemType Directory -Path $caseDir | Out-Null }

function DataCount([string]$filePath) {
    $lines = Get-Content -LiteralPath $filePath
    if ($lines.Count -le 1) { return 0 }
    return (($lines | Select-Object -Skip 1 | Where-Object { $_ -match '\S' }).Count)
}

function GetRowById([string]$filePath, [string]$id) {
    $row = Get-Content -LiteralPath $filePath | Select-Object -Skip 1 | Where-Object { $_ -like "$id`t*" } | Select-Object -First 1
    return $row
}

function GetPatientBalance([string]$patientId) {
    $row = GetRowById -filePath (Join-Path $dataDir 'outpatients.txt') -id $patientId
    if (-not $row) { throw "Patient not found in outpatients: $patientId" }
    $parts = $row -split "`t"
    if ($parts.Count -lt 7) { throw "Bad outpatients row format for $patientId" }
    return [double]$parts[6]
}

function RunCase([string]$name, [string]$inputText) {
    $inPath = Join-Path $caseDir ($name + '.in')
    $outPath = Join-Path $outDir ($name + '.out.txt')
    Set-Content -LiteralPath $inPath -Value $inputText -Encoding ASCII

    $cmd = '"' + $exePath + '" < "' + $inPath + '" > "' + $outPath + '"'
    cmd /c $cmd | Out-Null
    return @{ ExitCode = $LASTEXITCODE; OutPath = $outPath; InPath = $inPath }
}

function BuildExe() {
    Get-ChildItem src\*.c | ForEach-Object {
        & gcc -Iinclude -Wall -g -finput-charset=UTF-8 -fexec-charset=UTF-8 -c $_.FullName -o (Join-Path 'obj' ($_.BaseName + '.o'))
        if ($LASTEXITCODE -ne 0) { throw "Compile failed for $($_.Name)" }
    }
    & gcc obj\*.o -o $exePath
    if ($LASTEXITCODE -ne 0) { throw "Link failed for his_system_fulltest.exe" }
}

function SaveReport($results) {
    $total = $results.Count
    $pass = ($results | Where-Object { $_.Status -eq 'PASS' }).Count
    $fail = ($results | Where-Object { $_.Status -eq 'FAIL' }).Count

    $lines = @(
        "FULL_TEST_TOTAL=$total",
        "FULL_TEST_PASS=$pass",
        "FULL_TEST_FAIL=$fail",
        "TIME=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
        "---DETAIL---"
    )

    foreach ($r in $results) {
        $lines += "[$($r.Status)] $($r.Name) | $($r.Detail)"
    }

    Set-Content -LiteralPath $reportPath -Value $lines -Encoding UTF8
}

$results = New-Object System.Collections.Generic.List[object]
$createdPatientId = $null
$today = (Get-Date).ToString('yyyy-MM-dd')
$uniq = (Get-Date).ToString('HHmmss')
$testNurseId = "NF$uniq"
$testDoctorId = "D9$uniq"

function AddResult([string]$name, [bool]$ok, [string]$detail) {
    $status = if ($ok) { 'PASS' } else { 'FAIL' }
    $results.Add([pscustomobject]@{ Name = $name; Status = $status; Detail = $detail }) | Out-Null
}

if (Test-Path -LiteralPath $backupDir) {
    Remove-Item -LiteralPath $backupDir -Recurse -Force
}
New-Item -ItemType Directory -Path $backupDir | Out-Null
Copy-Item -LiteralPath (Join-Path $dataDir '*') -Destination $backupDir -Force

try {
    BuildExe
    AddResult 'build_fulltest_exe' $true 'Compile+link ok'

    try {
        $r = RunCase 'main_invalid_then_exit' "abc`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        AddResult 'main_invalid_then_exit' $true "exit=0 out=$($r.OutPath)"
    } catch {
        AddResult 'main_invalid_then_exit' $false $_.Exception.Message
    }

    try {
        $before = DataCount (Join-Path $dataDir 'outpatients.txt')
        $r = RunCase 'patient_register_new' "1`n2`nFT_REG_A`n25`nM`n0`n`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        $after = DataCount (Join-Path $dataDir 'outpatients.txt')
        if ($after -ne ($before + 1)) { throw "outpatients count before=$before after=$after" }
        $firstData = Get-Content -LiteralPath (Join-Path $dataDir 'outpatients.txt') | Select-Object -Skip 1 | Select-Object -First 1
        $createdPatientId = ($firstData -split "`t")[0]
        if (-not $createdPatientId) { throw 'failed to detect new patient id' }
        AddResult 'patient_register_new' $true "newPatientId=$createdPatientId"
    } catch {
        AddResult 'patient_register_new' $false $_.Exception.Message
    }

    try {
        if (-not $createdPatientId) { throw 'skip: no created patient id' }
        $beforeBal = GetPatientBalance $createdPatientId
        $r = RunCase 'patient_topup_100' "1`n1`n$createdPatientId`n5`n100`n`n0`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        $afterBal = GetPatientBalance $createdPatientId
        if ($afterBal -lt ($beforeBal + 99.99)) { throw "balance before=$beforeBal after=$afterBal" }
        AddResult 'patient_topup_100' $true "balance $beforeBal -> $afterBal"
    } catch {
        AddResult 'patient_topup_100' $false $_.Exception.Message
    }

    try {
        $before = DataCount (Join-Path $dataDir 'nurses.txt')
        $r = RunCase 'admin_nurse_add_delete' "3`nadmin123`n3`n$testNurseId`nFullNurse`nDEPT01`n2`n`n4`n$testNurseId`n`n0`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        $after = DataCount (Join-Path $dataDir 'nurses.txt')
        if ($after -ne $before) { throw "nurses count before=$before after=$after" }
        $row = GetRowById -filePath (Join-Path $dataDir 'nurses.txt') -id $testNurseId
        if ($row) { throw "nurse still exists: $testNurseId" }
        AddResult 'admin_nurse_add_delete' $true "count stable=$after"
    } catch {
        AddResult 'admin_nurse_add_delete' $false $_.Exception.Message
    }

    try {
        $before = DataCount (Join-Path $dataDir 'doctors.txt')
        $r = RunCase 'admin_doctor_add_delete' "3`nadmin123`n1`n$testDoctorId`nFullDoctor`nDEPT01`n2`n`n2`n$testDoctorId`n`n0`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        $after = DataCount (Join-Path $dataDir 'doctors.txt')
        if ($after -ne $before) { throw "doctors count before=$before after=$after" }
        $row = GetRowById -filePath (Join-Path $dataDir 'doctors.txt') -id $testDoctorId
        if ($row) { throw "doctor still exists: $testDoctorId" }
        AddResult 'admin_doctor_add_delete' $true "count stable=$after"
    } catch {
        AddResult 'admin_doctor_add_delete' $false $_.Exception.Message
    }

    try {
        $r = RunCase 'admin_add_schedule_today' "3`nadmin123`n5`n3`n$today`n1`nDEPT01`nD001`n`n0`n0`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        $sched = Get-Content -LiteralPath (Join-Path $dataDir 'schedules.txt') | Where-Object { $_ -like "$today`t门诊`tDEPT01`tD001" } | Select-Object -First 1
        if (-not $sched) { throw "schedule not found for $today 门诊 DEPT01 D001" }
        AddResult 'admin_add_schedule_today' $true 'schedule exists'
    } catch {
        AddResult 'admin_add_schedule_today' $false $_.Exception.Message
    }

    try {
        if (-not $createdPatientId) { throw 'skip: no created patient id' }
        $r = RunCase 'patient_register_d001' "1`n1`n$createdPatientId`n2`n1`nD001`n`n0`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        $rec = Get-Content -LiteralPath (Join-Path $dataDir 'records.txt') | Select-Object -Skip 1 |
            Where-Object {
                $p = $_ -split "`t"
                ($p.Count -ge 8) -and $p[1] -eq $createdPatientId -and $p[2] -eq 'D001' -and $p[3] -eq '1' -and $p[4] -eq $today
            } | Select-Object -First 1
        if (-not $rec) { throw "registration record not found for patient=$createdPatientId doctor=D001 date=$today" }
        AddResult 'patient_register_d001' $true 'registration record created'
    } catch {
        AddResult 'patient_register_d001' $false $_.Exception.Message
    }

    try {
        if (-not $createdPatientId) { throw 'skip: no created patient id' }
        $inBefore = GetRowById -filePath (Join-Path $dataDir 'inpatients.txt') -id $createdPatientId
        if ($inBefore) { throw "patient already inpatient before test: $createdPatientId" }

        $r = RunCase 'doctor_call_and_admission_cancel' "2`nD001`n2`n`n8`n-1`n`n0`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }

        $inAfter = GetRowById -filePath (Join-Path $dataDir 'inpatients.txt') -id $createdPatientId
        if ($inAfter) { throw "patient unexpectedly became inpatient: $createdPatientId" }
        AddResult 'doctor_call_and_admission_cancel' $true 'patient remains outpatient'
    } catch {
        AddResult 'doctor_call_and_admission_cancel' $false $_.Exception.Message
    }

    try {
        $r = RunCase 'ambulance_basic' "4`n1`n`n2`nBADCAR`n`n4`n`n5`n`n0`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        AddResult 'ambulance_basic' $true "exit=0 out=$($r.OutPath)"
    } catch {
        AddResult 'ambulance_basic' $false $_.Exception.Message
    }

    try {
        if (-not $createdPatientId) { throw 'skip: no created patient id' }
        $r = RunCase 'nurse_login_round_vitals_invalid_patient' "5`nN001`n`n1`n`n2`n$createdPatientId`n`n0`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        AddResult 'nurse_login_round_vitals_invalid_patient' $true "exit=0 out=$($r.OutPath)"
    } catch {
        AddResult 'nurse_login_round_vitals_invalid_patient' $false $_.Exception.Message
    }

    try {
        $r = RunCase 'admin_query_time_range' "3`nadmin123`n17`n2026-04-01`n2026-04-30`n`n0`n0`n"
        if ($r.ExitCode -ne 0) { throw "exit=$($r.ExitCode)" }
        AddResult 'admin_query_time_range' $true "exit=0 out=$($r.OutPath)"
    } catch {
        AddResult 'admin_query_time_range' $false $_.Exception.Message
    }

    try {
        $code = @"
#include <stdio.h>
#include "utils.h"
int main(void){ double v=safe_get_double_allow_negative(); printf("%.2f\n", v); return 0; }
"@
        $probeC = Join-Path $tmpRoot 'probe_allow_negative.c'
        $probeExe = Join-Path $tmpRoot 'probe_allow_negative.exe'
        $probeIn = Join-Path $tmpRoot 'probe_allow_negative.in'
        $probeOutPath = Join-Path $tmpRoot 'probe_allow_negative.out.txt'
        Set-Content -LiteralPath $probeC -Value $code -Encoding UTF8
        Set-Content -LiteralPath $probeIn -Value "-1`n" -Encoding ASCII
        & gcc -Iinclude $probeC src\utils.c -o $probeExe
        if ($LASTEXITCODE -ne 0) { throw 'probe compile failed' }
        $cmdProbe = '"' + $probeExe + '" < "' + $probeIn + '" > "' + $probeOutPath + '"'
        cmd /c $cmdProbe | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "probe exit=$LASTEXITCODE" }
        $probeOut = (Get-Content -LiteralPath $probeOutPath -Raw).Trim()
        if ($probeOut -ne '-1.00') { throw "probe output=$probeOut" }
        AddResult 'allow_negative_probe' $true 'safe_get_double_allow_negative returns -1.00'
    } catch {
        AddResult 'allow_negative_probe' $false $_.Exception.Message
    }

} catch {
    AddResult 'harness_runtime' $false $_.Exception.Message
} finally {
    try {
        Copy-Item -LiteralPath (Join-Path $backupDir '*') -Destination $dataDir -Force
        AddResult 'restore_data' $true 'data restored from backup'
    } catch {
        AddResult 'restore_data' $false $_.Exception.Message
    }

    SaveReport $results

    $results | ForEach-Object {
        Write-Output ("[{0}] {1} | {2}" -f $_.Status, $_.Name, $_.Detail)
    }

    $failCount = ($results | Where-Object { $_.Status -eq 'FAIL' }).Count
    if ($failCount -gt 0) { exit 1 }
    exit 0
}
