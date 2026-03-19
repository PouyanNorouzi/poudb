#!/bin/sh
set -eu

PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-}"
RUN_USER="${RUN_USER:-poudb}"
RUN_GROUP="${RUN_GROUP:-poudb}"
BINARY_NAME="${BINARY_NAME:-poudb}"

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)

BIN_SRC="${REPO_ROOT}/bin/${BINARY_NAME}"
CONF_SRC="${REPO_ROOT}/poudb.conf.example"

BIN_DIR="${DESTDIR}${PREFIX}/bin"
ETC_DIR="${DESTDIR}/etc/poudb"
DATA_DIR="${DESTDIR}/var/lib/poudb"
CONF_DST="${ETC_DIR}/poudb.conf"

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

ensure_group() {
    if getent group "${RUN_GROUP}" >/dev/null 2>&1; then
        return
    fi

    if command_exists groupadd; then
        groupadd --system "${RUN_GROUP}" >/dev/null 2>&1 || true
    fi
}

ensure_user() {
    if id -u "${RUN_USER}" >/dev/null 2>&1; then
        return
    fi

    if command_exists useradd; then
        useradd --system --no-create-home --gid "${RUN_GROUP}" \
            --shell /usr/sbin/nologin "${RUN_USER}" >/dev/null 2>&1 || true
    fi
}

if [ ! -f "${BIN_SRC}" ]; then
    echo "Missing binary: ${BIN_SRC}. Run 'make' first." >&2
    exit 1
fi

if [ ! -f "${CONF_SRC}" ]; then
    echo "Missing config template: ${CONF_SRC}" >&2
    exit 1
fi

install -d -m 0755 "${BIN_DIR}" "${ETC_DIR}" "${DATA_DIR}"
install -m 0755 "${BIN_SRC}" "${BIN_DIR}/${BINARY_NAME}"

if [ -e "${CONF_DST}" ]; then
    echo "Keeping existing config: ${CONF_DST}"
else
    install -m 0644 "${CONF_SRC}" "${CONF_DST}"
fi

if [ "$(id -u)" -eq 0 ] && [ -z "${DESTDIR}" ]; then
    ensure_group
    ensure_user

    if id -u "${RUN_USER}" >/dev/null 2>&1 && getent group "${RUN_GROUP}" >/dev/null 2>&1; then
        chown "${RUN_USER}:${RUN_GROUP}" "${DATA_DIR}" || true
        chmod 0750 "${DATA_DIR}" || true
    else
        echo "Warning: could not verify ${RUN_USER}:${RUN_GROUP}; data directory ownership unchanged" >&2
    fi
else
    echo "Skipping user/group creation and ownership changes (non-root or DESTDIR set)"
fi

echo "Installed ${BINARY_NAME} to ${BIN_DIR}/${BINARY_NAME}"
echo "Config path: ${CONF_DST}"
echo "Data path: ${DATA_DIR}"
