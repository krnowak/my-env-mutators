#!/bin/bash

set -euo pipefail

if [[ ${#} -gt 0 ]]
then
    version="${1}"
else
    version='master'
fi

dev_file="dev${version}.sh.inc"
clean_file="clean-go.sh.inc"
dir="$(realpath "$(dirname "${0}")")"
real_dir="$(dirname "$(realpath "${0}")")"
mutators_lib="${real_dir}/env_var_mutators.sh.inc"
gopath_rebuilder="${real_dir}/gopath_rebuilder.sh.inc"
printf -v mutators_lib_escaped '%q' "${mutators_lib}"
printf -v gopath_rebuilder_escaped '%q' "${gopath_rebuilder}"
printf -v gopath_escaped '%q' "${PWD}/GOPATH"

path_extra="${dir}/go-${version}/bin:${PWD}/GOPATH/bin"
if [[ "${PWD}" != "${dir}" ]]; then
    path_extra+=":${dir}/GOPATH/bin"
fi
printf -v path_extra_escaped '%q' "${path_extra}"
ps1_extra="go-${version}, $(basename "${PWD}")"
printf -v ps1_extra_escaped '%q' "${ps1_extra}"

{
    echo "source ${mutators_lib_escaped}"
    echo "source ${gopath_rebuilder_escaped}"
    echo "evm_add GOPATH go ${gopath_escaped}"
    echo "evm_add PATH go ${path_extra_escaped}"
    echo "evm_add PS1 go ${ps1_extra_escaped}"
} >"${dev_file}"

{
    echo "source ${mutators_lib_escaped}"
    echo "source ${gopath_rebuilder_escaped}"
    echo "evm_clean_id go"
} >"${clean_file}"
