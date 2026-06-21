$DxcPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe"

if (!(Test-Path $DxcPath)) {
    Write-Error "dxc.exe not found at $DxcPath. Please verify Windows SDK installation."
    exit 1
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $ScriptDir

Write-Host "Compiling VS Main to DXIL..."
& $DxcPath -T vs_6_0 -E VSMain -Fo shader.vs.dxil shader.hlsl

Write-Host "Compiling VS Shadow to DXIL..."
& $DxcPath -T vs_6_0 -E VSShadow -Fo shadow.vs.dxil shader.hlsl

Write-Host "Compiling PS Main to DXIL..."
& $DxcPath -T ps_6_0 -E PSMain -Fo shader.ps.dxil shader.hlsl

Write-Host "Compiling PS Shadow (no-op) to DXIL..."
& $DxcPath -T ps_6_0 -E PSShadow -Fo shadow.ps.dxil shader.hlsl

Write-Host "Compiling VS Sky to DXIL..."
& $DxcPath -T vs_6_0 -E VSSky -Fo sky.vs.dxil shader.hlsl

Write-Host "Compiling PS Sky to DXIL..."
& $DxcPath -T ps_6_0 -E PSSky -Fo sky.ps.dxil shader.hlsl

Pop-Location
Write-Host "Shader compilation complete!"
