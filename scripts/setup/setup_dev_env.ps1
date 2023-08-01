$ErrorActionPreference = "Stop"
Set-ExecutionPolicy Bypass -Scope Process -Force;

##  Install Chocolatey
Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

Write-Host "=== Creating your XStudio development environment! ==="

Write-Host "====> Installing Choco packages..."
choco --version
choco feature enable -name=exitOnRebootDetected
choco install ChocolateyGUI -y

# core components
Write-Host "====> Installing core components..."
choco install powershell-core -y
choco install git -y
choco install cmake -y
choco install ninja -y
choco install doxygen.install -y

# ides
Write-Host "====> Installing IDEs..."
#choco install visualstudio2019buildtools -y
#choco install visualstudio2019community -y --package-parameters "--includeRecommended --locale en-US --passive --add Microsoft.VisualStudio.Component.CoreEditor --add Microsoft.VisualStudio.Workload.NetWeb --add Microsoft.VisualStudio.Workload.Azure --add Microsoft.VisualStudio.Workload.NetCoreTools --add Microsoft.VisualStudio.Workload.Python --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Workload.NativeGame --add Microsoft.VisualStudio.Workload.NativeCrossPlat --add Component.GitHub.VisualStudio --add Component.Incredibuild --add Microsoft.VisualStudio.Workload.VisualStudioExtension --add Microsoft.VisualStudio.Workload.ManagedDesktop --add Microsoft.VisualStudio.Workload.Universal"
#choco install visualstudio2019professional -y --package-parameters "--includeRecommended --locale en-US --passive --add Microsoft.VisualStudio.Component.CoreEditor --add Microsoft.VisualStudio.Workload.NetWeb --add Microsoft.VisualStudio.Workload.Azure --add Microsoft.VisualStudio.Workload.NetCoreTools --add Microsoft.VisualStudio.Workload.Python --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Workload.NativeGame --add Microsoft.VisualStudio.Workload.NativeCrossPlat --add Component.GitHub.VisualStudio --add Microsoft.VisualStudio.Workload.VisualStudioExtension --add Microsoft.VisualStudio.Workload.ManagedDesktop --add Microsoft.VisualStudio.Workload.Universal"

refreshenv

Write-Host "=== Your XStudio development environment is ready to use! Enjoy! ==="
