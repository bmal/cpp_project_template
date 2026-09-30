# The Linux toolchain for the dev container and CI's Linux jobs. Tool versions live in scripts/bootstrap.sh.
FROM ubuntu:24.04@sha256:008173c23f95b170204355c12626cb5a965d779a7e1283b09e9cffbb1bf33ca3

# vcpkg, pipx tools, and pre-commit hook environments live outside the workspace, shared by every user.
ENV VCPKG_ROOT=/opt/vcpkg \
    VCPKG_DISABLE_METRICS=1 \
    PIPX_HOME=/opt/pipx \
    PIPX_BIN_DIR=/usr/local/bin \
    PRE_COMMIT_HOME=/opt/pre-commit

COPY scripts/bootstrap.sh /opt/bootstrap/scripts/bootstrap.sh
RUN /opt/bootstrap/scripts/bootstrap.sh --ci && rm -rf /var/lib/apt/lists/*

# Installs the hook environments now, so pre-commit run --all-files downloads nothing.
# Any user may build with vcpkg and pre-commit, and git reads a checkout mounted from the host.
COPY .pre-commit-config.yaml /opt/bootstrap/
RUN cd /opt/bootstrap && git init --quiet && pre-commit install-hooks && \
    cd / && rm -rf /opt/bootstrap && chmod -R a+rwX "${VCPKG_ROOT}" "${PRE_COMMIT_HOME}" && \
    git config --system --add safe.directory '*'

WORKDIR /workspaces
