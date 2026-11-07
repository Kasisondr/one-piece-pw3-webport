param([string]$InputDump = 'rom/PCSE00638', [string]$OutputDump = 'generated/decrypted/PCSE00638')
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
if (!(Test-Path external/pfs-vita-enc/pfs.py)) {
    git clone --depth 1 https://github.com/cobra838/pfs-vita-enc.git external/pfs-vita-enc
    if ($LASTEXITCODE -ne 0) { throw 'Could not fetch PFS tool' }
}
git -C external/pfs-vita-enc fetch --depth 1 origin a4fe52fae9ee323e443fe225222f29ca0edea5a1
if ($LASTEXITCODE -ne 0) { throw 'Could not fetch pinned PFS tool' }
git -C external/pfs-vita-enc checkout a4fe52fae9ee323e443fe225222f29ca0edea5a1
if ($LASTEXITCODE -ne 0) { throw 'Could not select pinned PFS tool' }
if (!(Test-Path external/crypto-venv/Scripts/python.exe)) {
    python -m venv external/crypto-venv
    & external/crypto-venv/Scripts/python.exe -m pip install 'pycryptodome==3.24.1'
    if ($LASTEXITCODE -ne 0) { throw 'Could not install local crypto dependency' }
}
& external/crypto-venv/Scripts/python.exe external/pfs-vita-enc/pfs.py decrypt $InputDump $OutputDump
if ($LASTEXITCODE -ne 0) { throw 'PFS decryption failed; partial output retained for inspection' }
Write-Host 'PFS decrypted. Retail SELF modules still need Vita3K Other > Decrypt All SELF.'
