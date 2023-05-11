if [[ -n "${_evm_gopath_sh_inc_included:-}" ]]; then
    return
fi
declare -g _evm_gopath_sh_inc_included
_evm_gopath_sh_inc_included=x

evm_rebuild_env_var_GOPATH() {
    local original_value=${1}; shift
    evm_rebuild_listlike "${original_value}" 'GOPATH' "${@}"
}
