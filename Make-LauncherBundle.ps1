# Builds the player launcher: WZLauncher.exe, a .wzc pointed at the server's patch server, the files
# in bundle\ and the helper files from a Huxley install. Output, next to the bundle folder:
#   LunariteLauncher-win-Portable.zip  unzip into an empty folder; the launcher downloads the game there
#   LunariteLauncher-win-Setup.exe     installs the launcher with shortcuts (needs Inno Setup 6)
param(
    [Parameter(Mandatory)] [string] $Server,          # address players reach the server at
    [int] $Port = 8090,                               # the patch server's port
    # Page in the launcher's web panel (an IE control, 734 x 418): the website's launcher.html, for
    # example https://<site>/launcher.html. Default: the patch server's own /notice page.
    [string] $WebUrl,
    # Page for Korean Windows; default: $WebUrl with launcher.html -> launcher-ko.html.
    [string] $WebUrlKorean,
    [string] $Output = (Join-Path $PSScriptRoot 'out\bundle'),
    # The Korean Huxley client: it ships the launcher's helper DLLs and LauncherImage folder.
    [Parameter(Mandatory)] [string] $KoreanInstall,
    [string] $Version = '1.0.0'
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot

# vcvarsall prints harmless noise on stderr; only the exit code matters.
& cmd /c "`"$here\build.cmd`" >nul 2>&1"
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

if ([string]::IsNullOrWhiteSpace($WebUrl)) { $WebUrl = "http://${Server}:$Port/notice" }
# The Korean news page sits next to the English one (launcher.html -> launcher-ko.html).
if ([string]::IsNullOrWhiteSpace($WebUrlKorean)) { $WebUrlKorean = $WebUrl -replace 'launcher\.html$', 'launcher-ko.html' }
# WZLauncher.wzc is the config XML under AES-256-CBC with PKCS7 padding and a fixed key and IV
# (AesCrypt.dll decrypts it). A Korean Windows reads WZLauncher.kor.wzc instead.
$aes = [Security.Cryptography.Aes]::Create()
try {
    $aes.Mode = 'CBC'; $aes.Padding = 'PKCS7'
    $aes.Key = [Text.Encoding]::ASCII.GetBytes('Jin Hye Jin JJang!! HaHa! Sin Na')
    $aes.IV = [Text.Encoding]::ASCII.GetBytes('Mellong~ Sim Sim')
    foreach ($config in @(@('WZLauncher.xml', 'WZLauncher.wzc'), @('WZLauncher.kor.xml', 'WZLauncher.kor.wzc'))) {
        $xml = [IO.File]::ReadAllText((Join-Path $here "bundle\$($config[0])"), [Text.Encoding]::UTF8)
        $xml = $xml.Replace('{WEBURL_KOR}', $WebUrlKorean).Replace('{WEBURL}', $WebUrl).Replace('{SERVER}', $Server).Replace('{PORT}', [string]$Port)
        $plain = (New-Object Text.UTF8Encoding($false)).GetBytes($xml)
        $cipher = $aes.CreateEncryptor().TransformFinalBlock($plain, 0, $plain.Length)
        [IO.File]::WriteAllBytes((Join-Path $Output $config[1]), $cipher)
    }
} finally {
    $aes.Dispose()
}

@"
Huxley launcher
===============

1. Put this folder where you want the game, with a short path such as
   C:\Games\Lunarite and about 23 GB free.
2. Run WZLauncher.exe. The first time it downloads the whole game (about
   22 GB); after that only changed files. Then press Start.

The launcher talks to $Server on port $Port. If it says it cannot reach the
update server, check your internet connection and that the server is online.
"@ | Set-Content -LiteralPath (Join-Path $Output 'README.txt') -Encoding ascii

# Release asset names; the website links to releases/latest/download/LunariteLauncher-win-Setup.exe.
$dist = Split-Path -Parent $Output
$zip = Join-Path $dist 'LunariteLauncher-win-Portable.zip'
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path (Join-Path $Output '*') -DestinationPath $zip
Write-Output "Portable: $zip"

$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $iscc) { Write-Warning 'Inno Setup 6 not found; skipped the Setup.exe.'; return }
& $iscc /Q "/DAppVersion=$Version" "/DBundleDir=$Output" "/O$dist" (Join-Path $here 'installer\LunariteLauncher.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer build failed.' }
Write-Output "Setup: $(Join-Path $dist 'LunariteLauncher-win-Setup.exe')"
