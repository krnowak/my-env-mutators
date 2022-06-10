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


path_extra="${dir}/go-${version}/bin:${PWD}/GOPATH/bin"
if [[ "${PWD}" != "${dir}" ]]; then
    path_extra+=":${dir}/GOPATH/bin"
fi
ps1_extra="go-${version}, $(basename "${PWD}")"


truncate --size=0 "${dev_file}"

echo "source '${mutators_lib}'" >>"${dev_file}"
echo "add_to_env_var GOPATH go '${PWD}/GOPATH'" >>"${dev_file}"
echo "add_to_env_var PATH go '${path_extra}'" >>"${dev_file}"
echo "add_to_env_var PS1 go '${ps1_extra}'" >>"${dev_file}"


truncate --size=0 "${clean_file}"

echo "source '${mutators_lib}'" >>"${clean_file}"
echo "clean_from_env_var GOPATH go" >>"${clean_file}"
echo "clean_from_env_var PATH go" >>"${clean_file}"
echo "clean_from_env_var PS1 go" >>"${clean_file}"
