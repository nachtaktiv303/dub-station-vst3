#!/usr/bin/env bash
# Builds a macOS installer that puts the VST3 and the AU into /Library/Audio/Plug-Ins.
# Files installed by a .pkg aren't quarantined, so the DAW loads them without further steps.
#   make_pkg.sh <version> <artefacts dir (contains VST3/ and AU/)> <output dir>
set -euo pipefail
version="$1"; art="$2"; out="$3"
work="$(mktemp -d)"
mkdir -p "$out" "$work/vst3" "$work/au"
cp -R "$art/VST3/Dub Station.vst3" "$work/vst3/"
cp -R "$art/AU/Dub Station.component" "$work/au/"

component_pkg() # name, root, install-location
{
    pkgbuild --analyze --root "$2" "$work/$1.plist"
    # always install to the standard folder, never "relocate" onto an older copy somewhere else
    local pb=/usr/libexec/PlistBuddy i=0
    while "$pb" -c "Print :$i" "$work/$1.plist" > /dev/null 2>&1; do
        "$pb" -c "Set :$i:BundleIsRelocatable false" "$work/$1.plist" 2> /dev/null \
            || "$pb" -c "Add :$i:BundleIsRelocatable bool false" "$work/$1.plist"
        i=$((i + 1))
    done
    pkgbuild --root "$2" --component-plist "$work/$1.plist" --install-location "$3" \
        --identifier "ch.nachtaktiv.dubstation.$1" --version "$version" "$work/$1.pkg"
}
component_pkg vst3 "$work/vst3" "/Library/Audio/Plug-Ins/VST3"
component_pkg au "$work/au" "/Library/Audio/Plug-Ins/Components"

productbuild --package "$work/vst3.pkg" --package "$work/au.pkg" "$out/DubStation-$version-macOS.pkg"
echo "built $out/DubStation-$version-macOS.pkg"
