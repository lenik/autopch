# Version is injected by packaging/rpm/Makefile via `zfr version`.
# RPM Version cannot contain '-'; use `zfr version -r` (hyphens → '_').
# srcversion is the unsanitized Meson/git version and names the tarball.
%{!?version:%global version 0.0.0}
%{!?srcversion:%global srcversion %{version}}

Name:           autopch
Version:        %{version}
Release:        1%{?dist}
Summary:        Cluster headers into balanced PCH sets for C/C++ builds

License:        AGPL-3.0-or-later
URL:            https://github.com/lenik/autopch
Packager:       Lenik <autopch@bodz.net>
Source0:        %{name}-%{srcversion}.tar.xz

BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconf
BuildRequires:  bas-c-devel
BuildRequires:  asciidoctor

%description
autopch analyzes C or C++ sources, builds a co-occurrence graph of included
headers, and emits one or more precompiled-header clusters so large projects
can avoid a single oversized PCH without scattering tiny ones.

%prep
%setup -q -n %{name}-%{srcversion}

%build
meson setup build \
    --prefix=%{_prefix} \
    --bindir=%{_bindir} \
    --datadir=%{_datadir} \
    --mandir=%{_mandir} \
    --sysconfdir=%{_sysconfdir} \
    --localstatedir=%{_localstatedir} \
    --buildtype=plain
meson compile -C build

%install
meson install -C build --destdir=%{buildroot}

%files
%{_bindir}/autopch
%{_datadir}/bash-completion/completions/autopch
%{_mandir}/man1/autopch.1*
%{_datadir}/doc/autopch/
%changelog
* Mon Sep 28 2026 Lenik <autopch@bodz.net>
- Rewrite as C/Meson/bas-c PCH clustering tool.
* Thu Aug 20 2026 Lenik <autopch@bodz.net>
- Align spec with debian/control (Meson, AGPL-3.0-or-later).
- Version comes from `zfr version`, the same method meson.build uses.
