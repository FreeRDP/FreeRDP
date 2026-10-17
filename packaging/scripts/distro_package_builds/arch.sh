#!/usr/bin/env bash

package_probe()
{
	log "Package manager: $(command -v pacman)"
	log "Checking source snapshot"
	test -s /input/freerdp-src.tar.gz
}

install_arch_bootstrap()
{
	log "Installing Arch package-build bootstrap"
	pacman -Syu --noconfirm --needed \
		base-devel \
		ca-certificates \
		git \
		sudo \
		tar \
		xz

	if ! id -u builduser >/dev/null 2>&1; then
		useradd -m -U builduser
	fi
	printf 'builduser ALL=(ALL) NOPASSWD: ALL\n' >/etc/sudoers.d/builduser
	chmod 0440 /etc/sudoers.d/builduser
}

package_bootstrap()
{
	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP:-0}"; then
		log "Using cached Arch package-build bootstrap"
		return
	fi

	install_arch_bootstrap
}

prepare_arch_build_context()
{
	log "Preparing Arch makepkg context"
	rm -rf /work/arch-build /work/arch-src
	mkdir -p /work/arch-build /work/arch-src/freerdp
	tar -C /work/src -cf - . | tar -C /work/arch-src/freerdp -xf -
	tar -C /work/arch-src -czf /work/arch-build/freerdp-src.tar.gz freerdp
	cp -v /work/src/packaging/scripts/arch_PKGBUILD /work/arch-build/PKGBUILD
	patch_arch_pkgbuild_for_local_source /work/arch-build/PKGBUILD
	chown -R builduser:builduser /work/arch-build
}

patch_arch_pkgbuild_for_local_source()
{
	local pkgbuild=${1:-/work/arch-build/PKGBUILD}

	if ! grep -Fq 'source=("$_pkgsrc"::"git+$url.git")' "$pkgbuild"; then
		echo "Unsupported Arch PKGBUILD source declaration" >&2
		return 1
	fi
	if ! grep -Eq '^pkgver\(\)[[:space:]]*\{' "$pkgbuild"; then
		echo "Unsupported Arch PKGBUILD pkgver function" >&2
		return 1
	fi

	sed -i -e 's|source=("$_pkgsrc"::"git+$url.git")|source=("freerdp-src.tar.gz")|' "$pkgbuild"
	sed -i -e '/^pkgver()[[:space:]]*{/,/^}/c\
pkgver() {\
  printf '\''%s\\n'\'' "$pkgver"\
}' "$pkgbuild"
}

install_arch_builddeps_from_pkgbuild()
{
	local -a packages=()

	log "Installing Arch package dependencies"
	# shellcheck disable=SC1091
	source /work/arch-build/PKGBUILD
	packages=("${depends[@]}" "${makedepends[@]}")
	if [[ ${#packages[@]} -gt 0 ]]; then
		pacman -S --noconfirm --needed "${packages[@]}"
	fi
}

package_deps()
{
	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
		log "Using cached Arch package dependencies"
	else
		package_bootstrap
		prepare_source
		prepare_arch_build_context
		install_arch_builddeps_from_pkgbuild
	fi
}

package_build()
{
	package_bootstrap
	prepare_source
	prepare_arch_build_context

	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
		log "Using cached Arch package dependencies"
	else
		install_arch_builddeps_from_pkgbuild
	fi

	log "Building Arch package"
	cd /work/arch-build
	sudo -u builduser \
		env \
		CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc)}" \
		makepkg --noconfirm --needed --cleanbuild --force

	log "Collecting Arch artifacts"
	find /work/arch-build -maxdepth 1 \( -name '*.pkg.tar.*' -o -name '*.src.tar.*' \) \
		-exec cp -v {} "$(artifact_dir)"/ \;
}
