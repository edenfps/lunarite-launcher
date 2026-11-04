# Builds the player launcher bundle: WZLauncher.exe, a .wzc pointed at the server's patch server,
# the files in bundle\ and the helper files from a Huxley install. Players unzip it into their
# Huxley folder (the one with Binaries) and run WZLauncher.exe.
param(
    [Parameter(Mandatory)] [string] $Server,          # address players reach the server at
    [int] $Port = 8090,                               # the patch server's port
    # Page in the launcher's web panel (an IE control, 734 x 418): the website's launcher.html, for
    # example https://<site>/launcher.html. Default: the patch server's own /notice page.
    [string] $WebUrl,
    [string] $Output = (Join-Path $PSScriptRoot 'out\bundle'),
    # The Korean Huxley client: it ships the launcher's helper DLLs and LauncherImage folder.
    [Parameter(Mandatory)] [string] $KoreanInstall
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot

& cmd /c "`"$here\build.cmd`"" | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Launcher build failed.' }

if (Test-Path -LiteralPath $Output) { Remove-Item -LiteralPath $Output -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $Output 'LauncherImage') | Out-Null

Copy-Item -LiteralPath (Join-Path $here 'out\WZLauncher.exe') -Destination $Output
foreach ($file in 'AesCrypt.dll', 'ZlibAdapter.dll', 'RTPatchAdapter.dll', 'patchw32.dll') {
    Copy-Item -LiteralPath (Join-Path $KoreanInstall $file) -Destination $Output
}
Get-ChildItem -LiteralPath (Join-Path $KoreanInstall 'LauncherImage') -File |
    Where-Object Name -ne 'Thumbs.db' |
    Copy-Item -Destination (Join-Path $Output 'LauncherImage')
# Our replacements win: MainFrame.BMP carries the high-resolution logo (its lower half continues in
# the website's launcher.html as img/launcher-top.png).
Get-ChildItem -LiteralPath (Join-Path $here 'bundle\LauncherImage') -File |
    Copy-Item -Destination (Join-Path $Output 'LauncherImage') -Force

# The launcher reads its own version from this file; the patch server reports the same one.
[IO.File]::WriteAllText((Join-Path $Output 'LauncherVersion.txt'), '0003.0000')

$xml = [IO.File]::ReadAllText((Join-Path $here 'bundle\WZLauncher.xml'))
if ([string]::IsNullOrWhiteSpace($WebUrl)) { $WebUrl = "http://${Server}:$Port/notice" }
$xml = $xml.Replace('{WEBURL}', $WebUrl).Replace('{SERVER}', $Server).Replace('{PORT}', [string]$Port)
# WZLauncher.wzc is the config XML under AES-256-CBC with PKCS7 padding and a fixed key and IV
# (AesCrypt.dll decrypts it).
$aes = [Security.Cryptography.Aes]::Create()
try {
    $aes.Mode = 'CBC'; $aes.Padding = 'PKCS7'
    $aes.Key = [Text.Encoding]::ASCII.GetBytes('Jin Hye Jin JJang!! HaHa! Sin Na')
    $aes.IV = [Text.Encoding]::ASCII.GetBytes('Mellong~ Sim Sim')
    $plain = (New-Object Text.UTF8Encoding($false)).GetBytes($xml)
    $cipher = $aes.CreateEncryptor().TransformFinalBlock($plain, 0, $plain.Length)
    [IO.File]::WriteAllBytes((Join-Path $Output 'WZLauncher.wzc'), $cipher)
} finally {
    $aes.Dispose()
}

@"
Huxley launcher
===============

1. Start from a clean US Huxley install (the October 2010 English client).
2. Copy everything in this folder into the Huxley folder (the one that contains
   Binaries, HuxleyGame and HuxleyMMOGame).
3. Run WZLauncher.exe. It downloads the server's game files the first time
   (about 1.3 GB) and only changed files after that, then press Start.

The launcher talks to $Server on port $Port. If it says it cannot reach the
update server, check your internet connection and that the server is online.
"@ | Set-Content -LiteralPath (Join-Path $Output 'README.txt') -Encoding ascii

# Release asset name: the website links to releases/latest/download/LunariteLauncher.zip.
$zip = Join-Path (Split-Path -Parent $Output) 'LunariteLauncher.zip'
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path (Join-Path $Output '*') -DestinationPath $zip
Write-Output "Bundle: $Output ($zip)"
