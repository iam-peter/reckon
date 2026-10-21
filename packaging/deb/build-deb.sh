#!/bin/sh
# Builds the Debian package from the AppDir linuxdeploy fills for the
# AppImage. Only Qt and its ICU come along, in /opt/reckon: distributions
# ship a Qt too old for Reckon. Every other library comes from the system
# and becomes a dependency, which dpkg-shlibdeps works out, so this has to
# run on the oldest system the package is for.
#
# Usage: build-deb.sh <AppDir> <version> <output.deb>
set -eu

if [ $# -ne 3 ]; then
    echo "Usage: $0 <AppDir> <version> <output.deb>" >&2
    exit 2
fi
appdir=$1
version=$2
output=$3
logo=$(cd "$(dirname "$0")/../../src/logo" && pwd)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
# Under debian/ dpkg-shlibdeps takes the libraries in it as the package's own
root=$work/debian/reckon
prefix=$root/opt/reckon

mkdir -p "$prefix/lib" "$root/usr/bin" "$root/DEBIAN"
cp -a "$appdir/usr/bin" "$appdir/usr/plugins" "$appdir/usr/qml" "$appdir/usr/translations" "$prefix/"
cp -a "$appdir"/usr/lib/libQt6* "$appdir"/usr/lib/libicu* "$prefix/lib/"
ln -s /opt/reckon/bin/reckon "$root/usr/bin/reckon"
install -Dm644 "$logo/reckon.desktop" "$root/usr/share/applications/reckon.desktop"
install -Dm644 "$logo/reckon.svg" "$root/usr/share/icons/hicolor/scalable/apps/reckon.svg"
chmod -R u+rwX,go+rX,go-w "$root"

printf 'Source: reckon\n\nPackage: reckon\nArchitecture: amd64\n' > "$work/debian/control"
libs=$(cd "$work" && find debian/reckon/opt -type f \( -name '*.so*' -o -path '*/bin/reckon' \) \
    -exec dpkg-shlibdeps -O -l"$prefix/lib" {} + | sed -n 's/^shlibs:Depends=//p')

cat > "$root/DEBIAN/control" <<EOF
Package: reckon
Version: $version
Architecture: amd64
Maintainer: Henning Gruendl <henning.gruendl@qt.io>
Installed-Size: $(du -sk "$root" | cut -f1)
Depends: $libs
Section: math
Priority: optional
Homepage: https://github.com/iam-peter/reckon
Description: Calculator with a history, graphs and scripts
 Reckon evaluates expressions into a history like SpeedCrunch, with
 variables, user-defined functions, number formats and precision. It
 plots y = f(x) curves and runs JavaScript against the calculator.
EOF

dpkg-deb --root-owner-group -Zxz -b "$root" "$output"
