Param([Parameter(Mandatory=$true)][string]$Config)
# Get PowerShell 7.2.1 or higher from the MS Store: https://aka.ms/Install-PowerShell

$IsFinal=$($Config -eq "Final")
# Windows 11 SDK (10.0.22000.0)
$Compiler="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22000.0\x64\dxc.exe"
$OutputDir="${PSScriptRoot}\..\bin\${Config}\shaders"

if (!(Test-Path -Path $OutputDir))
{
	New-Item -Path $OutputDir -ItemType Directory
}
$OutputDir=Resolve-Path $OutputDir

#compile shaders
foreach($file in Get-ChildItem -Path $PSScriptRoot -Filter *.hlsl) {
	$Entry=[System.IO.Path]::GetFileNameWithoutExtension($file)
	$AdditionalParamsVS="-Od", "-Zi",  "-Fd", "${OutputDir}\${Entry}_VS.pdb"
	$AdditionalParamsPS="-Od", "-Zi",  "-Fd", "${OutputDir}\${Entry}_PS.pdb"

	if($IsFinal) {
		$AdditionalParamsPS=$AdditionalParamsVS="-Qstrip_debug"		
	}
	& $Compiler -T vs_6_0 -E vs_main @AdditionalParamsVS $file -Fo ${OutputDir}\${Entry}_VS.cso
	& $Compiler -T ps_6_0 -E ps_main @AdditionalParamsPS $file -Fo ${OutputDir}\${Entry}_PS.cso
}
