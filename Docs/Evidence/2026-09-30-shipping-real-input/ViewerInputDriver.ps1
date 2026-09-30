# ViewerInputDriver.ps1
# Drives a running CharacterShowcase window with REAL OS-level input (user32 SendInput)
# and captures the window client area to PNG. Intended for Shipping-package verification
# where no automation tests are compiled in. Windows PowerShell 5.1.
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File ViewerInputDriver.ps1 -ProcessId 1234 -StepsJson steps.json -OutDir shots
#   powershell ... -WindowTitle "CharacterShowcase" ...
#
# steps.json = array of step objects. Coordinates fx/fy are FRACTIONS of the client size (0..1).
#   {"a":"wait","ms":3000}
#   {"a":"shot","name":"01_default"}
#   {"a":"move","fx":0.45,"fy":0.5}
#   {"a":"down"} / {"a":"up"}
#   {"a":"drag","fx":0.45,"fy":0.5,"tx":0.65,"ty":0.5,"steps":20,"ms":15}   # press, move in N steps, release
#   {"a":"wheel","delta":-120}      # negative = wheel down (zoom out in UE default), positive = up
#   {"a":"key","k":"R"}             # R SPACE H I W ESC
#   {"a":"focusloss"}               # alt-tab away and back (activates another window then returns)
param(
    [int]$ProcessId = 0,
    [string]$WindowTitle = "CharacterShowcase",
    [Parameter(Mandatory=$true)][string]$StepsJson,
    [Parameter(Mandatory=$true)][string]$OutDir
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$sig = @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class Win32 {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Explicit)] public struct INPUTUNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public INPUTUNION u; }
    [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    public static bool ForceForeground(IntPtr h) {
        uint pid; uint target = GetWindowThreadProcessId(h, out pid);
        uint fgThread = GetWindowThreadProcessId(GetForegroundWindow(), out pid);
        uint me = GetCurrentThreadId();
        ShowWindow(h, 9);
        AttachThreadInput(me, target, true); AttachThreadInput(me, fgThread, true);
        BringWindowToTop(h); SetForegroundWindow(h);
        AttachThreadInput(me, target, false); AttachThreadInput(me, fgThread, false);
        System.Threading.Thread.Sleep(300);
        return GetForegroundWindow() == h;
    }
    public static IntPtr FindByPidOrTitle(uint pid, string title) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((h, l) => {
            if (!IsWindowVisible(h)) return true;
            uint p; GetWindowThreadProcessId(h, out p);
            var sb = new StringBuilder(256); GetWindowText(h, sb, 256);
            bool ok = (pid != 0 && p == pid) || (pid == 0 && sb.ToString().IndexOf(title, StringComparison.OrdinalIgnoreCase) >= 0);
            if (ok && sb.Length > 0) { found = h; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    const uint INPUT_MOUSE = 0, INPUT_KEYBOARD = 1;
    const uint MOUSEEVENTF_MOVE = 0x0001, MOUSEEVENTF_LEFTDOWN = 0x0002, MOUSEEVENTF_LEFTUP = 0x0004, MOUSEEVENTF_WHEEL = 0x0800, MOUSEEVENTF_ABSOLUTE = 0x8000;
    const uint KEYEVENTF_KEYUP = 0x0002, KEYEVENTF_SCANCODE = 0x0008;
    static INPUT M(uint flags, int dx, int dy, uint data) { var i = new INPUT(); i.type = INPUT_MOUSE; i.u.mi.dwFlags = flags; i.u.mi.dx = dx; i.u.mi.dy = dy; i.u.mi.mouseData = data; return i; }
    public static void MoveAbs(int x, int y) {
        int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
        int ax = (int)Math.Round(x * 65535.0 / (sw - 1)), ay = (int)Math.Round(y * 65535.0 / (sh - 1));
        SendInput(1, new[] { M(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, ax, ay, 0) }, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void LeftDown() { SendInput(1, new[] { M(MOUSEEVENTF_LEFTDOWN, 0, 0, 0) }, Marshal.SizeOf(typeof(INPUT))); }
    public static void LeftUp()   { SendInput(1, new[] { M(MOUSEEVENTF_LEFTUP, 0, 0, 0) }, Marshal.SizeOf(typeof(INPUT))); }
    public static void Wheel(int delta) { SendInput(1, new[] { M(MOUSEEVENTF_WHEEL, 0, 0, unchecked((uint)delta)) }, Marshal.SizeOf(typeof(INPUT))); }
    public static void Key(ushort vk) { Key(vk, 60); }
    public static void Key(ushort vk, int holdMs) {
        ushort sc = (ushort)MapVirtualKey(vk, 0);
        var d = new INPUT(); d.type = INPUT_KEYBOARD; d.u.ki.wVk = vk; d.u.ki.wScan = sc; d.u.ki.dwFlags = KEYEVENTF_SCANCODE;
        var u = new INPUT(); u.type = INPUT_KEYBOARD; u.u.ki.wVk = vk; u.u.ki.wScan = sc; u.u.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        SendInput(1, new[] { d }, Marshal.SizeOf(typeof(INPUT))); System.Threading.Thread.Sleep(holdMs);
        SendInput(1, new[] { u }, Marshal.SizeOf(typeof(INPUT)));
    }
}
'@
Add-Type -TypeDefinition $sig -ReferencedAssemblies System.Windows.Forms

$vk = @{ 'R'=0x52; 'SPACE'=0x20; 'H'=0x48; 'I'=0x49; 'W'=0x57; 'ESC'=0x1B; 'F'=0x46 }

$hwnd = [Win32]::FindByPidOrTitle([uint32]$ProcessId, $WindowTitle)
if ($hwnd -eq [IntPtr]::Zero) { throw "Window not found (pid=$ProcessId title~'$WindowTitle')" }
[Win32]::ShowWindow($hwnd, 9) | Out-Null   # SW_RESTORE
[Win32]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 500

function Get-ClientBounds {
    $r = New-Object Win32+RECT
    [Win32]::GetClientRect($hwnd, [ref]$r) | Out-Null
    $p = New-Object Win32+POINT; $p.X = 0; $p.Y = 0
    [Win32]::ClientToScreen($hwnd, [ref]$p) | Out-Null
    return @{ X=$p.X; Y=$p.Y; W=($r.Right-$r.Left); H=($r.Bottom-$r.Top) }
}
function To-Screen($fx, $fy) {
    $b = Get-ClientBounds
    return @{ X = [int]($b.X + $fx * $b.W); Y = [int]($b.Y + $fy * $b.H) }
}
function Move-To($fx, $fy) { $s = To-Screen $fx $fy; [Win32]::MoveAbs($s.X, $s.Y); Start-Sleep -Milliseconds 40 }
function Take-Shot($name) {
    $b = Get-ClientBounds
    $bmp = New-Object System.Drawing.Bitmap $b.W, $b.H
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($b.X, $b.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    New-Item -ItemType Directory -Force $OutDir | Out-Null
    $path = Join-Path $OutDir ("$name.png")
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Write-Host "SHOT $path ($($b.W)x$($b.H))"
}

function Assert-GameForeground($context) {
    # SAFETY: never inject input unless the game window is verified to be the foreground window.
    # Otherwise clicks/keys would land in whatever application is in front (this happened once).
    for ($try = 0; $try -lt 5; $try++) {
        if ([Win32]::GetForegroundWindow() -eq $hwnd) { return }
        if ([Win32]::ForceForeground($hwnd)) { return }
        Start-Sleep -Milliseconds 300
    }
    throw "ABORT ($context): game window is not the foreground window; another application is in front. No input was sent for this step."
}

$steps = Get-Content -Raw -Encoding UTF8 $StepsJson | ConvertFrom-Json
foreach ($s in $steps) {
    if ($s.a -ne 'focusloss' -and $s.a -ne 'wait') { Assert-GameForeground $s.a }
    switch ($s.a) {
        'wait'  { Start-Sleep -Milliseconds $s.ms; Write-Host "wait $($s.ms)" }
        'shot'  { Take-Shot $s.name }
        'move'  { Move-To $s.fx $s.fy; Write-Host "move $($s.fx),$($s.fy)" }
        'down'  { [Win32]::LeftDown(); Write-Host "down" }
        'up'    { [Win32]::LeftUp(); Write-Host "up" }
        'drag'  {
            $n = if ($s.steps) { [int]$s.steps } else { 20 }
            $dm = if ($s.ms) { [int]$s.ms } else { 15 }
            Move-To $s.fx $s.fy; [Win32]::LeftDown(); Start-Sleep -Milliseconds 80
            for ($i = 1; $i -le $n; $i++) {
                $fx = $s.fx + ($s.tx - $s.fx) * $i / $n; $fy = $s.fy + ($s.ty - $s.fy) * $i / $n
                Move-To $fx $fy; Start-Sleep -Milliseconds $dm
            }
            Start-Sleep -Milliseconds 80; [Win32]::LeftUp()
            Write-Host "drag $($s.fx),$($s.fy) -> $($s.tx),$($s.ty)"
        }
        'wheel' { [Win32]::Wheel([int]$s.delta); Write-Host "wheel $($s.delta)" }
        'key'   { $hold = if ($s.hold) { [int]$s.hold } else { 60 }; [Win32]::Key([uint16]$vk[$s.k.ToUpper()], $hold); Write-Host "key $($s.k) hold=$hold" }
        'focusloss' {
            # Activate the desktop/taskbar (Win+D would minimize; instead use a hidden helper window)
            $f = New-Object System.Windows.Forms.Form; $f.Text = 'FocusStealer'; $f.Width = 200; $f.Height = 100
            $f.StartPosition = 'Manual'; $f.Left = 0; $f.Top = 0; $f.Show(); $f.Activate()
            Start-Sleep -Milliseconds 800
            $f.Close(); $f.Dispose()
            [Win32]::SetForegroundWindow($hwnd) | Out-Null; Start-Sleep -Milliseconds 500
            Write-Host "focusloss done (fg back = $([Win32]::GetForegroundWindow() -eq $hwnd))"
        }
        default { throw "unknown step $($s.a)" }
    }
}
Write-Host "DRIVER_DONE"
