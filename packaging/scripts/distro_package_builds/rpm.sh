#!/usr/bin/env bash

package_probe()
{
	log "Package manager: $(command -v zypper || command -v dnf5 || command -v dnf)"
	log "Checking source snapshot"
	test -s /input/freerdp-src.tar.gz
}

configure_dnf_commands()
{
	RPM_BUILDDEP_KIND=dnf
	RPM_BUILDDEP_CMD=(dnf -y builddep --spec)
	RPM_INSTALL_CMD=(dnf -y install)
}

install_dnf_bootstrap()
{
	log "Installing RPM package-build bootstrap"
	dnf -y install dnf-plugins-core "dnf-command(builddep)" || true

	if [[ -f /etc/almalinux-release ]]; then
		local alma_release_devel=almalinux-release-devel
		if [[ "${NAME:-}" == "AlmaLinux Kitten" ]]; then
			alma_release_devel=almalinux-kitten-release-devel
		fi
		dnf -y install "$alma_release_devel" || true
		dnf install -y --nogpgcheck https://dl.fedoraproject.org/pub/epel/epel-release-latest-$(rpm -E %rhel).noarch.rpm https://mirrors.rpmfusion.org/free/el/rpmfusion-free-release-$(rpm -E %rhel).noarch.rpm
		/usr/bin/crb enable
	fi

	dnf -y upgrade
	dnf -y install \
		ca-certificates \
		findutils \
		git \
		rpm-build \
		tar \
		which \
		xz || true

	configure_dnf_commands
}

configure_zypper_commands()
{
	RPM_INSTALL_CMD=(zypper --non-interactive install --no-recommends)
}

install_zypper_bootstrap()
{
	log "Installing RPM package-build bootstrap with zypper"
	zypper --non-interactive refresh
	zypper --non-interactive install --no-recommends \
		ca-certificates \
		findutils \
		git \
		rpm-build \
		tar \
		which \
		xz

	configure_zypper_commands
}

configure_rpm_commands()
{
	if command -v zypper >/dev/null 2>&1; then
		configure_zypper_commands
	elif command -v dnf5 >/dev/null 2>&1 || command -v dnf >/dev/null 2>&1; then
		configure_dnf_commands
	else
		echo "No supported RPM package manager found" >&2
		return 1
	fi
}

install_rpm_bootstrap()
{
	if command -v zypper >/dev/null 2>&1; then
		install_zypper_bootstrap
	elif command -v dnf5 >/dev/null 2>&1 || command -v dnf >/dev/null 2>&1; then
		install_dnf_bootstrap
	else
		echo "No supported RPM package manager found" >&2
		return 1
	fi
}

package_bootstrap()
{
	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP:-0}"; then
		log "Using cached RPM package-build bootstrap"
		configure_rpm_commands
		return
	fi

	install_rpm_bootstrap
}

rpm_av1_args()
{
	case "${RPM_AV1:-auto}" in
		on)
			printf '%s\0%s\0' --with av1
			;;
		off)
			printf '%s\0%s\0' --without av1
			;;
		auto)
			;;
	esac
}

rpm_av1_builddep_args()
{
	case "${RPM_AV1:-auto}" in
		on)
			printf '%s\0%s\0' -D '_with_av1 1'
			;;
		off)
			printf '%s\0%s\0' -D '_without_av1 1'
			;;
		auto)
			;;
	esac
}

normalize_rpm_requirement()
{
	local requirement=$1

	if [[ "$requirement" != \(* && "$requirement" =~ ^([^[:space:]<>=]+)[[:space:]]*(<=|>=|=|<|>) ]]; then
		requirement=${BASH_REMATCH[1]}
	fi

	printf '%s\n' "$requirement"
}

install_rpm_builddeps()
{
	local -a builddep_args=()
	local -a rpmspec_args=()
	local -a requires=()
	local requirement

	if [[ "${RPM_BUILDDEP_KIND:-}" == dnf ]]; then
		while IFS= read -r -d '' arg; do
			builddep_args+=("$arg")
		done < <(rpm_av1_builddep_args)

		log "Installing RPM BuildRequires"
		"${RPM_BUILDDEP_CMD[@]}" "${builddep_args[@]}" packaging/rpm/freerdp-nightly.spec
		return
	fi

	while IFS= read -r -d '' arg; do
		rpmspec_args+=("$arg")
	done < <(rpm_av1_builddep_args)

	log "Installing RPM BuildRequires"
	while IFS= read -r requirement; do
		requirement=$(normalize_rpm_requirement "$requirement")
		requires+=("$requirement")
	done < <(
		rpmspec "${rpmspec_args[@]}" -q --buildrequires packaging/rpm/freerdp-nightly.spec |
			sed -E -e '/^[[:space:]]*$/d' |
			sed -E -e '/^(and|or)$/d' |
			sort -u
	)

	if [[ ${#requires[@]} -gt 0 ]]; then
		"${RPM_INSTALL_CMD[@]}" "${requires[@]}"
	fi
}

package_deps()
{
	package_bootstrap
	prepare_source
	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
		log "Using cached RPM BuildRequires"
	else
		install_rpm_builddeps
	fi
}

package_build()
{
	local artifact
	local rpm_topdir="$HOME/rpmbuild"
	local -a rpm_args=()

	package_bootstrap
	prepare_source

	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
		log "Using cached RPM BuildRequires"
	else
		install_rpm_builddeps
	fi

	while IFS= read -r -d '' arg; do
		rpm_args+=("$arg")
	done < <(rpm_av1_args)

	log "Preparing RPM source archive"
	mkdir -p "$rpm_topdir/BUILD" "$rpm_topdir/BUILDROOT" "$rpm_topdir/RPMS" \
		"$rpm_topdir/SOURCES" "$rpm_topdir/SPECS" "$rpm_topdir/SRPMS"
	rm -rf /work/rpm-src
	mkdir -p /work/rpm-src/freerdp-nightly-3.0
	tar -C /work/src -cf - . | tar -C /work/rpm-src/freerdp-nightly-3.0 -xf -
	tar -C /work/rpm-src -cjf "$rpm_topdir/SOURCES/freerdp-nightly-3.0.tar.bz2" freerdp-nightly-3.0
	cp -v /work/src/source_version "$rpm_topdir/SOURCES/source_version"

	log "Building RPM package"
	rpmbuild -ba --define "_topdir $rpm_topdir" "${rpm_args[@]}" packaging/rpm/freerdp-nightly.spec

	log "Collecting RPM artifacts"
	while IFS= read -r -d '' artifact; do
		cp -v "$artifact" "$(artifact_dir)"/
	done < <(find "$rpm_topdir" \( -name '*.rpm' -o -name '*.src.rpm' \) -print0)
}
