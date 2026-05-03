@echo off
setlocal

rem Convert Claude skills into Codex skills.
rem Defaults:
rem   source: .\.claude\skills
rem   target: .\.codex\skills
rem Usage:
rem   convert_claude_skills_to_codex.bat [source_skills_dir] [target_skills_dir]

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference = 'Stop';" ^
  "$repo = (Get-Location).Path;" ^
  "$srcArg = '%~1'; $dstArg = '%~2';" ^
  "if ([string]::IsNullOrWhiteSpace($srcArg)) {" ^
  "  $src = Join-Path $repo '.claude\skills';" ^
  "  if (-not (Test-Path -LiteralPath $src)) { Write-Host 'No .\.claude\skills directory found. Nothing to convert.'; exit 0 }" ^
  "} else { $src = (Resolve-Path -LiteralPath $srcArg).Path }" ^
  "if ([string]::IsNullOrWhiteSpace($dstArg)) {" ^
  "  $dst = Join-Path $repo '.codex\skills'" ^
  "} else { $dst = $dstArg }" ^
  "if ([System.IO.Path]::IsPathRooted($dst)) { $dst = [System.IO.Path]::GetFullPath($dst) } else { $dst = [System.IO.Path]::GetFullPath((Join-Path $repo $dst)) }" ^
  "New-Item -ItemType Directory -Force -Path $dst | Out-Null;" ^
  "$stamp = Get-Date -Format 'yyyyMMdd-HHmmss';" ^
  "$skills = Get-ChildItem -LiteralPath $src -Directory -Force;" ^
  "if ($skills.Count -eq 0) { Write-Host 'No skill folders found.'; exit 0 }" ^
  "$copied = 0; $skipped = 0;" ^
  "foreach ($skill in $skills) {" ^
  "  $skillMd = Join-Path $skill.FullName 'SKILL.md';" ^
  "  if (-not (Test-Path -LiteralPath $skillMd)) { Write-Warning ('Skipping {0}: missing SKILL.md' -f $skill.Name); $skipped++; continue }" ^
  "  $target = Join-Path $dst $skill.Name;" ^
  "  if (Test-Path -LiteralPath $target) {" ^
  "    $backup = $target + '.bak-' + $stamp;" ^
  "    Move-Item -LiteralPath $target -Destination $backup;" ^
  "    Write-Host ('Backed up existing {0} to {1}' -f $skill.Name, $backup);" ^
  "  }" ^
  "  New-Item -ItemType Directory -Force -Path $target | Out-Null;" ^
  "  Get-ChildItem -LiteralPath $skill.FullName -Force | Copy-Item -Destination $target -Recurse -Force;" ^
  "  Get-ChildItem -LiteralPath $target -Directory -Recurse -Force -Filter '__pycache__' | Remove-Item -Recurse -Force;" ^
  "  Get-ChildItem -LiteralPath $target -File -Recurse -Force | Where-Object { $_.Extension -in '.pyc', '.pyo' } | Remove-Item -Force;" ^
  "  $targetSkillMd = Join-Path $target 'SKILL.md';" ^
  "  $text = Get-Content -LiteralPath $targetSkillMd -Raw;" ^
  "  if ($text -match '(?s)^---\r?\n(.*?)\r?\n---\r?\n(.*)$') {" ^
  "    $front = $matches[1]; $body = $matches[2];" ^
  "    $name = [regex]::Match($front, '(?m)^name:\s*(.+?)\s*$').Groups[1].Value;" ^
  "    $description = [regex]::Match($front, '(?m)^description:\s*(.+?)\s*$').Groups[1].Value;" ^
  "    if (-not [string]::IsNullOrWhiteSpace($name) -and -not [string]::IsNullOrWhiteSpace($description)) {" ^
  "      $newText = ('---' + \"`n\" + 'name: ' + $name.Trim() + \"`n\" + 'description: ' + $description.Trim() + \"`n\" + '---' + \"`n\" + $body);" ^
  "      [System.IO.File]::WriteAllText($targetSkillMd, $newText, [System.Text.UTF8Encoding]::new($false));" ^
  "    } else { Write-Warning ('{0}: SKILL.md frontmatter did not contain both name and description; copied unchanged.' -f $skill.Name) }" ^
  "  } else { Write-Warning ('{0}: SKILL.md has no YAML frontmatter; copied unchanged.' -f $skill.Name) }" ^
  "  $copied++;" ^
  "  Write-Host ('Copied {0}' -f $skill.Name);" ^
  "}" ^
  "Write-Host ('Done. Copied {0} skill(s), skipped {1}. Target: {2}' -f $copied, $skipped, $dst);" ^
  "Write-Host 'Restart Codex to pick up new skills.'"

if errorlevel 1 exit /b %errorlevel%
endlocal
