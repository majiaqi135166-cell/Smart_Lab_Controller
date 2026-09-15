# ═══════════════════════════════════════════════════════
# com0com 一键修复脚本（右键"以管理员身份运行"）
# ═══════════════════════════════════════════════════════
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  com0com 虚拟串口修复工具" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# 0. 检查管理员权限
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[错误] 请右键此脚本 → 以管理员身份运行" -ForegroundColor Red
    Read-Host "按回车退出"
    exit 1
}

# 1. 开启测试签名
Write-Host "[1/3] 开启测试签名模式..." -ForegroundColor Yellow
$result = bcdedit /set testsigning on 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "  [OK] testsigning 已开启" -ForegroundColor Green
} else {
    Write-Host "  [失败] $result" -ForegroundColor Red
    Write-Host "  可能原因：Secure Boot 未关闭（进 BIOS 关闭后重试）" -ForegroundColor Red
}

# 2. 清理幽灵设备（CM_PROB_PHANTOM）
Write-Host ""
Write-Host "[2/3] 清理幽灵设备..." -ForegroundColor Yellow
$phantomPorts = Get-PnpDevice -Class Ports -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -like "COM0COM*" -and $_.Status -eq "Unknown" }

if ($phantomPorts) {
    foreach ($p in $phantomPorts) {
        Write-Host "  移除幽灵设备: $($p.FriendlyName) ($($p.InstanceId))"
        # 从注册表删除设备节点
        $instanceId = $p.InstanceId -replace "\\", "\"
        $enumKey = "HKLM:\SYSTEM\CurrentControlSet\Enum\$instanceId"
        if (Test-Path $enumKey) {
            Remove-Item -Path $enumKey -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "    已删除注册表节点"
        }
    }
} else {
    Write-Host "  没有发现幽灵设备（或已清理）"
}

# 3. 重启 com0com 总线
Write-Host ""
Write-Host "[3/3] 重启 com0com 总线驱动..." -ForegroundColor Yellow
$setupc = "D:\Qt\Tools\com0com\setupc.exe"
if (Test-Path $setupc) {
    Write-Host "  移除旧端口对..."
    & $setupc remove 1 2>$null
    Write-Host "  重新创建端口对 COM12 <-> COM13..."
    & $setupc install 1 PortName=COM12 PortName=COM13 2>&1
} else {
    Write-Host "  [警告] 找不到 $setupc" -ForegroundColor Yellow
    Write-Host "  请手动在设备管理器中：查看→显示隐藏设备→删除 CNCA1/CNCB1→扫描硬件改动"
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  修复完成！请重启电脑。" -ForegroundColor Green
Write-Host "  重启后桌面右下角应出现'测试模式'水印，" -ForegroundColor Green
Write-Host "  设备管理器中出现 COM12 和 COM13。" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
Read-Host "按回车退出"
