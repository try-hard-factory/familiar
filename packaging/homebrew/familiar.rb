# This file belongs in a separate tap repo (e.g.
# try-hard-factory/homebrew-familiar, Casks/familiar.rb) - kept here as
# a draft/reference only. Users would install via:
#   brew tap try-hard-factory/familiar
#   brew install --cask familiar
#
# arm64 only - x86_64/macos-13 was dropped from the release matrix
# entirely (see MacOS-pack.yml), so there's no Intel .dmg to point at.

cask "familiar" do
  version "0.0.16"
  # PLACEHOLDER - not a real hash. Fill in with the actual sha256 from
  # the release's own familiar-<version>-osx-arm64.dmg.sha256sum.
  sha256 "REPLACE_WITH_REAL_SHA256_FROM_RELEASE"

  url "https://github.com/try-hard-factory/familiar/releases/download/v#{version}/familiar-#{version}-osx-arm64.dmg"
  name "Familiar"
  desc "Reference board for 2D/3D artists"
  homepage "https://github.com/try-hard-factory/familiar"

  depends_on arch: :arm64
  depends_on macos: ">= :high_sierra"

  # Matches the actual bundle name - no CFBundleName override in
  # packaging/macos/Info.plist, so it's just the CMake target name.
  app "familiar.app"

  zap trash: [
    "~/Library/Application Support/familiar",
    "~/Library/Preferences/org.tryhardfactory.Familiar.plist",
    "~/Library/Saved Application State/org.tryhardfactory.Familiar.savedState",
  ]
end
