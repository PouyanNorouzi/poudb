#!/bin/sh
set -eu

PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-}"
BINARY_NAME="${BINARY_NAME:-poudb}"

BIN_PATH="${DESTDIR}${PREFIX}/bin/${BINARY_NAME}"
CONF_PATH="${DESTDIR}/etc/poudb/poudb.conf"
ETC_DIR="${DESTDIR}/etc/poudb"
DATA_DIR="${DESTDIR}/var/lib/poudb"

if [ -f "${BIN_PATH}" ]; then
    rm -f "${BIN_PATH}"
    echo "Removed ${BIN_PATH}"
fi

if [ -f "${CONF_PATH}" ]; then
    rm -f "${CONF_PATH}"
    echo "Removed ${CONF_PATH}"
fi

rmdir "${ETC_DIR}" 2>/dev/null || true
rmdir "${DATA_DIR}" 2>/dev/null || true

echo "Uninstall completed for ${BINARY_NAME}"
