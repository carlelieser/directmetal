# Loads every DLL under a directory into this process and fails unless each one loads from that
# file, not from a same-named system DLL. Run from a process whose bitness matches the DLLs.
# Usage: load_dlls.ps1 -Directory <dir> -Architecture x64|x86
param(
    [Parameter(Mandatory)] [string] $Directory,
    [Parameter(Mandatory)] [ValidateSet('x64', 'x86')] [string] $Architecture
)
$ErrorActionPreference = 'Stop'

$isExpecting64Bit = $Architecture -eq 'x64'
if ([Environment]::Is64BitProcess -ne $isExpecting64Bit) {
    throw "load_dlls: $Architecture DLLs need a matching process; this one is not $Architecture"
}

Add-Type -Namespace DirectMetal -Name Kernel32 -MemberDefinition @'
[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern IntPtr LoadLibraryW(string path);
[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern uint GetModuleFileNameW(IntPtr module, System.Text.StringBuilder path, uint size);
[DllImport("kernel32.dll", SetLastError = true)]
public static extern bool FreeLibrary(IntPtr module);
'@

function Test-DllLoad([System.IO.FileInfo] $Dll) {
    $module = [DirectMetal.Kernel32]::LoadLibraryW($Dll.FullName)
    if ($module -eq [IntPtr]::Zero) {
        $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "load_dlls: LoadLibraryW failed for $($Dll.FullName): Win32 error $code"
    }
    $loadedPath = New-Object System.Text.StringBuilder 1024
    [void][DirectMetal.Kernel32]::GetModuleFileNameW($module, $loadedPath, $loadedPath.Capacity)
    [void][DirectMetal.Kernel32]::FreeLibrary($module)
    if ($loadedPath.ToString() -ne $Dll.FullName) {
        throw "load_dlls: $($Dll.Name) resolved to $loadedPath instead of $($Dll.FullName)"
    }
    Write-Output "loaded $($Dll.FullName)"
}

$dlls = @(Get-ChildItem -Path $Directory -Recurse -Filter *.dll)
if ($dlls.Count -eq 0) {
    throw "load_dlls: no DLLs found under $Directory"
}
foreach ($dll in $dlls) {
    Test-DllLoad $dll
}
