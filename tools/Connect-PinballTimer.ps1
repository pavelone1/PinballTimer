<#
.SYNOPSIS
Discovers or joins a PinballTimerXXXX WiFi network and verifies its web API.

.EXAMPLE
.\tools\Connect-PinballTimer.ps1 -SecretsPath .\include\Secrets.h

.EXAMPLE
.\tools\Connect-PinballTimer.ps1 -Ssid PinballTimerF928

.EXAMPLE
.\tools\Connect-PinballTimer.ps1 -NoJoin -Address pinballtimer.local

.NOTES
The SecretsPath option reads WIFI_PORTAL_PASSWORD without printing it. Without
that option, the script prompts for a SecureString. The connector creates its
own Windows WLAN profile so it does not overwrite an existing profile with the
same name. Use -ForgetProfile to remove the connector profile after testing.
#>
[CmdletBinding()]
param(
    [Parameter()]
    [ValidatePattern('^PinballTimer[0-9A-Fa-f]{4}$')]
    [string]$Ssid,

    [Parameter()]
    [SecureString]$Password,

    [Parameter()]
    [string]$SecretsPath,

    [Parameter()]
    [string]$Address = '10.10.10.1',

    [Parameter()]
    [string]$InterfaceName,

    [Parameter()]
    [ValidateRange(5, 120)]
    [int]$TimeoutSeconds = 30,

    [Parameter()]
    [switch]$NoJoin,

    [Parameter()]
    [switch]$ForgetProfile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-Netsh {
    param([Parameter(Mandatory)][string[]]$Arguments)

    $output = & netsh.exe @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "netsh failed ($LASTEXITCODE): $($output -join [Environment]::NewLine)"
    }
    return $output
}

function Find-PinballTimerSsid {
    $networks = Invoke-Netsh -Arguments @('wlan', 'show', 'networks', 'mode=bssid')
    $matches = @(
        $networks |
            ForEach-Object {
                if ($_ -match '^\s*SSID\s+\d+\s*:\s*(PinballTimer[0-9A-Fa-f]{4})\s*$') {
                    $Matches[1]
                }
            } |
            Sort-Object -Unique
    )

    if ($matches.Count -eq 0) {
        throw 'No PinballTimerXXXX access point is visible. Turn WiFi ON on the timer and try again.'
    }
    if ($matches.Count -gt 1) {
        throw "Multiple timers are visible ($($matches -join ', ')). Supply -Ssid explicitly."
    }
    return $matches[0]
}

function Read-PortalPasswordFromSecrets {
    param([Parameter(Mandatory)][string]$Path)

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $line = Get-Content -LiteralPath $resolved |
        Where-Object { $_ -match '^\s*#define\s+WIFI_PORTAL_PASSWORD\s+"(.*)"\s*$' } |
        Select-Object -First 1
    if (-not $line -or $line -notmatch '^\s*#define\s+WIFI_PORTAL_PASSWORD\s+"(.*)"\s*$') {
        throw "WIFI_PORTAL_PASSWORD was not found in $resolved"
    }

    return ConvertTo-SecureString -String $Matches[1] -AsPlainText -Force
}

function ConvertFrom-SecureStringTransient {
    param([Parameter(Mandatory)][SecureString]$Value)

    $pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Value)
    try {
        return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer)
    }
    finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer)
    }
}

function Add-PinballTimerProfile {
    param(
        [Parameter(Mandatory)][string]$NetworkName,
        [Parameter(Mandatory)][SecureString]$NetworkPassword
    )

    $plainPassword = ConvertFrom-SecureStringTransient -Value $NetworkPassword
    $escapedName = [Security.SecurityElement]::Escape($NetworkName)
    $profileName = "PinballTimerConnector-$NetworkName"
    $escapedProfileName = [Security.SecurityElement]::Escape($profileName)
    $escapedPassword = [Security.SecurityElement]::Escape($plainPassword)
    $profilePath = Join-Path ([IO.Path]::GetTempPath()) ("PinballTimer-{0}.xml" -f [Guid]::NewGuid())

    try {
        $profileXml = @"
<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
  <name>$escapedProfileName</name>
  <SSIDConfig><SSID><name>$escapedName</name></SSID></SSIDConfig>
  <connectionType>ESS</connectionType>
  <connectionMode>manual</connectionMode>
  <MSM><security>
    <authEncryption>
      <authentication>WPA2PSK</authentication>
      <encryption>AES</encryption>
      <useOneX>false</useOneX>
    </authEncryption>
    <sharedKey>
      <keyType>passPhrase</keyType>
      <protected>false</protected>
      <keyMaterial>$escapedPassword</keyMaterial>
    </sharedKey>
  </security></MSM>
</WLANProfile>
"@
        [IO.File]::WriteAllText($profilePath, $profileXml, [Text.UTF8Encoding]::new($false))
        $arguments = @('wlan', 'add', 'profile', "filename=$profilePath", 'user=current')
        if ($InterfaceName) {
            $arguments += "interface=$InterfaceName"
        }
        Invoke-Netsh -Arguments $arguments | Out-Null
        return $profileName
    }
    finally {
        $plainPassword = $null
        if (Test-Path -LiteralPath $profilePath) {
            Remove-Item -LiteralPath $profilePath -Force
        }
    }
}

function Connect-Wlan {
    param(
        [Parameter(Mandatory)][string]$ProfileName,
        [Parameter(Mandatory)][string]$NetworkName
    )

    $arguments = @('wlan', 'connect', "name=$ProfileName", "ssid=$NetworkName")
    if ($InterfaceName) {
        $arguments += "interface=$InterfaceName"
    }
    Invoke-Netsh -Arguments $arguments | Out-Null
}

function Test-PinballTimerHttp {
    param([Parameter(Mandatory)][string]$HostAddress)

    $baseUri = if ($HostAddress -match '^https?://') {
        $HostAddress.TrimEnd('/')
    } else {
        "http://$($HostAddress.TrimEnd('/'))"
    }

    $status = Invoke-RestMethod -Uri "$baseUri/status" -Method Get -TimeoutSec 5
    $machines = Invoke-RestMethod -Uri "$baseUri/api/machines" -Method Get -TimeoutSec 5

    [PSCustomObject]@{
        Connected = $true
        BaseUri = $baseUri
        Device = $status.device
        Mode = $status.modeName
        MachineCount = $machines.count
        LivePage = "$baseUri/game-live"
        SetupPage = "$baseUri/game-setup"
        DatabasePage = "$baseUri/machines"
    }
}

$connectorProfileName = $null
if (-not $NoJoin) {
    if (-not $Ssid) {
        $Ssid = Find-PinballTimerSsid
    }

    if (-not $Password -and $SecretsPath) {
        $Password = Read-PortalPasswordFromSecrets -Path $SecretsPath
    }
    if (-not $Password) {
        $Password = Read-Host "Password for $Ssid" -AsSecureString
    }

    Write-Host "Adding connector profile for $Ssid..."
    $connectorProfileName = Add-PinballTimerProfile -NetworkName $Ssid -NetworkPassword $Password
    Write-Host "Connecting to $Ssid..."
    Connect-Wlan -ProfileName $connectorProfileName -NetworkName $Ssid
}

$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$lastError = $null
do {
    try {
        $result = Test-PinballTimerHttp -HostAddress $Address
        $result
        $lastError = $null
        break
    }
    catch {
        $lastError = $_
        Start-Sleep -Milliseconds 750
    }
} while ([DateTime]::UtcNow -lt $deadline)

if ($lastError) {
    throw "Connected attempt timed out; the timer did not answer at $Address. Last error: $($lastError.Exception.Message)"
}

if ($ForgetProfile -and $connectorProfileName) {
    Invoke-Netsh -Arguments @('wlan', 'delete', 'profile', "name=$connectorProfileName") | Out-Null
    Write-Host "Removed WLAN profile $connectorProfileName."
}
