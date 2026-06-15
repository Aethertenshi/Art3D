$DxcPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe"

if (!(Test-Path $DxcPath)) {
    Write-Error "dxc.exe not found at $DxcPath. Please verify Windows SDK installation."
    exit 1
}

# Ensure we are in the directory containing the script
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $ScriptDir

Write-Host "Compiling VS Main to DXIL..."
& $DxcPath -T vs_6_0 -E VSMain -Fo shader.vs.dxil shader.hlsl

Write-Host "Compiling PS Main to DXIL..."
& $DxcPath -T ps_6_0 -E PSMain -Fo shader.ps.dxil shader.hlsl

Write-Host "Compiling VS Main to SPIR-V..."
& $DxcPath -T vs_6_0 -E VSMain -spirv -fvk-invert-y -Fo shader.vs.spv shader.hlsl

Write-Host "Compiling PS Main to SPIR-V..."
& $DxcPath -T ps_6_0 -E PSMain -spirv -Fo shader.ps.spv shader.hlsl

Pop-Location
Write-Host "Shader compilation complete!"
