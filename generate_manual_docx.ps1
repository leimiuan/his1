param(
    [string]$SourcePath = "$PSScriptRoot\\HIS系统操作手册.md",
    [string]$OutputPath = "$PSScriptRoot\\HIS系统操作手册.docx"
)

function Escape-Xml([string]$Text) {
    return [System.Security.SecurityElement]::Escape($Text)
}

function Write-Utf8File([string]$Path, [string]$Content) {
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $utf8)
}

if (-not (Test-Path $SourcePath)) {
    throw "Source file not found: $SourcePath"
}

$tmpDir = Join-Path $env:TEMP ("his_docx_" + [guid]::NewGuid().ToString())

try {
    New-Item -ItemType Directory -Path $tmpDir -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $tmpDir "_rels") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $tmpDir "word") -Force | Out-Null

    $raw = Get-Content -Path $SourcePath -Raw -Encoding UTF8
    $lines = $raw -split "`r?`n"

    $paragraphs = New-Object System.Collections.Generic.List[string]
    foreach ($line in $lines) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            $paragraphs.Add("<w:p/>")
            continue
        }

        $clean = $line -replace "^#{1,6}\\s+", ""
        $clean = Escape-Xml $clean
        $paragraphs.Add("<w:p><w:r><w:t xml:space=`"preserve`">$clean</w:t></w:r></w:p>")
    }

    $contentTypes = @"
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>
"@

    $rels = @"
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>
"@

    $document = @"
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body>
$($paragraphs -join "`n")
    <w:sectPr>
      <w:pgSz w:w="11906" w:h="16838"/>
      <w:pgMar w:top="1440" w:right="1440" w:bottom="1440" w:left="1440" w:header="720" w:footer="720" w:gutter="0"/>
    </w:sectPr>
  </w:body>
</w:document>
"@

    Write-Utf8File -Path (Join-Path $tmpDir "[Content_Types].xml") -Content $contentTypes
    Write-Utf8File -Path (Join-Path $tmpDir "_rels\\.rels") -Content $rels
    Write-Utf8File -Path (Join-Path $tmpDir "word\\document.xml") -Content $document

    $zipPath = [System.IO.Path]::ChangeExtension($OutputPath, ".zip")
    if (Test-Path $zipPath) {
        Remove-Item -Path $zipPath -Force
    }

    Compress-Archive -Path (Join-Path $tmpDir "*") -DestinationPath $zipPath -Force

    if (Test-Path $OutputPath) {
        Remove-Item -Path $OutputPath -Force
    }

    Move-Item -Path $zipPath -Destination $OutputPath -Force
    Write-Output "DOCX_CREATED: $OutputPath"
}
finally {
    if (Test-Path $tmpDir) {
        Remove-Item -Path $tmpDir -Recurse -Force
    }
}
