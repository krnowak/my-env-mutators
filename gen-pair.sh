#!/bin/bash

##
## gen-pair.sh [-s <path>] [-h] <name> <branch>
##
## Generates environment mutators for project <name> with default
## branch <branch>. The mutator will add path
## ${PWD}/<name>/<branch>/<suffix> to PATH to <name>-<branch> to PS1.
##
## The <branch> part can be overridden at runtime by passing a
## parameter to the generated file, like "source mkosi.sh.inc
## other-branch".
##

set -euo pipefail

this_dir=$(dirname "${0}")
this_dir=$(realpath "${this_dir}")

function fail {
    printf '%s\n' "${*}" >&2
    exit 1
}

suffix=''
while [[ ${#} -gt 0 ]]; do
    case ${1} in
        -h)
            grep '^##' "${0}" | sed -e 's/^##[[:space:]]*//'
            exit 0
            ;;
        -s)
            if [[ ${#} -eq 0 ]]; then
                fail 'Missing parameter for -s'
            fi
            suffix=${2}
            shift 2
            ;;
        --)
            shift
            break
            ;;
        -*)
            fail "Unknown flag ${1}"
            ;;
        *)
            break
            ;;
    esac
done

name=${1}; shift
branch=${1}; shift

one_or_branch_q='"${1:-'"${branch}"'}"'

pwd_slash_q='"${PWD}/"'
name_slash="${name}/"
maybe_slash_suffix="${suffix+/}${suffix}"
maybe_slash_suffix_q=${maybe_slash_suffix+${maybe_slash_suffix@Q}}
name_dash="${name}-"

path="${pwd_slash_q}${name_slash@Q}${one_or_branch_q}${maybe_slash_suffix_q}"
ps="${name_dash@Q}${one_or_branch_q}"

shinc="${this_dir}/env_var_mutators.sh.inc"
cat <<EOF >"${name}.sh.inc"
source ${shinc@Q}
evm_add PATH ${name@Q} ${path}
evm_add PS1 ${name@Q} ${ps}
EOF

cat <<EOF >"clean-${name}.sh.inc"
source ${shinc@Q}
evm_clean_id ${name@Q}
EOF
