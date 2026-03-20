#!/bin/sh
set -eu

PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-}"
BINARY_NAME="${BINARY_NAME:-poudb}"
SERVICE_NAME="${SERVICE_NAME:-poudb.service}"

BIN_PATH="${DESTDIR}${PREFIX}/bin/${BINARY_NAME}"
CONF_PATH="${DESTDIR}/etc/poudb/poudb.conf"
ETC_DIR="${DESTDIR}/etc/poudb"
DATA_DIR="${DESTDIR}/var/lib/poudb"
UNIT_PATH="${DESTDIR}/etc/systemd/system/${SERVICE_NAME}"

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

if [ -f "${BIN_PATH}" ]; then
    rm -f "${BIN_PATH}"
    echo "Removed ${BIN_PATH}"
fi

if [ -f "${CONF_PATH}" ]; then
    rm -f "${CONF_PATH}"
    echo "Removed ${CONF_PATH}"
fi

if [ -f "${UNIT_PATH}" ]; then
    rm -f "${UNIT_PATH}"
    echo "Removed ${UNIT_PATH}"
fi

if [ "$(id -u)" -eq 0 ] && [ -z "${DESTDIR}" ] && command_exists systemctl; then
    systemctl disable --now "${SERVICE_NAME}" >/dev/null 2>&1 || true
    systemctl daemon-reload || true
fi

rmdir "${ETC_DIR}" 2>/dev/null || true
rmdir "${DATA_DIR}" 2>/dev/null || true

echo "Uninstall completed for ${BINARY_NAME}"
