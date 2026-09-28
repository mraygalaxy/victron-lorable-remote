param([string]$ArduinoCli='C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe')
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$installed=& $ArduinoCli core list --format json | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or ($installed.platforms|Where-Object id -eq 'rak_rui:stm32').installed_version -ne '4.2.4'){throw 'RAK RUI BSP 4.2.4 required'}
# The first-install helper uses stock BSP flags, even if the optional project
# platform.local.txt is installed: it does not contain the main app's wrappers.
& $ArduinoCli compile --fqbn 'rak_rui:stm32:WisDuoRAK11160Board:supportlora=2,supportLA915=2' --build-property 'compiler.cpp.extra_flags=-DDEBUG' --build-property 'compiler.c.elf.extra_flags=' --build-property 'compiler.ar.cmd=arm-none-eabi-ar' --build-path (Join-Path $project 'build_first_install') (Join-Path $project 'installer/bootstrap')
if($LASTEXITCODE -ne 0){throw 'First-install helper build failed'}
$imagePath=Join-Path $project 'build_first_install/bootstrap.ino.bin'
$resource=[ordered]@{format=1;target='RAK11162';protocol=1;bsp='4.2.4';sha256=(Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash.ToLowerInvariant();image_base64=[Convert]::ToBase64String([IO.File]::ReadAllBytes($imagePath))}
[IO.File]::WriteAllText((Join-Path $project 'installer/bootstrap-image.json'),($resource|ConvertTo-Json -Compress),[Text.UTF8Encoding]::new($false))
Write-Host 'First-install helper built. Nothing flashed.'
