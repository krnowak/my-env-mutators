#!/bin/bash

krnowak_prepare_aliases_setup() {
    local this_file this_dir aliases_dir cmd

    this_file=$(realpath "${1}")
    this_dir=$(dirname "${this_file}")
    aliases_dir="${this_dir}/aliases"

    for cmd in "${aliases_dir}/"*.sh.inc; do
        if [[ ! -f "${cmd}" ]]; then
            continue
        fi
        # drop .sh.inc
        cmd="${cmd%.*}"
        cmd="${cmd%.*}"
        # drop dirs
        cmd="${cmd##*/}"
        # shellcheck disable=SC2139
        alias "${cmd}=source ${aliases_dir}/${cmd}.sh.inc"
    done
}

if [[ ${#BASH_SOURCE[@]} -gt 0 ]]; then
    krnowak_prepare_aliases_setup "${BASH_SOURCE[0]}"
fi

unset -f krnowak_prepare_aliases_setup
