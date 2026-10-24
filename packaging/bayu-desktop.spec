# Builds the whole Bayu monorepo. Sources come from packaging/rpmbuild.sh:
# Source0 is `git archive` of the repository, Source1 the pinned Skia checkout
# that bayu-toolkit builds (components/bayu-toolkit/scripts/fetch-skia.sh).
%global debug_package %{nil}
%global __brp_mangle_shebangs %{nil}

Name:           bayu-desktop
Version:        %{?bayu_version}%{!?bayu_version:0.1.0}
Release:        %{?bayu_release}%{!?bayu_release:1}%{?dist}
Summary:        Bayu desktop environment
License:        BSD-3-Clause
URL:            https://github.com/igos-nusantara/bayu
Source0:        bayu-%{version}.tar.gz
Source1:        skia.tar.gz

BuildRequires:  git cmake ninja-build pkgconf clang gcc-c++ python3 gettext
BuildRequires:  gtk-update-icon-cache libxcrypt-devel libzip-devel pam-devel
BuildRequires:  pkgconfig(wayland-client) pkgconfig(wayland-server) pkgconfig(wayland-egl)
BuildRequires:  pkgconfig(wayland-scanner) pkgconfig(wayland-protocols) pkgconfig(egl) pkgconfig(glesv2)
BuildRequires:  pkgconfig(xkbcommon) pkgconfig(libsystemd) pkgconfig(fontconfig) pkgconfig(freetype2)
BuildRequires:  pkgconfig(harfbuzz) pkgconfig(icu-uc) pkgconfig(icu-i18n) pkgconfig(expat)
BuildRequires:  pkgconfig(libpng) pkgconfig(libjpeg) pkgconfig(zlib) pkgconfig(libwebp)
BuildRequires:  pkgconfig(libtiff-4) pkgconfig(libheif) pkgconfig(libjxl) pkgconfig(libjxl_threads)
BuildRequires:  pkgconfig(libpipewire-0.3) pkgconfig(libspa-0.2) pkgconfig(gbm) pkgconfig(libdrm)
BuildRequires:  pkgconfig(libavcodec) pkgconfig(libavformat) pkgconfig(libavutil) pkgconfig(libswscale)
BuildRequires:  pkgconfig(libva) pkgconfig(libinput) pkgconfig(libseat) pkgconfig(libudev)
BuildRequires:  pkgconfig(xcb) pkgconfig(xcb-composite) pkgconfig(xcb-xfixes) pkgconfig(xcb-shape)
BuildRequires:  pkgconfig(xcb-sync) pkgconfig(xcursor) pkgconfig(libpcre2-16) pkgconfig(libpcre2-8)
BuildRequires:  pkgconfig(libarchive) pkgconfig(fuse3) pkgconfig(libmtp) pkgconfig(smbclient)
BuildRequires:  pkgconfig(libssh) pkgconfig(libcurl) pkgconfig(libical) pkgconfig(librsvg-2.0)
BuildRequires:  qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtdeclarative-devel

# Runtime services, from upstream's getting-started step 4.
Requires:       accountsservice polkit gsettings-desktop-schemas xdg-user-dirs
Requires:       NetworkManager bluez upower (power-profiles-daemon or tuned-ppd) udisks2 fuse3
Requires:       pipewire pipewire-pulseaudio wireplumber gnome-keyring gnome-keyring-pam
Requires:       xdg-desktop-portal xorg-x11-server-Xwayland adwaita-icon-theme
Requires:       mesa-dri-drivers mesa-libEGL mesa-libgbm glibc-langpack-en
# dbus-update-activation-environment: the session hands its environment
# (WAYLAND_DISPLAY, XDG_CURRENT_DESKTOP) to D-Bus and systemd services.
Requires:       dbus-tools
# Default UI and fixed-width fonts.
Requires:       google-noto-sans-vf-fonts google-noto-sans-mono-vf-fonts
Requires(pre):  shadow-utils

%package -n nabau
Summary:        Installs the system from Bayu live media
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       anaconda-live anaconda-core polkit redhat-mono-fonts rsync kbd chrony
Requires:       libblockdev-lvm libblockdev-dm libblockdev-mpath libblockdev-mdraid lvm2
Requires:       dosfstools e2fsprogs xfsprogs efibootmgr cryptsetup
Recommends:     ntfsprogs

%description -n nabau
Nabau, the installer of Bayu live media: a native frontend for Anaconda's
installation modules. It copies the live system to a disk, beside Windows
or alone, and removes itself from the installed system.

%description
Bayu is a Wayland desktop environment forked from Cutefish
(https://github.com/cutefishos): compositor, shell, session, display
manager, file manager, settings, terminal and the apps around them.

%prep
%autosetup -n bayu-%{version}
mkdir -p components/bayu-toolkit/third_party
tar -xzf %{SOURCE1} -C components/bayu-toolkit/third_party

%build
BUILD=$PWD/_build STAGE=$PWD/_stage ./build.sh

%install
cp -a _stage/. %{buildroot}/
install -m 644 packaging/pam/bayu-dm packaging/pam/bayu-dm-greeter packaging/pam/bayu-auth \
  %{buildroot}%{_sysconfdir}/pam.d/
install -Dm 644 packaging/fontconfig/60-bayu-noto-mono.conf \
  %{buildroot}%{_datadir}/fontconfig/conf.avail/60-bayu-noto-mono.conf
mkdir -p %{buildroot}%{_sysconfdir}/fonts/conf.d
ln -s ../../..%{_datadir}/fontconfig/conf.avail/60-bayu-noto-mono.conf \
  %{buildroot}%{_sysconfdir}/fonts/conf.d/60-bayu-noto-mono.conf
cd %{buildroot} && find . \( -type f -o -type l \) | sed -e 's#^\.##' \
  -e 's#^/etc/#%%config(noreplace) /etc/#' > %{_builddir}/all-files.list
# The installer is its own package, on live media only.
grep -E 'nabau' %{_builddir}/all-files.list > %{_builddir}/installer-files.list
grep -vE 'nabau' %{_builddir}/all-files.list > %{_builddir}/bayu-files.list

%pre
getent passwd bayu-greeter >/dev/null || \
  useradd --system --user-group --home-dir /var/lib/bayu-greeter --create-home bayu-greeter
:

%post
gtk-update-icon-cache -q -f /usr/share/icons/* 2>/dev/null || :
glib-compile-schemas /usr/share/glib-2.0/schemas 2>/dev/null || :

%files -n nabau -f %{_builddir}/installer-files.list
%dir %{_libexecdir}/nabau
%dir %{_datadir}/cockpit
%dir %{_datadir}/cockpit/anaconda-webui

%files -f %{_builddir}/bayu-files.list
%license LICENSE
%doc README.md UPSTREAM.md
