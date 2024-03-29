if [[ -n ${_evm_sh_inc_included:-} ]]; then
    return
fi
declare -g _evm_sh_inc_included
_evm_sh_inc_included=x

_evm_name() {
    local actual_name en_name_var_name
    actual_name=${1}; shift
    en_name_var_name=${1:-}; shift

    if [[ -z ${en_name_var_name} ]] || [[ ${en_name_var_name} = '-' ]]; then
        return 0
    fi

    local -n en_name_var_ref="${en_name_var_name}"
    # shellcheck disable=SC2034
    en_name_var_ref=${actual_name}
}

_evm_names() {
    local ev
    ev=${1}; shift

    _evm_name "_EVM_IDS_${ev}" "${1:-}"; shift
    _evm_name "_EVM_ID_VALUES_${ev}" "${1:-}"; shift
    _evm_name "_EVM_ORIGINAL_VALUE_${ev}" "${1:-}"; shift
}

_evm_global_names() {
    _evm_name '_EVM_ALL_ENV_VARS' "${1:-}"; shift
    _evm_name '_EVM_ALL_IDS' "${1:-}"; shift
}

_evm_setup() {
    local ev
    ev=${1}; shift

    local id_array_name id_to_value_map_name original_value_name

    _evm_names "${ev}" id_array_name id_to_value_map_name original_value_name

    declare -g -a "${id_array_name}"
    local -n id_array="${id_array_name}"
    # shellcheck disable=SC2034
    id_array=()

    declare -g -A "${id_to_value_map_name}"
    local -n id_to_value_map="${id_to_value_map_name}"
    # shellcheck disable=SC2034
    id_to_value_map=()

    local -n original_value="${original_value_name}"
    if [[ -z ${original_value+x} ]]; then # is unset
        original_value=${!ev-}
    fi
}

_evm_ensure_global_map() {
    local map_name
    map_name=${1}; shift

    if ! declare -p "${map_name}" &>/dev/null; then
        declare -g -A "${map_name}"
        local -n map_ref="${map_name}"
        # shellcheck disable=SC2034
        map_ref=()
        unset -n map_ref
    fi
}

_evm_ensure() {
    local ev
    ev=${1}; shift

    local id_array_name id_to_value_map_name original_value_name all_env_vars_name all_ids_name

    _evm_names "${ev}" id_array_name id_to_value_map_name original_value_name
    _evm_global_names all_env_vars_name all_ids_name

    _evm_ensure_global_map "${all_env_vars_name}"
    _evm_ensure_global_map "${all_ids_name}"

    # shellcheck disable=SC2178
    local -n all_env_vars_ref="${all_env_vars_name}"

    if ! declare -p "${id_array_name}" "${id_to_value_map_name}" "${original_value_name}" >/dev/null 2>&1; then
        _evm_setup "${ev}"
        # shellcheck disable=SC2034
        all_env_vars_ref["${ev}"]=x
    fi
}

_evm_with_globals() {
    local func
    func=${1}; shift
    # rest are params to the func

    local all_env_vars_name all_ids_name

    _evm_global_names all_env_vars_name all_ids_name

    _evm_ensure_global_map "${all_env_vars_name}"
    _evm_ensure_global_map "${all_ids_name}"

    local -n _evm_i_all_env_vars="${all_env_vars_name}"
    local -n _evm_i_all_ids="${all_ids_name}"

    "${func}" "${@}"
}

_evm_with_ev() {
    local ev
    ev=${1}; shift
    # rest are a func and params to the func

    _evm_ensure "${ev}"

    local id_array_name id_to_value_map_name original_value_name

    _evm_names "${ev}" id_array_name id_to_value_map_name original_value_name

    # shellcheck disable=SC2178
    local -n _evm_i_id_array="${id_array_name}"
    # shellcheck disable=SC2178
    local -n _evm_i_id_to_value_map="${id_to_value_map_name}"
    local -n _evm_i_original_value="${original_value_name}"
    # This gets modified in _evm_add_impl, _evm_clean_impl and _evm_reset_impl
    local changed=''

    _evm_with_globals "${@}"

    if [[ -n ${changed} ]]; then
        local rebuild_func
        rebuild_func="evm_rebuild_env_var_${ev}"

        values=()

        unset -n _evm_i_id_array
        unset -n _evm_i_id_to_value_map
        unset -n _evm_i_original_value
        # shellcheck disable=SC2178
        local -n _evm_i_id_array="${id_array_name}"
        # shellcheck disable=SC2178
        local -n _evm_i_id_to_value_map="${id_to_value_map_name}"
        local -n _evm_i_original_value="${original_value_name}"

        local -a values
        local id value
        for id in "${_evm_i_id_array[@]}"; do
            if [[ -n ${_evm_i_id_to_value_map["${id}"]+x} ]]; then # is set
                value=${_evm_i_id_to_value_map["${id}"]}
                values+=("${value}")
            fi
        done

        "${rebuild_func}" "${_evm_i_original_value}" "${values[@]}"
    fi
}

_evm_param_check() {
    local expected
    expected=${1}; shift

    if [[ ${expected} -ne "${#}" ]]; then
        return 1
    fi

    local arg
    for arg; do
        if [[ -z ${arg} ]]; then
            return 1
        fi
    done
    return 0
}

_evm_print_list() {
    local id
    id=${1}; shift

    local joined
    joined=$(evm_join_by "', " "${@}")
    echo "${id}: '${joined}'"
}

_evm_dump_ev_impl() {
    local id value

    _evm_print_list "    ids" "${_evm_i_id_array[@]}"
    echo "    values per id:"
    for id in "${!_evm_i_id_to_value_map[@]}"; do
        value=${_evm_i_id_to_value_map["${id}"]}
        echo "      ${id}: ${value}"
    done
    echo "    original value: ${_evm_i_original_value}"
}

_evm_dump_impl() {
    local ev

    _evm_print_list "all env vars" "${!_evm_i_all_env_vars[@]}"
    _evm_print_list "all ids" "${!_evm_i_all_ids[@]}"
    echo

    for ev in "${!_evm_i_all_env_vars[@]}"; do
        echo "- ${ev}:"
        _evm_with_ev "${ev}" _evm_dump_ev_impl
    done
}

# no params
evm_dump() {
    _evm_param_check 0 "${@}" || return 1

    _evm_with_globals _evm_dump_impl
}

_evm_add_impl() {
    local id value
    id=${1}; shift
    value=${1}; shift

    if [[ -z ${_evm_i_id_to_value_map["${id}"]+x} ]]; then # is unset
        _evm_i_id_array+=( "${id}" )
    fi
    _evm_i_id_to_value_map["${id}"]=${value}
    _evm_i_all_ids["${id}"]=x
    changed=x
}

# 1 - env var (GOPATH, PATH, PS1)
# 2 - id (go, rust, k8s)
# 3 - value
evm_add() {
    _evm_param_check 3 "${@}" || return 1
    local ev
    ev=${1}; shift

    _evm_with_ev "${ev}" _evm_add_impl "${@}"
}

_evm_clean_impl() {
    local id
    id=${1}; shift

    local id2
    for id2 in "${_evm_i_id_array[@]}"; do
        if [[ ${id2} != "${id}" ]]; then
            new_array+=( "${id2}" )
        fi
    done
    _evm_i_id_array=( "${new_array[@]}" )
    unset "_evm_i_id_to_value_map[${id}]"
    changed=x
}

# 1 - env var (GOPATH, PATH, PS1)
# 2 - id (go, rust, k8s)
evm_clean() {
    _evm_param_check 2 "${@}" || return 1
    local ev
    ev=${1}; shift

    _evm_with_ev "${ev}" _evm_clean_impl "${@}"
}

_evm_clean_id_impl() {
    local id
    id=${1}; shift

    local value ev

    value=${_evm_i_all_ids["${id}"]:-}
    if [[ -z ${value} ]]; then
        return 0
    fi

    for ev in "${!_evm_i_all_env_vars[@]}"; do
        evm_clean "${ev}" "${id}"
    done
    unset "_evm_i_all_ids[${id}]"
}

# 1 - id (go, rust, k8s)
evm_clean_id() {
    _evm_param_check 1 "${@}" || return 1

    _evm_with_globals _evm_clean_id_impl "${@}"
}

_evm_reset_impl() {
    local ev
    ev=${1}; shift

    unset _evm_i_id_array
    unset _evm_i_id_to_value_map
    unset "_evm_i_all_env_vars[${ev}]"
    _evm_setup "${ev}"
    changed=x
}

# 1 - env var (GOPATH, PATH, PS1)
evm_reset() {
    _evm_param_check 1 "${@}" || return 1
    local ev
    ev=${1}; shift

    _evm_with_ev "${ev}" _evm_reset_impl "${ev}"
}

evm_join_by() {
    local delimiter first
    delimiter=${1-}
    first=${2-}

    if shift 2; then
        printf '%s' "${first}" "${@/#/${delimiter}}";
    fi
}

evm_rebuild_listlike() {
    local original_value ev
    original_value=${1}; shift
    ev=${1}; shift
    # rest are values

    local values=()
    values+=("${@}")
    if [[ -n ${original_value} ]]; then
        values+=("${original_value}")
    fi
    export "${ev}=$(evm_join_by ':' "${values[@]}")"
}

evm_rebuild_env_var_PATH() {
    local original_value
    original_value=${1}; shift
    evm_rebuild_listlike "${original_value}" 'PATH' "${@}"
}

evm_rebuild_env_var_PS1() {
    local original_value
    original_value=${1}; shift

    local joined result
    joined=''
    if [[ ${#} -gt 0 ]]; then
        joined="([$(evm_join_by '], [' "${@}")])"
        if [[ ${original_value} = *'\w'* ]]; then
            result=${original_value//\\w/"${joined}" \\w}
        elif [[ ${original_value} = *'\W'* ]]; then
            result=${original_value//\\W/"${joined}" \\W}
        else
            result=${original_value//\\$/"${joined}" \\$}
        fi
    else
        result=${original_value}
    fi
    export PS1=${result}
}
