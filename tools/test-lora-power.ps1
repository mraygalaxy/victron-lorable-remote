param(
 [string]$Zig = 'zig',
 [string]$RuiRoot = (Join-Path $env:LOCALAPPDATA 'Arduino15/packages/rak_rui/hardware/stm32/4.2.4')
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $projectRoot 'test-results'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = Join-Path $PSScriptRoot 'tests/lora-power.cpp'
$mockIncludes = '-I' + (Join-Path $PSScriptRoot 'tests/power')
foreach ($mode in @('manual', 'standard')) {
 $defines = @()
 if ($mode -eq 'standard') { $defines = @('-DTEST_STANDARD') }
 $exe = Join-Path $output "lora-power-$mode.exe"
 & $Zig c++ -std=c++11 -Wall -Wextra -Werror @defines $mockIncludes $source -o $exe
 if ($LASTEXITCODE -ne 0) { throw "$mode power test compilation failed" }
 & $exe
 if ($LASTEXITCODE -ne 0) { throw "$mode power test failed" }
}
# Compile the actual installed vendor channel-selection implementation, without
# altering the BSP, mocking only the radio/context boundary and monotonic time.
$stack = Join-Path $RuiRoot 'cores/STM32WLE/external/lora/LoRaMac-node-4.7.0/src'
$includes = @('mac', 'mac/region', 'system', 'boards', 'radio') | ForEach-Object { '-I' + (Join-Path $stack $_) }
$vendorObject = Join-Path $output 'region-common-vendor.o'
& $Zig cc -std=c99 -DREGION_EU868 -ffunction-sections -fdata-sections @includes -c (Join-Path $stack 'mac/region/RegionCommon.c') -o $vendorObject
if ($LASTEXITCODE -ne 0) { throw 'Vendor region compilation failed' }
$vendorExe = Join-Path $output 'lora-power-vendor.exe'
& $Zig c++ -std=c++11 -DREGION_EU868 -DTEST_VENDOR_REAL -ffunction-sections -fdata-sections @includes $source $vendorObject -o $vendorExe
if ($LASTEXITCODE -ne 0) { throw 'Vendor-backed power test compilation failed' }
& $vendorExe
if ($LASTEXITCODE -ne 0) { throw 'Vendor-backed power test failed' }
