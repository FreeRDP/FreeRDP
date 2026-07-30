ARG BASE_IMAGE=scratch

FROM ${BASE_IMAGE} AS base
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ARG IMAGE_NAME
ARG IMAGE_SAFE
ARG RPM_AV1
ARG SOURCE_VERSION
ARG PACKAGE_FAMILY=auto
ENV IMAGE_NAME=${IMAGE_NAME}
ENV IMAGE_SAFE=${IMAGE_SAFE}
ENV RPM_AV1=${RPM_AV1}
ENV SOURCE_VERSION=${SOURCE_VERSION}
ENV PACKAGE_FAMILY=${PACKAGE_FAMILY}
WORKDIR /work
COPY distro_package_builds/ /input/distro_package_builds/
RUN chmod +x /input/distro_package_builds/*.sh && mkdir -p /input /logs /work

FROM base AS probe
COPY freerdp-src.tar.gz /input/freerdp-src.tar.gz
RUN MODE=probe /bin/bash /input/distro_package_builds/container-run.sh

FROM base AS bootstrap
RUN MODE=bootstrap /bin/bash /input/distro_package_builds/container-run.sh

FROM bootstrap AS builddeps
COPY freerdp-deps-src.tar.gz /input/freerdp-src.tar.gz
ENV FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP=1
RUN FREERDP_SKIP_WEBVIEW_CLONE=1 MODE=deps /bin/bash /input/distro_package_builds/container-run.sh

FROM builddeps AS package
COPY freerdp-src.tar.gz /input/freerdp-src.tar.gz
ENV FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP=1
ENV FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS=1
RUN rm -rf /logs/artifacts && MODE=package /bin/bash /input/distro_package_builds/container-run.sh
