param([string]$ExistingAssets = "")

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

try {
    $assetRoot = Join-Path $PSScriptRoot "assets"
    $index = Get-Content -LiteralPath (Join-Path $assetRoot "index.json") -Raw | ConvertFrom-Json
    if ($index.release -ne "2.6" -or $index.content_format -ne 1) {
        throw "This Atlas package requires the included OpenYAMM 2.6 content index."
    }
    $store = Join-Path $assetRoot "packs"
    New-Item -ItemType Directory -Path $store -Force | Out-Null

    # English uses the base packs. Preload Polish too; honour other selected languages on later launches.
    $language = "en"
    $settings = Join-Path $PSScriptRoot "settings.ini"
    if (Test-Path -LiteralPath $settings) {
        $inGameplay = $false
        foreach ($line in Get-Content -LiteralPath $settings) {
            if ($line -match '^\s*\[([^\]]+)\]') { $inGameplay = $Matches[1] -eq "gameplay" }
            elseif ($inGameplay -and $line -match '^\s*language\s*=\s*([a-zA-Z0-9_-]+)') {
                $language = $Matches[1].ToLowerInvariant()
            }
        }
    }
    $packs = @($index.packs) + @($index.language_packs | Where-Object {
        $_.language -eq "pl" -or $_.language -eq $language
    })
    $sourceRoot = if ($ExistingAssets) { (Resolve-Path -LiteralPath $ExistingAssets).Path } else { "" }
    $reused = 0
    $downloaded = 0
    $imported = 0

    foreach ($pack in $packs) {
        if ($pack.file -notmatch '^[a-zA-Z0-9._-]+\.zip$' -or $pack.sha256 -notmatch '^[a-f0-9]{64}$') {
            throw "Invalid content record in the pinned index."
        }
        $target = Join-Path $store $pack.file
        if ((Test-Path -LiteralPath $target) -and (Get-Item -LiteralPath $target).Length -eq $pack.size) {
            $reused++
            continue
        }

        if ($sourceRoot) {
            $candidates = @(
                (Join-Path $sourceRoot $pack.file),
                (Join-Path (Join-Path $sourceRoot "packs") $pack.file),
                (Join-Path (Join-Path $sourceRoot "assets/packs") $pack.file)
            )
            foreach ($candidate in $candidates) {
                if (!(Test-Path -LiteralPath $candidate)) { continue }
                if ((Get-Item -LiteralPath $candidate).Length -ne $pack.size) { continue }
                if ((Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash -ine $pack.sha256) { continue }
                $temporary = $target + ".partial"
                if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
                try { New-Item -ItemType HardLink -Path $temporary -Target $candidate -ErrorAction Stop | Out-Null }
                catch { Copy-Item -LiteralPath $candidate -Destination $temporary }
                Move-Item -LiteralPath $temporary -Destination $target -Force
                $imported++
                break
            }
        }
        if ((Test-Path -LiteralPath $target) -and (Get-Item -LiteralPath $target).Length -eq $pack.size) { continue }

        Write-Host ("Downloading {0} ({1:N1} MB)" -f $pack.id, ($pack.size / 1MB))
        $temporary = $target + ".partial"
        # These immutable pack URLs come from the verified 2.6 manifest, never the latest release.
        $url = "https://jasicek.net/openyamm/updates/packs/" + $pack.sha256 + ".zip"
        Invoke-WebRequest -Uri $url -OutFile $temporary -UseBasicParsing
        if ((Get-Item -LiteralPath $temporary).Length -ne $pack.size -or
            (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ine $pack.sha256) {
            Remove-Item -LiteralPath $temporary -ErrorAction SilentlyContinue
            throw ("Content verification failed: " + $pack.id)
        }
        Move-Item -LiteralPath $temporary -Destination $target -Force
        $downloaded++
    }
    Write-Host "OpenYAMM 2.6 content ready: $reused cached, $imported imported, $downloaded downloaded."
    exit 0
}
catch {
    Write-Host ("Content setup failed: " + $_.Exception.Message) -ForegroundColor Red
    Write-Host "Run Play Atlas.cmd again to retry. Completed packs are kept."
    exit 1
}
