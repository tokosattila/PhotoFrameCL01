param(
  [switch]$Strict
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$jsRoot = Join-Path $repoRoot 'src\App\Dashboard\Assets\Js'

if(-not (Test-Path $jsRoot)) {
  Write-Error "JS root not found: $jsRoot"
}

$files = Get-ChildItem -Path $jsRoot -Filter '*.h' -Recurse -File
$violations = New-Object System.Collections.Generic.List[string]

$translateFallbackPattern = [regex]'TranslateMessage\s*\([^,\)]*,'
$shortVarPattern = [regex]'\bvar\s+([A-Za-z_$])\b'
$shortLetConstPattern = [regex]'\b(?:let|const)\s+([A-Za-z_$])\b'

foreach($file in $files) {
  $lines = Get-Content -Path $file.FullName
  for($lineIndex = 0; $lineIndex -lt $lines.Count; $lineIndex += 1) {
    $line = $lines[$lineIndex]

    # Exception: third-party minified Toastify bundle in App.h
    if($file.Name -eq 'App.h' -and $line -match '!function\(t,o\)') {
      continue
    }

    if($translateFallbackPattern.IsMatch($line)) {
      $violations.Add("Translate fallback is not allowed: $($file.FullName):$($lineIndex + 1)") | Out-Null
    }

    $shortVarMatches = $shortVarPattern.Matches($line)
    foreach($match in $shortVarMatches) {
      $varName = $match.Groups[1].Value
      $violations.Add("Non-descriptive var name '$varName': $($file.FullName):$($lineIndex + 1)") | Out-Null
    }

    if($Strict) {
      $shortLetConstMatches = $shortLetConstPattern.Matches($line)
      foreach($match in $shortLetConstMatches) {
        $varName = $match.Groups[1].Value
        $violations.Add("Non-descriptive let/const name '$varName': $($file.FullName):$($lineIndex + 1)") | Out-Null
      }
    }
  }
}

if($violations.Count -gt 0) {
  Write-Host ''
  Write-Host 'Dashboard JS style guard failed.' -ForegroundColor Red
  Write-Host 'Violations:' -ForegroundColor Red
  $violations | Sort-Object | ForEach-Object { Write-Host " - $_" -ForegroundColor Yellow }
  exit 1
}

if($Strict) {
  Write-Host 'Dashboard JS style guard passed (strict mode).' -ForegroundColor Green
} else {
  Write-Host 'Dashboard JS style guard passed.' -ForegroundColor Green
}
exit 0
