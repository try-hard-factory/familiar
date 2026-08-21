$ErrorActionPreference = 'Stop'

$packageName = 'familiar'
$version     = '0.0.16'
$url64       = "https://github.com/try-hard-factory/familiar/releases/download/v$version/Familiar-$version-win64.msi"

$packageArgs = @{
  packageName    = $packageName
  fileType       = 'msi'
  url64bit       = $url64
  softwareName   = 'Familiar*'
  # PLACEHOLDER - not a real hash. Fill in with the actual sha256 from
  # the release's own familiar-<version>-win64.msi.sha256sum before
  # ever running `choco push`. Chocolatey's own moderation re-verifies
  # this against the real download anyway, but don't ship a fake one.
  checksum64     = 'REPLACE_WITH_REAL_SHA256_FROM_RELEASE'
  checksumType64 = 'sha256'
  silentArgs     = '/qn /norestart'
  validExitCodes = @(0, 3010, 1641)
}

Install-ChocolateyPackage @packageArgs
