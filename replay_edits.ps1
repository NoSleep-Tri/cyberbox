$transcriptPath = "C:\Users\Tri\.gemini\antigravity\brain\615beb12-d545-47d8-b39d-53c9d81a79e0\.system_generated\logs\transcript_full.jsonl"
$lines = [System.IO.File]::ReadAllLines($transcriptPath)

# 1. Start with base from line 4729
$baseObj = $lines[4729] | ConvertFrom-Json
$content = $baseObj.tool_calls[0].args.CodeContent
Write-Host "Base length: $($content.Length)"

$successCount = 0
$failCount = 0

for ($i = 4730; $i -lt 7200; $i++) {
    $l = $lines[$i]
    if ($l.Contains("replace_file_content") -and $l.Contains("native_gui.cpp")) {
        try {
            $obj = $l | ConvertFrom-Json
            if ($obj.tool_calls) {
                foreach ($c in $obj.tool_calls) {
                    if ($c.name -eq "replace_file_content" -and $c.args.TargetFile -like "*native_gui.cpp") {
                        $target = $c.args.TargetContent
                        $repl = $c.args.ReplacementContent
                        if ($content.Contains($target)) {
                            $content = $content.Replace($target, $repl)
                            $successCount++
                        } else {
                            # Try normalizing line endings
                            $normTarget = $target.Replace("`r`n", "`n")
                            $normContent = $content.Replace("`r`n", "`n")
                            if ($normContent.Contains($normTarget)) {
                                $normRepl = $repl.Replace("`r`n", "`n")
                                $normContent = $normContent.Replace($normTarget, $normRepl)
                                $content = $normContent.Replace("`n", "`r`n")
                                $successCount++
                            } else {
                                Write-Host "Could not match edit at line $i : $($c.args.Description)"
                                $failCount++
                            }
                        }
                    }
                }
            }
        } catch {}
    }
}

Write-Host "Replay finished. Success: $successCount, Failed: $failCount"
[System.IO.File]::WriteAllText("src\native_gui.cpp", $content, [System.Text.Encoding]::UTF8)
Write-Host "Saved src\native_gui.cpp, final length: $($content.Length)"
