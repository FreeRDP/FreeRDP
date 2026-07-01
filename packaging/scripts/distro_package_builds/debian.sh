#!/usr/bin/env bash

package_probe()
{
	log "Package manager: $(command -v apt-get)"
	log "Checking source snapshot"
	test -s /input/freerdp-src.tar.gz
}

install_deb_bootstrap()
{
	export DEBIAN_FRONTEND=noninteractive
	log "Installing Debian package-build bootstrap"
	apt-get update
	apt-get install -y --no-install-recommends \
		ca-certificates \
		devscripts \
		dpkg-dev \
		equivs \
		fakeroot \
		git
}

package_bootstrap()
{
	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP:-0}"; then
		log "Using cached Debian package-build bootstrap"
		export DEBIAN_FRONTEND=noninteractive
		return
	fi

	install_deb_bootstrap
}

prepare_deb_control()
{
	log "Preparing generated debian/control"
	./packaging/scripts/prepare_deb_freerdp-nightly.sh
}

install_deb_builddeps()
{
	prepare_deb_control

	log "Installing Debian Build-Depends"
	mk-build-deps -i -r \
		-t "apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends -y" \
		debian/control
}

apply_deb_package_workarounds()
{
	local opensc_version

	if [[ "${ID:-}" != "ubuntu" || "${VERSION_ID:-}" != "26.04" ]]; then
		return
	fi

	opensc_version=$(dpkg-query -W -f='${Version}' opensc-pkcs11 2>/dev/null || true)
	if [[ -n "$opensc_version" ]] && dpkg --compare-versions "$opensc_version" ge "0.27.1-1"; then
		log "Ubuntu OpenSC PKCS#11 package is already fixed: opensc-pkcs11=$opensc_version"
		return
	fi

	# Ubuntu 26.04 currently ships OpenSC 0.27.0~rc1, which dlopens libpcsclite with
	# RTLD_DEEPBIND and aborts ASAN test runs. Use Debian's fixed OpenSC package until
	# Ubuntu carries OpenSC commit 776e4cd2874108acc1b563be573dd96aaab58cdb.
	log "Applying Ubuntu 26.04 OpenSC ASAN workaround"
	apt-get update
	apt-get install -y --no-install-recommends debian-archive-keyring
	cat >/etc/apt/sources.list.d/debian-forky-opensc.sources <<'EOF'
Types: deb
URIs: http://deb.debian.org/debian/
Suites: forky
Components: main
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
EOF
	cat >/etc/apt/preferences.d/opensc-forky <<'EOF'
Package: *
Pin: release n=forky
Pin-Priority: 100

Package: opensc-pkcs11
Pin: release n=forky
Pin-Priority: 990
EOF
	apt-get update
	apt-get install -y --no-install-recommends opensc-pkcs11=0.27.1-1
	dpkg-query -W opensc-pkcs11
}

package_deps()
{
	package_bootstrap
	prepare_source
	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
		log "Using cached Debian Build-Depends"
	else
		install_deb_builddeps
	fi
}

package_build()
{
	package_bootstrap
	prepare_source
	prepare_deb_control

	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
		log "Using cached Debian Build-Depends"
	else
		log "Installing Debian Build-Depends"
		mk-build-deps -i -r \
			-t "apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends -y" \
			debian/control
	fi

	apply_deb_package_workarounds

	log "Building Debian package"
	dpkg-buildpackage -us -uc -b

	log "Collecting Debian artifacts"
	find .. -maxdepth 1 \( -name '*.deb' -o -name '*.buildinfo' -o -name '*.changes' \) \
		-exec cp -v {} "$(artifact_dir)"/ \;
}
