param([switch]$CheckOnly, [switch]$Unload)
$ErrorActionPreference = 'Stop'
$taskDll = Join-Path $PSScriptRoot 'bin\x64\Release\SCC-StreamerDaily.dll'
$taskLog = [IO.Path]::ChangeExtension($taskDll, '.log')
$taskExeHash = 'DAC4C516BF382A4FBE809EA4940F830D54CEF92CC035DB55DF5871E4A694666C'
$taskAssemblyHash = 'D237B41BF9DD4835F2C56E980A19F1AAA523036275A0F780286B67DB7A0EB2C7'

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
public static class SCCStreamerDailyLoader {
 [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, int id);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool WriteProcessMemory(IntPtr process, IntPtr address, byte[] data, UIntPtr size, out UIntPtr written);
 [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr address, UIntPtr size, uint type, uint protection);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint type);
 [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr CreateRemoteThread(IntPtr process, IntPtr attributes, UIntPtr stack, IntPtr start, IntPtr argument, uint flags, out uint id);
 [DllImport("kernel32.dll", SetLastError=true)] static extern uint WaitForSingleObject(IntPtr handle, uint timeout);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr OpenEventW(uint access, bool inherit, string name);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetEvent(IntPtr handle);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandleW(string name);
 [DllImport("kernel32.dll", CharSet=CharSet.Ansi, ExactSpelling=true)] static extern IntPtr GetProcAddress(IntPtr module, string name);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool GetModuleHandleExW(uint flags, IntPtr address, out IntPtr module);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern uint GetModuleFileNameW(IntPtr module, StringBuilder name, int length);
 static void Require(bool ok) { if (!ok) throw new Win32Exception(Marshal.GetLastWin32Error()); }
 public static IntPtr Open(int id) { var handle=OpenProcess(0x43A, false, id); Require(handle!=IntPtr.Zero); return handle; }
 public static void Close(IntPtr handle) { CloseHandle(handle); }
 public static void RequestStop(int id) {
   var stop=OpenEventW(2, false, "Local\\SCCStreamerDaily.Stop." + id);
   if (stop==IntPtr.Zero) throw new Exception("Bu kopyada guvenli kaldirma baglantisi bulunamadi.");
   try { Require(SetEvent(stop)); } finally { CloseHandle(stop); }
 }
 public static string LoadLibraryOwner(out long offset) {
   var function=GetProcAddress(GetModuleHandleW("kernel32.dll"), "LoadLibraryW"); IntPtr owner;
   Require(function!=IntPtr.Zero); Require(GetModuleHandleExW(6, function, out owner));
   offset=function.ToInt64()-owner.ToInt64();
   var path=new StringBuilder(32768); Require(GetModuleFileNameW(owner, path, path.Capacity)!=0);
   return System.IO.Path.GetFileName(path.ToString());
 }
 public static void Load(IntPtr process, long loadLibrary, string path) {
   var data=Encoding.Unicode.GetBytes(path + "\0");
   var remote=VirtualAllocEx(process, IntPtr.Zero, (UIntPtr)data.Length, 0x3000, 4); Require(remote!=IntPtr.Zero);
   IntPtr thread=IntPtr.Zero; bool finished=false;
   try {
     UIntPtr written; Require(WriteProcessMemory(process, remote, data, (UIntPtr)data.Length, out written));
     if (written.ToUInt64()!=(ulong)data.Length) throw new Exception("DLL yolu eksik yazildi.");
     uint id; thread=CreateRemoteThread(process, IntPtr.Zero, UIntPtr.Zero, new IntPtr(loadLibrary), remote, 0, out id); Require(thread!=IntPtr.Zero);
     if (WaitForSingleObject(thread, 15000)!=0) throw new Exception("DLL yukleme zaman asimi.");
     finished=true;
   } finally {
     if (thread!=IntPtr.Zero) CloseHandle(thread);
     if (finished) VirtualFreeEx(process, remote, UIntPtr.Zero, 0x8000);
   }
 }
}
'@

try {
 if (![Environment]::Is64BitProcess) { throw '64 bit PowerShell gerekiyor.' }
 if ($CheckOnly -and $Unload) { throw '-CheckOnly ve -Unload birlikte kullanilamaz.' }
 $taskGame = Get-Process -Name 'Streamer Daily' -ErrorAction SilentlyContinue
 if ($Unload) {
   if (!$taskGame) { Write-Host 'Oyun kapali; kaldirilacak aktif kopya yok.'; exit 0 }
   if (@($taskGame).Count -ne 1) { throw 'Tek bir Streamer Daily islemi gerekiyor.' }
   $taskGame.Refresh()
   if (!@($taskGame.Modules | Where-Object ModuleName -eq 'SCC-StreamerDaily.dll').Count) { Write-Host 'Aktif kopya yok.'; exit 0 }
   [SCCStreamerDailyLoader]::RequestStop($taskGame.Id)
   $taskDeadline = (Get-Date).AddSeconds(15)
   do {
     Start-Sleep -Milliseconds 200
     $taskGame.Refresh()
     $taskLoaded = @($taskGame.Modules | Where-Object ModuleName -eq 'SCC-StreamerDaily.dll').Count -gt 0
   } until (!$taskLoaded -or (Get-Date) -gt $taskDeadline)
   if ($taskLoaded) { throw 'Kaldirma istegi gonderildi ancak temizlik zamaninda bitmedi.' }
   Write-Host 'KALDIRILDI: menu ve internal DLL temizlendi.'
   exit 0
 }
 if (!(Test-Path -LiteralPath $taskDll) -and !$CheckOnly) { throw 'DLL bulunamadi. Once Build-StreamerDaily.ps1 calistirin.' }
 if (!$taskGame -and !$CheckOnly) {
   Write-Host 'Streamer Daily baslatiliyor...'
   Start-Process 'steam://rungameid/1058380'
   $taskDeadline = (Get-Date).AddSeconds(120)
   do {
     Start-Sleep -Seconds 2
     $taskGame = Get-Process -Name 'Streamer Daily' -ErrorAction SilentlyContinue
   } until ($taskGame -or (Get-Date) -gt $taskDeadline)
 }
 if (!$taskGame -or @($taskGame).Count -ne 1) { throw 'Tek bir Streamer Daily islemi bulunamadi.' }
 $taskGame.Refresh()
 if ((Get-FileHash -LiteralPath $taskGame.Path -Algorithm SHA256).Hash -ne $taskExeHash) {
   throw 'Oyun EXE surumu bu derlemeyle uyusmuyor. DLL yuklenmedi.'
 }
 $taskManaged = Join-Path (Split-Path $taskGame.Path) 'Streamer Daily_Data\Managed\Assembly-CSharp.dll'
 if (!(Test-Path -LiteralPath $taskManaged) -or (Get-FileHash -LiteralPath $taskManaged -Algorithm SHA256).Hash -ne $taskAssemblyHash) {
   throw 'Assembly-CSharp surumu bu derlemeyle uyusmuyor. DLL yuklenmedi.'
 }
 if (!@($taskGame.Modules | Where-Object ModuleName -eq 'mono-2.0-bdwgc.dll').Count) { throw 'Mono henuz yuklenmedi; oyun ana menusu acilinca yeniden deneyin.' }
 Write-Host "Uyumluluk dogrulandi: Steam build 6248886, PID $($taskGame.Id)."
 if ($CheckOnly) { exit 0 }
 $taskExisting = @($taskGame.Modules | Where-Object ModuleName -eq 'SCC-StreamerDaily.dll')
 if ($taskExisting.Count) {
   if (![IO.Path]::GetFullPath($taskExisting[0].FileName).Equals([IO.Path]::GetFullPath($taskDll), [StringComparison]::OrdinalIgnoreCase)) {
     throw 'Baska bir SCC-StreamerDaily kopyasi zaten yuklu.'
   }
   Write-Host 'SCC-StreamerDaily zaten aktif.'
   exit 0
 }
 $taskHandle = [SCCStreamerDailyLoader]::Open($taskGame.Id)
 try {
   $taskOffset = [long]0
   $taskOwner = [SCCStreamerDailyLoader]::LoadLibraryOwner([ref]$taskOffset)
   $taskOwnerModule = $taskGame.Modules | Where-Object ModuleName -eq $taskOwner | Select-Object -First 1
   if (!$taskOwnerModule) { throw 'Windows yukleyici modulu bulunamadi.' }
   [SCCStreamerDailyLoader]::Load($taskHandle, $taskOwnerModule.BaseAddress.ToInt64() + $taskOffset, $taskDll)
 } finally { [SCCStreamerDailyLoader]::Close($taskHandle) }
 $taskDeadline = (Get-Date).AddSeconds(20)
 do {
   Start-Sleep -Milliseconds 250
   $taskGame.Refresh()
   $taskLoaded = @($taskGame.Modules | Where-Object { $_.FileName -eq $taskDll }).Count -gt 0
   $taskContent = if (Test-Path -LiteralPath $taskLog) { Get-Content -LiteralPath $taskLog -Raw } else { '' }
   if ($taskLoaded -and $taskContent -match 'Mono bridge ready' -and $taskContent -match 'Menu rendering active') { break }
 } until ((Get-Date) -gt $taskDeadline)
 if (!$taskLoaded) { throw 'DLL oyun surecine yuklenemedi.' }
 if ($taskContent -notmatch 'Mono bridge ready') { throw "DLL yuklendi ancak Mono baglantisi dogrulanamadi. Kayit: $taskLog" }
 if ($taskContent -notmatch 'Menu rendering active') { throw "DLL yuklendi ancak ImGui cizimi dogrulanamadi. Kayit: $taskLog" }
 Write-Host 'AKTIF: Mono oyun verisi ve ImGui menusu dogrulandi.'
 Write-Host 'INSERT: menuyu ac/kapat | Sistem: ayarlari kaydet veya hileyi tamamen kaldir'
 Write-Host "Kayit: $taskLog"
 exit 0
} catch {
 Write-Host "HATA: $($_.Exception.Message)" -ForegroundColor Red
 exit 1
}

