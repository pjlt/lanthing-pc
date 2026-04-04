Param(
    [string]$RepoRoot = ".",
    [switch]$FailOnLeak
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = Resolve-Path $RepoRoot
$sourceRoot = Join-Path $repo "src"

if (-not (Test-Path $sourceRoot)) {
    throw "Cannot find src directory under: $repo"
}

$allowedPatterns = @(
    "src/plat/",
    "src/inputs/",
    "src/inputs/executor/win_",
    "src/transport/rtc/",
    "src/transport/rtc2/"
)

$macroPattern = '^\s*#\s*(if|ifdef|ifndef).*?(LT_WINDOWS|_WIN32|WIN32)'
$files = Get-ChildItem -Path $sourceRoot -Recurse -File -Include *.h,*.hpp,*.cpp,*.c
$violations = New-Object System.Collections.Generic.List[object]

foreach ($file in $files) {
    $relative = ($file.FullName.Substring($repo.Path.Length + 1)) -replace '\\', '/'

    $isAllowed = $false
    foreach ($allowed in $allowedPatterns) {
        if ($relative.StartsWith($allowed)) {
            $isAllowed = $true
            break
        }
    }

    if ($isAllowed) {
        continue
    }

    $lines = @(Get-Content -Path $file.FullName)
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match $macroPattern) {
            $violations.Add([PSCustomObject]@{
                    Path    = $relative
                    Line    = $i + 1
                    Content = $lines[$i].Trim()
                })
        }
    }
}

if ($violations.Count -eq 0) {
    Write-Output "No platform macro leakage found outside adapter boundaries."
    exit 0
}

Write-Output "Detected platform macro leakage outside adapter boundaries:"
$violations | Sort-Object Path, Line | ForEach-Object {
    Write-Output ("{0}:{1}  {2}" -f $_.Path, $_.Line, $_.Content)
}

Write-Output "\nAllowed boundary prefixes:"
$allowedPatterns | ForEach-Object { Write-Output "- $_" }

if ($FailOnLeak) {
    throw "Platform macro leakage check failed with $($violations.Count) violation(s)."
}

Write-Output "\nLeakage check completed in report mode. Use -FailOnLeak to enforce non-zero exit."
exit 0
