# Loads every DLL under a directory into this process and fails unless each one loads from that
# file, not from a same-named system DLL. Imports between the DLLs (d3d11.dll -> directmetal.dll)
# resolve from the directories under test. Run from a process whose bitness matches the DLLs.
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
public static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern IntPtr AddDllDirectory(string path);
[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern uint GetModuleFileNameW(IntPtr module, System.Text.StringBuilder path, uint size);
[DllImport("kernel32.dll", SetLastError = true)]
public static extern bool FreeLibrary(IntPtr module);
'@

# LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
$searchFlags = 0x100 -bor 0x1000

function Add-DllDirectory([string] $Path) {
    if ([DirectMetal.Kernel32]::AddDllDirectory($Path) -eq [IntPtr]::Zero) {
        $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "load_dlls: AddDllDirectory failed for ${Path}: Win32 error $code"
    }
}

function Test-DllLoad([System.IO.FileInfo] $Dll) {
    $module = [DirectMetal.Kernel32]::LoadLibraryExW($Dll.FullName, [IntPtr]::Zero, $searchFlags)
    if ($module -eq [IntPtr]::Zero) {
        $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "load_dlls: LoadLibraryExW failed for $($Dll.FullName): Win32 error $code"
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
foreach ($directory in @($dlls | ForEach-Object { $_.DirectoryName } | Sort-Object -Unique)) {
    Add-DllDirectory $directory
}
foreach ($dll in $dlls) {
    Test-DllLoad $dll
}
