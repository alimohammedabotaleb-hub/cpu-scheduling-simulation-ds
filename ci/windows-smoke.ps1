$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class NativeGui {
    public delegate bool EnumProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr parameter);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")]
    public static extern IntPtr ReadMessage(IntPtr window, uint message, IntPtr wParam, StringBuilder text);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rectangle);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr window, int x, int y, int width, int height, bool repaint);
    [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr dc, uint flags);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
}
'@

$OutputFolder = Join-Path $PSScriptRoot '..\artifacts'
New-Item -ItemType Directory -Force $OutputFolder | Out-Null
$ProgramFile = (Resolve-Path (Join-Path $PSScriptRoot '..\build\CPU_Scheduling_Simulator.exe')).Path
$GuiProcess = Start-Process -FilePath $ProgramFile -PassThru
$script:TargetWindow = [IntPtr]::Zero
$TargetProcessId = [uint32]$GuiProcess.Id

function Read-Results {
    $Control = [NativeGui]::GetDlgItem($script:TargetWindow, 106)
    if ($Control -eq [IntPtr]::Zero) { throw 'Results EDIT control 106 was not created.' }
    $Text = [System.Text.StringBuilder]::new(32768)
    [void][NativeGui]::ReadMessage($Control, 13, [IntPtr]32768, $Text)
    return $Text.ToString()
}

function Click-Control([int]$ControlId) {
    $Control = [NativeGui]::GetDlgItem($script:TargetWindow, $ControlId)
    if ($Control -eq [IntPtr]::Zero) { throw "Missing button $ControlId" }
    # BM_CLICK generates the ordinary native button/checkbox notification.
    [void][NativeGui]::SendMessage($Control, 245, [IntPtr]::Zero, [IntPtr]::Zero)
    [void][NativeGui]::UpdateWindow($script:TargetWindow)
    Start-Sleep -Milliseconds 150
}

function Assert-Row([string]$Id, [int[]]$Values) {
    $Pattern = '(?m)^' + [regex]::Escape($Id) + '\s+' + (($Values | ForEach-Object { [string]$_ }) -join '\s+') + '\r?$'
    if ((Read-Results) -notmatch $Pattern) {
        throw "Wrong process row $Id; expected $Values`n$(Read-Results)"
    }
}

function Save-Window([string]$Name) {
    $Rectangle = New-Object NativeGui+Rect
    [void][NativeGui]::GetWindowRect($script:TargetWindow, [ref]$Rectangle)
    $Bitmap = [Drawing.Bitmap]::new($Rectangle.Right - $Rectangle.Left, $Rectangle.Bottom - $Rectangle.Top)
    $Graphics = [Drawing.Graphics]::FromImage($Bitmap)
    $Context = $Graphics.GetHdc()
    try {
        # WM_PRINT paints native child controls as well as the client drawing.
        if (-not [NativeGui]::PrintWindow($script:TargetWindow, $Context, 2)) {
            [void][NativeGui]::SendMessage($script:TargetWindow, 791, $Context, [IntPtr]30)
        }
    } finally {
        $Graphics.ReleaseHdc($Context)
        $Graphics.Dispose()
    }
    $Bitmap.Save((Join-Path $OutputFolder "$Name.png"), [Drawing.Imaging.ImageFormat]::Png)
    $Bitmap.Dispose()
    Read-Results | Set-Content -Encoding utf8 (Join-Path $OutputFolder "$Name.txt")
}

function Assert-Text([string]$Expected) {
    if (-not (Read-Results).Contains($Expected)) {
        throw "Expected result '$Expected' not found in native EDIT control:`n$(Read-Results)"
    }
}

try {
    for ($Attempt = 0; $Attempt -lt 50 -and $script:TargetWindow -eq [IntPtr]::Zero; ++$Attempt) {
        [void][NativeGui]::EnumWindows({
            param($Window, $Parameter)
            $WindowProcess = [uint32]0
            [void][NativeGui]::GetWindowThreadProcessId($Window, [ref]$WindowProcess)
            if ($WindowProcess -eq $TargetProcessId -and [NativeGui]::IsWindowVisible($Window)) {
                $script:TargetWindow = $Window
                return $false
            }
            return $true
        }, [IntPtr]::Zero)
        if ($script:TargetWindow -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 200 }
    }
    if ($script:TargetWindow -eq [IntPtr]::Zero) { throw 'The native GUI did not create a visible window.' }

    Assert-Text 'Average WT: 3.33   Average TAT: 8.67'
    Assert-Row 'P1' @(0, 5, 2, 5, 0, 5)
    Assert-Row 'P2' @(1, 3, 1, 8, 4, 7)
    Assert-Row 'P3' @(2, 8, 3, 16, 6, 14)
    Save-Window 'assignment-fcfs'

    Click-Control 104
    Assert-Text 'Round Robin  |  RR quantum = 2'
    Assert-Text 'Average WT: 6.00   Average TAT: 11.33'
    Assert-Row 'P1' @(0, 5, 2, 12, 7, 12)
    Assert-Row 'P2' @(1, 3, 1, 9, 5, 8)
    Assert-Row 'P3' @(2, 8, 3, 16, 6, 14)
    Save-Window 'assignment-rr'

    Click-Control 105
    Assert-Text 'Average WT: 8.40   Average TAT: 12.80'
    Assert-Text 'CPU utilization: 91.67%'
    Assert-Row 'P1' @(2, 5, 3, 17, 10, 15)
    Assert-Row 'P2' @(3, 2, 1, 6, 1, 3)
    Assert-Row 'P3' @(4, 8, 4, 24, 12, 20)
    Assert-Row 'P4' @(5, 3, 2, 18, 10, 13)
    Assert-Row 'P5' @(7, 4, 1, 20, 9, 13)
    Save-Window 'comparison-rr'

    Click-Control 102
    Assert-Text 'SJF  |  RR quantum = 2'
    Assert-Text 'Average WT: 5.00   Average TAT: 9.40'
    Assert-Row 'P3' @(4, 8, 4, 24, 12, 20)
    Assert-Row 'P4' @(5, 3, 2, 12, 4, 7)
    Assert-Row 'P5' @(7, 4, 1, 16, 5, 9)
    Save-Window 'comparison-sjf'

    Click-Control 103
    Assert-Text 'Priority  |  RR quantum = 2'
    Assert-Text 'Average WT: 5.20   Average TAT: 9.60'
    Assert-Row 'P4' @(5, 3, 2, 16, 8, 11)
    Assert-Row 'P5' @(7, 4, 1, 13, 2, 6)
    Save-Window 'comparison-priority'

    Click-Control 101
    Assert-Text 'FCFS  |  RR quantum = 2'
    Assert-Text 'Average WT: 6.80   Average TAT: 11.20'
    Assert-Row 'P3' @(4, 8, 4, 17, 5, 13)
    Assert-Row 'P4' @(5, 3, 2, 20, 12, 15)
    Assert-Row 'P5' @(7, 4, 1, 24, 13, 17)
    Save-Window 'comparison-fcfs'

    [void][NativeGui]::MoveWindow($script:TargetWindow, 0, 0, 850, 600, $true)
    [void][NativeGui]::UpdateWindow($script:TargetWindow)
    Save-Window 'resized-window'

    Click-Control 105
    Assert-Text 'Average WT: 3.33   Average TAT: 8.67'
    Assert-Row 'P2' @(1, 3, 1, 8, 4, 7)
    'PASS: startup, both samples, all four algorithm buttons, CT/WT/TAT, means, utilization and resize.' |
        Tee-Object -FilePath (Join-Path $OutputFolder 'gui-smoke-result.txt')
} finally {
    if ($script:TargetWindow -ne [IntPtr]::Zero) {
        [void][NativeGui]::SendMessage($script:TargetWindow, 16, [IntPtr]::Zero, [IntPtr]::Zero)
    }
    if (-not $GuiProcess.WaitForExit(5000)) { $GuiProcess.Kill() }
}
