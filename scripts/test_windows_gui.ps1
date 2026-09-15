param(
    [string]$Executable = ".\artifacts\CPU_Scheduling_Simulator.exe",
    [string]$OutputDirectory = ".\artifacts"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$Executable = (Resolve-Path -LiteralPath $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$checks = [System.Collections.Generic.List[string]]::new()
$captures = [System.Collections.Generic.List[object]]::new()
$process = $null
$failure = $null

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $checks.Add($Message)
    Write-Host "PASS: $Message"
}

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

public static class GuiValidation {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr window, int x, int y, int width, int height, bool repaint);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window, out RECT rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window, ref POINT point);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool RedrawWindow(IntPtr window, IntPtr updateRect, IntPtr region, uint flags);
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr SendMessageTimeoutW(
        IntPtr window, uint message, IntPtr wParam, IntPtr lParam, uint flags, uint timeout, out UIntPtr result);

    public static long Send(IntPtr window, uint message, long wParam, IntPtr lParam) {
        UIntPtr result;
        if (SendMessageTimeoutW(window, message, new IntPtr(wParam), lParam, 3, 5000, out result) == IntPtr.Zero)
            throw new InvalidOperationException("A desktop control did not respond within five seconds.");
        return unchecked((long)result.ToUInt64());
    }

    public sealed class CaptureResult {
        public string File { get; set; }
        public string Method { get; set; }
        public string MainContentHash { get; set; }
        public int Width { get; set; }
        public int Height { get; set; }
        public bool HeaderRendered { get; set; }
    }

    private static bool HasHeader(Bitmap image, int clientX, int clientY) {
        int x = Math.Max(0, Math.Min(image.Width - 1, clientX + 10));
        int y = Math.Max(0, Math.Min(image.Height - 1, clientY + 40));
        Color color = image.GetPixel(x, y);
        return Math.Abs(color.R - 24) < 12 && Math.Abs(color.G - 44) < 12 && Math.Abs(color.B - 73) < 12;
    }

    public static CaptureResult Capture(IntPtr window, string file) {
        RECT bounds, client;
        if (!GetWindowRect(window, out bounds) || !GetClientRect(window, out client))
            throw new InvalidOperationException("Cannot read desktop window dimensions.");
        POINT clientOrigin = new POINT();
        ClientToScreen(window, ref clientOrigin);
        int offsetX = 0;
        int offsetY = 0;
        int width = client.Right;
        int height = client.Bottom;
        string method = "PrintWindow PW_CLIENTONLY | PW_RENDERFULLCONTENT";
        using (Bitmap image = new Bitmap(width, height, PixelFormat.Format24bppRgb)) {
            using (Graphics graphics = Graphics.FromImage(image)) {
                graphics.Clear(Color.Magenta);
                IntPtr dc = graphics.GetHdc();
                try { PrintWindow(window, dc, 3); }
                finally { graphics.ReleaseHdc(dc); }
            }
            bool header = HasHeader(image, offsetX, offsetY);
            if (!header) {
                method = "CopyFromScreen";
                using (Graphics graphics = Graphics.FromImage(image)) {
                    graphics.CopyFromScreen(clientOrigin.X, clientOrigin.Y, 0, 0, new Size(width, height));
                }
                header = HasHeader(image, offsetX, offsetY);
            }
            image.Save(file, ImageFormat.Png);
            Rectangle content = new Rectangle(offsetX + 24, offsetY + 205,
                Math.Max(1, client.Right - 48), Math.Min(380, client.Bottom - 205));
            string digest;
            using (Bitmap crop = image.Clone(content, PixelFormat.Format24bppRgb))
            using (MemoryStream bytes = new MemoryStream())
            using (SHA256 sha = SHA256.Create()) {
                crop.Save(bytes, ImageFormat.Png);
                digest = BitConverter.ToString(sha.ComputeHash(bytes.ToArray())).Replace("-", "");
            }
            return new CaptureResult { File = Path.GetFileName(file), Method = method,
                MainContentHash = digest, Width = width, Height = height, HeaderRendered = header };
        }
    }
}
'@

function Select-Combo([int]$Id, [int]$Index) {
    $control = [GuiValidation]::GetDlgItem($script:window, $Id)
    $selected = [GuiValidation]::Send($control, 0x014E, $Index, [IntPtr]::Zero)
    Assert-True ($selected -eq $Index) "Control $Id selected option $Index"
    [void][GuiValidation]::Send($script:window, 0x0111, ((1 -shl 16) -bor $Id), $control)
    [void][GuiValidation]::RedrawWindow($script:window, [IntPtr]::Zero, [IntPtr]::Zero, 0x0181)
    Start-Sleep -Milliseconds 150
}

function Click-Control([int]$Id) {
    $control = [GuiValidation]::GetDlgItem($script:window, $Id)
    [void][GuiValidation]::Send($control, 0x00F5, 0, [IntPtr]::Zero)
    [void][GuiValidation]::RedrawWindow($script:window, [IntPtr]::Zero, [IntPtr]::Zero, 0x0181)
    Start-Sleep -Milliseconds 100
}

function Save-Gui([string]$Name) {
    $capture = [GuiValidation]::Capture($script:window, (Join-Path $OutputDirectory $Name))
    $captures.Add($capture)
    Assert-True $capture.HeaderRendered "Rendered window captured in $Name ($($capture.Method))"
    return $capture
}

function Check-Console([string]$Name, [string[]]$Arguments, [double[]]$Waiting, [double[]]$Turnaround, [double]$Utilization) {
    $text = (& $Executable @Arguments | Out-String)
    $exitCode = $LASTEXITCODE
    $text | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $OutputDirectory ($Name + "_Output.txt"))
    Assert-True ($exitCode -eq 0) "$Name console execution exited successfully"
    $wt = [regex]::Matches($text, 'Average Waiting Time\s*:\s*([\d.]+)')
    $tat = [regex]::Matches($text, 'Average Turnaround Time\s*:\s*([\d.]+)')
    $cpu = [regex]::Matches($text, 'CPU Utilization\s*:\s*([\d.]+)%')
    Assert-True ($wt.Count -eq 4 -and $tat.Count -eq 4 -and $cpu.Count -eq 4) "$Name prints metrics for exactly four algorithms"
    for ($i = 0; $i -lt 4; $i++) {
        $culture = [Globalization.CultureInfo]::InvariantCulture
        Assert-True ([double]::Parse($wt[$i].Groups[1].Value, $culture) -eq $Waiting[$i]) "$Name algorithm $i waiting-time average"
        Assert-True ([double]::Parse($tat[$i].Groups[1].Value, $culture) -eq $Turnaround[$i]) "$Name algorithm $i turnaround-time average"
        Assert-True ([double]::Parse($cpu[$i].Groups[1].Value, $culture) -eq $Utilization) "$Name algorithm $i utilization"
    }
    Assert-True (([regex]::Matches($text, 'Execution order:')).Count -eq 4) "$Name execution orders present"
    Assert-True (([regex]::Matches($text, 'Stack history \(latest first\):')).Count -eq 4) "$Name stack histories present"
    if ($Name -eq "Assignment") {
        $rows = [regex]::Matches($text, '(?m)^P[1-3]\s+\d+\s+\d+\s+\d+\s+(\d+)\s+(\d+)\s+(\d+)\s*$')
        $expected = @('5,0,5','8,4,7','16,6,14','5,0,5','8,4,7','16,6,14','5,0,5','8,4,7','16,6,14','12,7,12','9,5,8','16,6,14')
        Assert-True ($rows.Count -eq 12) "Assignment prints all twelve per-process result rows"
        for ($i = 0; $i -lt 12; $i++) {
            $actual = @($rows[$i].Groups[1].Value, $rows[$i].Groups[2].Value, $rows[$i].Groups[3].Value) -join ','
            Assert-True ($actual -eq $expected[$i]) "Assignment per-process CT/WT/TAT row $i"
        }
    }
}

try {
    Check-Console "Assignment" @('--console') @(3.33,3.33,3.33,6.00) @(8.67,8.67,8.67,11.33) 100.00
    Check-Console "Comparison" @('--console','--compare') @(6.80,5.00,5.20,8.40) @(11.20,9.40,9.60,12.80) 91.67
    $process = Start-Process -FilePath $Executable -PassThru
    # This executable supports both console and GUI startup. Poll its actual
    # window instead of WaitForInputIdle, which rejects console-subsystem EXEs.
    $window = [IntPtr]::Zero
    for ($attempt = 0; $attempt -lt 100; $attempt++) {
        $process.Refresh()
        if ($process.HasExited) { throw "GUI exited before a window appeared: $($process.ExitCode)" }
        $window = $process.MainWindowHandle
        if ($window -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 100
    }
    Assert-True ($window -ne [IntPtr]::Zero) "Default Windows launch opens the desktop GUI"
    Assert-True ([GuiValidation]::IsWindowVisible($window)) "Desktop window is visible"
    [void][GuiValidation]::ShowWindow($window, 9)
    [void][GuiValidation]::SetForegroundWindow($window)
    [void][GuiValidation]::MoveWindow($window, 20, 20, 1120, 730, $true)
    Start-Sleep -Milliseconds 500
    foreach ($id in 101..106) {
        Assert-True ([GuiValidation]::GetDlgItem($window, $id) -ne [IntPtr]::Zero) "Required GUI control $id exists"
    }
    foreach ($entry in @(@(101,2),@(102,4),@(103,5))) {
        $count = [GuiValidation]::Send([GuiValidation]::GetDlgItem($window, $entry[0]), 0x0146, 0, [IntPtr]::Zero)
        Assert-True ($count -eq $entry[1]) "Control $($entry[0]) exposes $count options"
    }
    Assert-True ([GuiValidation]::Send([GuiValidation]::GetDlgItem($window, 101), 0x0147, 0, [IntPtr]::Zero) -eq 0) "Assignment sample is the default"
    Assert-True ([GuiValidation]::Send([GuiValidation]::GetDlgItem($window, 103), 0x0147, 0, [IntPtr]::Zero) -eq 1) "Default Round Robin quantum is two"
    $assignment = Save-Gui "GUI_Assignment.png"
    Select-Combo 101 1
    $comparison = Save-Gui "GUI_Comparison.png"
    Assert-True ($comparison.MainContentHash -ne $assignment.MainContentHash) "Changing the sample repaints the result tables and Gantt chart"
    Select-Combo 102 3
    $roundRobin = Save-Gui "GUI_Round_Robin.png"
    Assert-True ($roundRobin.MainContentHash -ne $comparison.MainContentHash) "Selecting Round Robin repaints the comparison results"
    Select-Combo 103 0
    $quantumOne = Save-Gui "GUI_Quantum_1.png"
    Assert-True ($quantumOne.MainContentHash -ne $roundRobin.MainContentHash) "Changing RR quantum recalculates the displayed results"
    Select-Combo 103 1
    $nextButton = [GuiValidation]::GetDlgItem($window, 105)
    Assert-True ([GuiValidation]::IsWindowEnabled($nextButton)) "Replay begins with Next step enabled"
    for ($step = 1; $step -le 13; $step++) {
        Click-Control 105
        if ($step -eq 2) { [void](Save-Gui "GUI_Replay.png") }
        if ($step -eq 12 -or $step -eq 13) {
            Assert-True ([GuiValidation]::IsWindowEnabled($nextButton) -eq ($step -lt 13)) "RR replay step $step has the correct completion state"
        }
    }
    Click-Control 106
    Assert-True ([GuiValidation]::IsWindowEnabled($nextButton)) "Reset replay restores Next step"
    Click-Control 105
    Click-Control 104
    Assert-True ([GuiValidation]::IsWindowEnabled($nextButton)) "Run all recomputes and resets replay"
    [void][GuiValidation]::MoveWindow($window, 0, 0, 990, 720, $true)
    Start-Sleep -Milliseconds 250
    $resized = Save-Gui "GUI_Resized.png"
    Assert-True ($resized.Width -ge 960 -and $resized.Height -ge 680 -and $resized.Width -lt $assignment.Width) "GUI repaints at a smaller supported window size"
    $process.Refresh()
    Assert-True (-not $process.HasExited) "GUI remains running after every control test"
}
catch {
    $failure = $_.Exception.Message
    Write-Warning $failure
}
finally {
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            [void]$process.CloseMainWindow()
            if (-not $process.WaitForExit(5000)) { $process.Kill() }
        }
    }
    [ordered]@{
        status = $(if ($null -eq $failure) { 'passed' } else { 'failed' })
        executable = [IO.Path]::GetFileName($Executable)
        completedUtc = [DateTime]::UtcNow.ToString('o')
        checks = @($checks.ToArray())
        captures = @($captures.ToArray())
        error = $failure
    } | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $OutputDirectory 'Windows_GUI_Test_Results.json')
}
if ($null -ne $failure) { throw $failure }
Write-Host "Windows GUI validation passed: $($checks.Count) checks and $($captures.Count) screenshots."
