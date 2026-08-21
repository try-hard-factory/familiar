$ErrorActionPreference = 'Stop'

$packageName  = 'familiar'
$softwareName = 'Familiar*'

[array]$key = Get-UninstallRegistryKey -SoftwareName $softwareName

if ($key.Count -eq 1) {
  $key | ForEach-Object {
    $silentArgs = '/qn /norestart'
    Uninstall-ChocolateyPackage -PackageName $packageName `
      -FileType 'msi' `
      -SilentArgs "$($_.PSChildName) $silentArgs" `
      -ValidExitCodes @(0, 3010, 1605, 1614, 1641)
  }
} elseif ($key.Count -eq 0) {
  Write-Warning "$packageName has already been uninstalled."
} elseif ($key.Count -gt 1) {
  Write-Warning "$($key.Count) matches found for '$softwareName' - manual uninstall may be needed."
}
