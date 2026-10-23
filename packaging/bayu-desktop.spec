# Packages the tree that build.sh installed into $STAGE (see build-rpm.sh).
%global debug_package %{nil}
%global __brp_mangle_shebangs %{nil}

Name:           bayu-desktop
Version:        %{?bayu_version}%{!?bayu_version:0.1.0}
Release:        %{?bayu_release}%{!?bayu_release:1}%{?dist}
Summary:        Bayu desktop environment
License:        BSD-3-Clause
URL:            https://github.com/igos-nusantara/bayu
Source0:        bayu-stage.tar
Source1:        bayu-dm.pam
Source2:        bayu-dm-greeter.pam
Source3:        bayu-auth.pam
Source4:        60-bayu-noto-mono.conf

# Runtime services, from upstream's getting-started step 4.
Requires:       accountsservice polkit gsettings-desktop-schemas xdg-user-dirs
Requires:       NetworkManager bluez upower (power-profiles-daemon or tuned-ppd) udisks2 fuse3
Requires:       pipewire pipewire-pulseaudio wireplumber gnome-keyring gnome-keyring-pam
Requires:       xdg-desktop-portal xorg-x11-server-Xwayland adwaita-icon-theme
Requires:       mesa-dri-drivers mesa-libEGL mesa-libgbm glibc-langpack-en
# Default UI and fixed-width fonts.
Requires:       google-noto-sans-vf-fonts google-noto-sans-mono-vf-fonts
Requires(pre):  shadow-utils

%description
Bayu is a Wayland desktop environment forked from Cutefish
(https://github.com/cutefishos): compositor, shell, session, display
manager, file manager, settings, terminal and the apps around them.

%prep
%setup -q -c -T
tar -xf %{SOURCE0}

%install
cp -a . %{buildroot}/
install -m 644 %{SOURCE1} %{buildroot}%{_sysconfdir}/pam.d/bayu-dm
install -m 644 %{SOURCE2} %{buildroot}%{_sysconfdir}/pam.d/bayu-dm-greeter
install -m 644 %{SOURCE3} %{buildroot}%{_sysconfdir}/pam.d/bayu-auth
install -Dm 644 %{SOURCE4} %{buildroot}%{_datadir}/fontconfig/conf.avail/60-bayu-noto-mono.conf
mkdir -p %{buildroot}%{_sysconfdir}/fonts/conf.d
ln -s ../../..%{_datadir}/fontconfig/conf.avail/60-bayu-noto-mono.conf %{buildroot}%{_sysconfdir}/fonts/conf.d/60-bayu-noto-mono.conf
cd %{buildroot} && find . \( -type f -o -type l \) | sed -e 's#^\.##' -e 's#^/etc/#%%config(noreplace) /etc/#' > %{_builddir}/files.list

%pre
getent passwd bayu-greeter >/dev/null || \
  useradd --system --user-group --home-dir /var/lib/bayu-greeter --create-home bayu-greeter
:

%post
gtk-update-icon-cache -q -f /usr/share/icons/* 2>/dev/null || :
glib-compile-schemas /usr/share/glib-2.0/schemas 2>/dev/null || :

%files -f %{_builddir}/files.list
