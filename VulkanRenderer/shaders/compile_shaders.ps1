Param([Parameter(Mandatory=$true)][string]$Config)
# Get PowerShell 7.2.1 or higher from the MS Store: https://aka.ms/Install-PowerShell

$IsFinal=$($Config -eq "Final")
# Vulkan SDK Environment variable is set via VulkanSDK installer (https://vulkan.lunarg.com)
$Compiler="${Env:VULKAN_SDK}\Bin\dxc.exe"
$OutputDir="${PSScriptRoot}\..\bin\${Config}\shaders"
$ShaderModel="6_7"

if (!(Test-Path -Path $OutputDir))
{
	New-Item -Path $OutputDir -ItemType Directory
}
$OutputDir=Resolve-Path $OutputDir

#compile shaders
foreach($file in Get-ChildItem -Path $PSScriptRoot -Filter *.hlsl) {
	$Entry=[System.IO.Path]::GetFileNameWithoutExtension($file)
	$AdditionalParamsVS = "-fspv-target-env=vulkan1.3", "-spirv"
	$AdditionalParamsPS = "-fspv-target-env=vulkan1.3", "-spirv"

	if($IsFinal) {
		$FinalParams = "-Qstrip_debug"
		$AdditionalParamsPS += , $FinalParams		
	} else {
		$DebugParams = "-Od", "-Zi"
		$AdditionalParamsPS += , $DebugParams
		$AdditionalParamsVS += , $DebugParams
	}
	& $Compiler -T vs_${ShaderModel} -E vs_main @AdditionalParamsVS $file -Fo ${OutputDir}\${Entry}_VS.spv
	& $Compiler -T ps_${ShaderModel} -E ps_main @AdditionalParamsPS $file -Fo ${OutputDir}\${Entry}_PS.spv
}
