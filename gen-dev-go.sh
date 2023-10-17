#!/bin/bash

set -euo pipefail

if [[ ${#} -gt 0 ]]
then
    version="${1}"
else
    version='master'
fi

dev_file="dev${version}.sh.inc"
clean_file='clean-go.sh.inc'
dir=$(realpath "$(dirname "${0}")")
real_dir=$(dirname "$(realpath "${0}")")
mutators_lib="${real_dir}/env_var_mutators.sh.inc"
gopath_rebuilder="${real_dir}/gopath_rebuilder.sh.inc"
gopath="${PWD}/GOPATH"

path_extra="${dir}/go-${version}/bin:${gopath}/bin"
if [[ "${PWD}" != "${dir}" ]]; then
    path_extra+=":${dir}/GOPATH/bin"
fi
ps1_extra="go-${version}, $(basename "${PWD}")"

df_lines=(
    "source ${mutators_lib@Q}"
    "source ${gopath_rebuilder@Q}"
    "evm_add GOPATH go ${gopath@Q}"
    "evm_add PATH go ${path_extra@Q}"
    "evm_add PS1 go ${ps1_extra@Q}"
)
printf '%s\n' "${df_lines[@]}" >"${dev_file}"

cf_lines=(
    "source ${mutators_lib@Q}"
    "source ${gopath_rebuilder@Q}"
    "evm_clean_id go"
)
printf '%s\n' "${cf_lines[@]}" >"${clean_file}"
