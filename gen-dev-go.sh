#!/bin/bash

set -e

if [[ -n ${1} ]]
then
    version="${1}"
else
    version='master'
fi

fn="dev${version}.inc"
dir=$(realpath $(dirname "${0}"))

cat <<EOF >"${fn}"
LKSJHDFSKDH_GO_VERSION="${version}"
LKSJHDFSKDH_PWD="${PWD}"
LKSJHDFSKDH_GO_DIR="${dir}"
EOF

cat <<'EOF' >>"${fn}"
export GOPATH="${LKSJHDFSKDH_PWD}/GOPATH"
add_to_env_var PATH go "${LKSJHDFSKDH_GO_DIR}/go-${LKSJHDFSKDH_GO_VERSION}/bin:${GOPATH}/bin"
add_to_env_var PS1DATA go "go-${LKSJHDFSKDH_GO_VERSION}, $(basename ${LKSJHDFSKDH_PWD})"

unset LKSJHDFSKDH_GO_DIR
unset LKSJHDFSKDH_PWD
unset LKSJHDFSKDH_GO_VERSION
EOF
