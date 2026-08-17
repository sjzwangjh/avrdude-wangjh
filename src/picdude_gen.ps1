param(
    [string]$AvrSrc = "E:\wangjunhua\Project\AvrProgrammer\avrdude\src",
    [string]$ConstFile = "E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\picDeviceConst.c",
    [string]$ConstHeader = "E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\picDeviceConst.h",
    [switch]$SyncFirmware,
    [switch]$SyncXml,
    [switch]$SyncInc,
    [string]$OutConf = "picdude.conf",
    [string]$OutReport = "picdude_check_report.md"
)

$ErrorActionPreference = "Stop"

# Names removed per user decision (item 3/4/5): PS200 + non-standard names
$Blacklist = @(
    "PS200",
    "PICRF675F", "PICRF675H", "PICRF675K",
    "PICRF509AF", "PICRF509AG",
    "PIC16F1829LIN",
    "PIC12F529T39A", "PIC12F529T48A",
    "PIC12LF1840T39A", "PIC12LF1840T48A",
    "PIC16LF1824T39A"
)

$xmlPath = Join-Path $AvrSrc "pic10-12-16-init.xml"
$incPath = Join-Path $AvrSrc "pic_devicenames.inc"
$outConf = Join-Path $AvrSrc $OutConf
$outRpt  = Join-Path $AvrSrc $OutReport

function Read-TextPreserving([string]$path) {
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $bom = $false
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { $bom = $true }
    $enc = if ($bom) { [System.Text.Encoding]::UTF8 } else { [System.Text.Encoding]::GetEncoding(936) }
    return [pscustomobject]@{ Text = $enc.GetString($bytes); Enc = $enc; Bom = $bom }
}

function Write-TextPreserving([string]$path, [string]$text, $enc, [bool]$bom) {
    $bytes = $enc.GetBytes($text)
    if ($bom) {
        $withBom = New-Object byte[] ($bytes.Length + 3)
        $withBom[0] = 0xEF; $withBom[1] = 0xBB; $withBom[2] = 0xBF
        [Array]::Copy($bytes, 0, $withBom, 3, $bytes.Length)
        $bytes = $withBom
    }
    [System.IO.File]::WriteAllBytes($path, $bytes)
}

# ---------------- 1. g_deviceTable (all rows)
$tblRows = New-Object System.Collections.Generic.List[object]
$inTable = $false
foreach ($line in Get-Content $ConstFile) {
    if ($line -match 'g_deviceTable\s*\[\]') { $inTable = $true; continue }
    if ($inTable) {
        if ($line -match '^\s*\{\s*"([^"]+)"\s*,\s*([^\}]+)\},?\s*$') {
            $tblRows.Add([pscustomobject]@{ index = $tblRows.Count; name = $Matches[1]; row = $Matches[2].Trim() })
        } elseif ($line -match '^\s*\};') { break }
    }
}

# Prefer the original (pre-sync) table from the .full.c backup if present
$srcRows = $tblRows
$bakPath = $ConstFile -replace '\.c$', '.full.c'
if ((Test-Path $bakPath) -and $tblRows.Count -lt 400) {
    $srcRows = New-Object System.Collections.Generic.List[object]
    $inTable = $false
    foreach ($line in Get-Content $bakPath) {
        if ($line -match 'g_deviceTable\s*\[\]') { $inTable = $true; continue }
        if ($inTable) {
            if ($line -match '^\s*\{\s*"([^"]+)"\s*,\s*([^\}]+)\},?\s*$') {
                $srcRows.Add([pscustomobject]@{ index = $srcRows.Count; name = $Matches[1]; row = $Matches[2].Trim() })
            } elseif ($line -match '^\s*\};') { break }
        }
    }
}

# ---------------- 2. authoritative list: first occurrence, blacklist removed
$unique = @()
$firstIdx = [ordered]@{}
$origIdx = [ordered]@{}
foreach ($r in $srcRows) {
    if ($r.name -in $Blacklist) { continue }
    if (-not $firstIdx.Contains($r.name)) {
        $origIdx[$r.name] = $r.index
        $firstIdx[$r.name] = $unique.Count
        $unique += $r.name
    }
}
$removed = @($Blacklist | Where-Object { $_ -in @($srcRows | ForEach-Object { $_.name }) })

# ---------------- 3. XML fields
$xml = [ordered]@{}
$curName = $null; $curFields = $null
foreach ($line in Get-Content $xmlPath) {
    if ($line -match '<DeviceName>([^<]+)</DeviceName>') {
        $curName = $Matches[1]
        $curFields = [System.Collections.Specialized.OrderedDictionary]::new()
    } elseif ($line -match '</Device>' -and $curName) {
        $xml[$curName] = [pscustomobject]$curFields
        $curName = $null; $curFields = $null
    } elseif ($curName -and $line -match '<([A-Za-z0-9_]+)>([^<]*)</\1>') {
        $curFields[$Matches[1]] = $Matches[2]
    }
}

# ---------------- 4. inc (old, for report only)
$incOld = @(Get-Content $incPath | Select-String -Pattern '"(PIC[^"]+)"' | ForEach-Object { $_.Matches[0].Groups[1].Value })

# ---------------- 5. duplicates in original table (report only)
$dupInfo = @($srcRows | Group-Object name | Where-Object { $_.Count -gt 1 } | ForEach-Object {
    $idx = @($_.Group | ForEach-Object { $_.index })
    $rows = @($_.Group | ForEach-Object { $_.row })
    $same = ($rows | Select-Object -Unique).Count -eq 1
    [pscustomobject]@{ name = $_.Name; count = $_.Count; indices = ($idx -join ','); identical = $same }
})

$capacitySuspects = @($unique | Where-Object { $xml.Contains($_) -and $xml[$_].code_end_addr -eq '0x800' })

# ---------------- 6. rewrite picDeviceConst.c (dedupe + blacklist + comment count)
if ($SyncFirmware) {
    $bak = $ConstFile -replace '\.c$', '.full.c'
    if (-not (Test-Path $bak)) { Copy-Item -LiteralPath $ConstFile -Destination $bak }
    $f = Read-TextPreserving $ConstFile
    $t = $f.Text
    $startMark = "/* Device table ("
    $arrStart = $t.IndexOf("static const pic8_device_index_t g_deviceTable[] = {")
    $s0 = $t.LastIndexOf("`n", $arrStart) + 1
    $arrEnd = $t.IndexOf("};", $arrStart) + 2
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("/* Device table ($($unique.Count) entries) */")
    [void]$sb.AppendLine("static const pic8_device_index_t g_deviceTable[] = {")
    foreach ($r in $srcRows) {
        if ($r.name -notin $Blacklist -and $origIdx.Contains($r.name) -and $origIdx[$r.name] -eq $r.index) {
            [void]$sb.AppendLine("  { `"$($r.name)`", $($r.row) },")
        }
    }
    [void]$sb.AppendLine("};")
    $newArr = $sb.ToString()
    $t = $t.Substring(0, $s0) + $newArr + $t.Substring($arrEnd)
    $t = $t.Replace("457", "274")
    Write-TextPreserving $ConstFile $t $f.Enc $f.Bom

    # rewrite header: PIC8_DEVICE_TABLE_SIZE and comments
    $bakH = $ConstHeader -replace '\.h$', '.full.h'
    if (-not (Test-Path $bakH)) { Copy-Item -LiteralPath $ConstHeader -Destination $bakH }
    $h = Read-TextPreserving $ConstHeader
    $ht = $h.Text.Replace("457", "274")
    Write-TextPreserving $ConstHeader $ht $h.Enc $h.Bom
    "firmware: picDeviceConst.c/h synced ($($unique.Count) entries)"
}

# ---------------- 7. rewrite XML (dedupe first occurrence, blacklist removed)
if ($SyncXml) {
    $fx = Read-TextPreserving $xmlPath
    $nl = if ($fx.Text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $lines = $fx.Text -split '\r?\n'
    $head = @()
    $blocks = New-Object System.Collections.Generic.List[object]
    $cur = $null
    foreach ($line in $lines) {
        if ($line -match '<Device>') {
            $cur = New-Object System.Collections.Generic.List[string]
            $cur.Add($line)
        } elseif ($line -match '</Device>' -and $cur) {
            $cur.Add($line)
            $nm = ($cur | Select-String -Pattern '<DeviceName>([^<]+)</DeviceName>' | Select-Object -First 1).Matches[0].Groups[1].Value
            $blocks.Add([pscustomobject]@{ name = $nm; lines = @($cur) })
            $cur = $null
        } elseif ($cur) {
            $cur.Add($line)
        } elseif ($line -notmatch '^</PIC8_Devices>') {
            $head += $line
        }
    }
    $seen = @{}
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($h in $head) {
        $repl = $h -replace 'total_devices="\d+"', ("total_devices=" + '"' + $unique.Count + '"')
        $out.Add($repl)
    }
    foreach ($b in $blocks) {
        if ($b.name -in $Blacklist) { continue }
        if ($seen.ContainsKey($b.name)) { continue }
        $seen[$b.name] = $true
        foreach ($l in $b.lines) { $out.Add($l) }
    }
    $out.Add("</PIC8_Devices>")
    Write-TextPreserving $xmlPath (($out -join $nl) + $nl) $fx.Enc $fx.Bom
    "xml: pic10-12-16-init.xml deduped ($($unique.Count) devices)"
}

# ---------------- 8. rewrite inc
if ($SyncInc) {
    $si = New-Object System.Text.StringBuilder
    [void]$si.AppendLine("/* Auto-generated from pic10-12-16-init.xml - $($unique.Count) devices */")
    [void]$si.AppendLine("#define PIC_DEVICE_COUNT $($unique.Count)")
    [void]$si.AppendLine("")
    [void]$si.AppendLine("static const char *const pic_device_names[PIC_DEVICE_COUNT] = {")
    foreach ($n in $unique) { [void]$si.AppendLine("    `"$n`",") }
    [void]$si.AppendLine("};")
    Set-Content -Path $incPath -Value $si.ToString() -Encoding ASCII
    "inc: pic_devicenames.inc regenerated ($($unique.Count) names)"
}

# ---------------- 9. picdude.conf
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("# picdude.conf - PIC10/12/16 device definitions for AVRDUDE")
[void]$sb.AppendLine("# Generated by picdude_gen.ps1 (Phase 0), ASCII only")
[void]$sb.AppendLine("#")
[void]$sb.AppendLine("# Order authority : g_deviceTable[] in picDeviceConst.c (first occurrence)")
[void]$sb.AppendLine("#   devices: $($unique.Count) (duplicates removed: $($dupInfo.Count) groups / $($srcRows.Count - $unique.Count) rows;")
[void]$sb.AppendLine("#   removed by decision: $($removed.Count) -> $($removed -join ', '))")
[void]$sb.AppendLine("# Parameter source: pic10-12-16-init.xml (deduped, same order)")
[void]$sb.AppendLine("#")
[void]$sb.AppendLine("# NOTE 1 [CAPACITY_CHECK]: $($capacitySuspects.Count) devices have code_end_addr == 0x800;")
[void]$sb.AppendLine("#       XML value is truncated for many larger parts - verify size against datasheet.")
[void]$sb.AppendLine("# NOTE 2: config*/userid memories intentionally not emitted (Phase 3 C support).")
[void]$sb.AppendLine("")

$count = 0
$missingXml = @()
foreach ($n in $unique) {
    $count++
    if (-not $xml.Contains($n)) {
        $missingXml += $n
        [void]$sb.AppendLine("# WARNING: '$n' not found in pic10-12-16-init.xml - parameters unavailable")
        [void]$sb.AppendLine("# part")
        [void]$sb.AppendLine("#     id         = `"$n`";")
        [void]$sb.AppendLine("#     desc       = `"$n`";")
        [void]$sb.AppendLine("#     prog_modes = PM_ISP;")
        [void]$sb.AppendLine("# ;")
        [void]$sb.AppendLine("")
        continue
    }
    $x = $xml[$n]
    $ce  = [Convert]::ToInt32($x.code_end_addr, 16)
    $row = [Convert]::ToInt32($x.row_pgm_words)
    $flashSize = $ce * 2
    $pageSize  = $row * 2
    $flag = if ($x.code_end_addr -eq '0x800') { "  # [CAPACITY_CHECK] code_end=$($x.code_end_addr) words may be truncated" } else { "" }

    [void]$sb.AppendLine("part")
    [void]$sb.AppendLine("    # [g_deviceTable idx $($firstIdx[$n])]")
    [void]$sb.AppendLine("    id         = `"$n`";")
    [void]$sb.AppendLine("    desc       = `"$n`";")
    [void]$sb.AppendLine("    prog_modes = PM_ISP;")
    [void]$sb.AppendLine("    mcuid      = $([int](431 + $firstIdx[$n]));   # unique, derived from table index")
    [void]$sb.AppendLine("    memory `"flash`"")
    [void]$sb.AppendLine("        size      = $flashSize;    # code_end $($x.code_end_addr) words x 2")
    [void]$sb.AppendLine("        page_size = $pageSize;     # row_pgm_words $($x.row_pgm_words) x 2")
    [void]$sb.AppendLine("        readsize  = 256;")
    [void]$sb.AppendLine("    ;")
    if ($x.eedata_end_addr -ne '0x0' -and $x.eedata_base -ne '0x0') {
        $eeSize = [Convert]::ToInt32($x.eedata_end_addr, 16) - [Convert]::ToInt32($x.eedata_base, 16)
        [void]$sb.AppendLine("    memory `"eeprom`"")
        [void]$sb.AppendLine("        size      = $eeSize;     # eedata $($x.eedata_base)..$($x.eedata_end_addr)")
        [void]$sb.AppendLine("        readsize  = 256;")
        [void]$sb.AppendLine("    ;")
    }
    if ($flag) { [void]$sb.AppendLine($flag) }
    [void]$sb.AppendLine(";")
    [void]$sb.AppendLine("")
}

Set-Content -Path $outConf -Value $sb.ToString() -Encoding ASCII

# ---------------- 10. report
$r = New-Object System.Text.StringBuilder
[void]$r.AppendLine("# picdude Phase 0 check report (after sync)")
[void]$r.AppendLine("")
[void]$r.AppendLine("## 1. Final counts")
[void]$r.AppendLine("| file | entries |")
[void]$r.AppendLine("|---|---|")
[void]$r.AppendLine("| g_deviceTable (picDeviceConst.c) | $($unique.Count) |")
[void]$r.AppendLine("| pic10-12-16-init.xml | $($unique.Count) |")
[void]$r.AppendLine("| pic_devicenames.inc | $($unique.Count) |")
[void]$r.AppendLine("| picdude.conf | $count |")
[void]$r.AppendLine("")
[void]$r.AppendLine("## 2. Original source state (before sync)")
[void]$r.AppendLine("- g_deviceTable rows: $($srcRows.Count); distinct: $(@($srcRows | ForEach-Object { $_.name } | Select-Object -Unique).Count)")
[void]$r.AppendLine("- Duplicate groups: $($dupInfo.Count); extra rows: $($srcRows.Count - $unique.Count)")
[void]$r.AppendLine("- Removed by decision (PS200 + non-standard names): $($removed.Count) -> $($removed -join ', ')")
[void]$r.AppendLine("")
[void]$r.AppendLine("## 3. Duplicate groups in original g_deviceTable (first occurrence kept)")
[void]$r.AppendLine("| name | count | original row indices | identical rows |")
[void]$r.AppendLine("|---|---|---|---|")
foreach ($d in ($dupInfo | Sort-Object name)) {
    [void]$r.AppendLine("| $($d.name) | $($d.count) | $($d.indices) | $(if($d.identical){'yes'}else{'NO'}) |")
}
[void]$r.AppendLine("")
[void]$r.AppendLine("## 4. Capacity suspects kept (code_end_addr == 0x800; not corrected per user decision)")
[void]$r.AppendLine("Count: $($capacitySuspects.Count)")
[void]$r.AppendLine("")
[void]$r.AppendLine("## 5. Output")
[void]$r.AppendLine("- $outConf : $count part entries; XML-missing: $($missingXml.Count) ($($missingXml -join ', '))")
[void]$r.AppendLine("- This report: $outRpt")
[void]$r.AppendLine("- Backups: picDeviceConst.full.c / picDeviceConst.full.h next to the originals;")
[void]$r.AppendLine("  pic10-12-16-init.xml and pic_devicenames.inc are recoverable via git.")
Set-Content -Path $outRpt -Value $r.ToString() -Encoding UTF8

# ---------------- summary
"final device count   = $($unique.Count)"
"original rows        = $($srcRows.Count)"
"duplicate groups     = $($dupInfo.Count)"
"removed names        = $($removed.Count)"
"capacity suspects    = $($capacitySuspects.Count)"
"conf entries         = $count"
"conf  : $outConf"
"report: $outRpt"
