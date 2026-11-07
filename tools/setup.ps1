$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
function Checked([scriptblock]$Action) {
    & $Action
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $Action" }
}
if (!(Test-Path external/vitarecomp)) {
    Checked { git clone https://github.com/soyjxck/vitarecomp.git external/vitarecomp }
}
Checked { git -C external/vitarecomp checkout 5a095d7f429b1e51ffca3af438b18ad1502f07fb }
if (!(Test-Path external/vita-headers)) {
    Checked { git clone --depth 1 https://github.com/vitasdk/vita-headers.git external/vita-headers }
}
Checked { git -C external/vita-headers fetch --depth 1 origin e66ebe90b73d1fa4cce005a5b2072cec27322544 }
Checked { git -C external/vita-headers checkout e66ebe90b73d1fa4cce005a5b2072cec27322544 }
if (!(Test-Path external/emsdk)) {
    Checked { git clone https://github.com/emscripten-core/emsdk.git external/emsdk }
}
Checked { & ./external/emsdk/emsdk.bat install 6.0.13 }
Checked { & ./external/emsdk/emsdk.bat activate 6.0.13 }
Write-Host 'Ready. Run python tools/build.py demo'
