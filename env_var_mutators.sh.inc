dump_env_var() {
    local ev=${1}
    local id_array_var_name="KRNOWAK_ARRAY_${ev}"
    declare -n id_array="${id_array_var_name}"
    local id_to_value_map_var_name="KRNOWAK_MAP_${ev}"
    declare -n id_to_value_map="${id_to_value_map_var_name}"
    declare -n original_value="KRNOWAK_ORIGINAL_VALUE_${ev}"

    echo "DUMP FOR ${ev}:"
    echo "array (${id_array_var_name}): (${id_array[@]})"
    echo -n "map (${id_to_value_map_var_name}): ("
    for k in "${!id_to_value_map[@]}"; do
        echo -n " [${k}]=${id_to_value_map[${k}]}"
    done
    echo ' )'
    echo "original value: ${original_value}"
}

setup_env_var() {
    local ev=${1}
    declare -n id_array="KRNOWAK_ARRAY_${ev}"
    #declare -n id_to_value_map="KRNOWAK_MAP_${ev}"
    local n1="KRNOWAK_ORIGINAL_VALUE_${ev}"
    #declare -n original_value="KRNOWAK_ORIGINAL_VALUE_${ev}"

    if [[ -z "${!n1+x}" ]]; then
        declare -g "${n1}"="${!ev}"
    fi
    declare -g -a id_array
    local n2="KRNOWAK_MAP_${ev}"
    #declare -g -A id_to_value_map
    declare -g -A "${n2}"
}

add_to_env_var() {
    local ev="${1}"
    local id="${2}"
    local value="${3}"
    declare -n id_array="KRNOWAK_ARRAY_${ev}"
    declare -n id_to_value_map="KRNOWAK_MAP_${ev}"

    if [[ -z "${id_to_value_map[$id]}" ]]; then # unset
        if [[ ${#id_array[@]} -gt 0 ]]; then
            id_array=("${id}" "${id_array[@]}")
        else
            id_array=("${id}")
        fi
    fi
    id_to_value_map["${id}"]="${value}"
    rebuild_env_var "${ev}"
}

reset_env_var() {
    local ev="${1}"
    declare -n id_array="KRNOWAK_ARRAY_${ev}"
    declare -n id_to_value_map="KRNOWAK_MAP_${ev}"

    unset id_array
    unset id_to_value_map
    setup_env_var "${ev}"
    rebuild_env_var "${ev}"
}

rebuild_env_var() {
    local ev=${1}
    declare -n id_array="KRNOWAK_ARRAY_${ev}"
    declare -n id_to_value_map="KRNOWAK_MAP_${ev}"
    declare -n original_value="KRNOWAK_ORIGINAL_VALUE_${ev}";
    local rebuild_func="KRNOWAK_REBUILD_FUNC_${ev}"
    local values=()

    for id in "${id_array[@]}"; do
        if [[ -z "${id_to_value_map[${id}]+x}" ]]; then #unset
            :
        else
            values+=("${id_to_value_map[${id}]}")
        fi
    done

    "${rebuild_func}" "${original_value}" "${values[@]}"
}

KRNOWAK_JOIN_BY() {
    local delimiter="${1}"
    shift
    if [[ ${#} -eq 0 ]]; then
        :
    elif [[ ${#} -eq 1 ]]; then
        printf "%s" "${1}"
    else
        local first="${1}"
        shift
        printf "%s%s" "${first}" "${@/#/${delimiter}}"
    fi
}

KRNOWAK_REBUILD_LISTLIKE() {
    local original_value="${1}"
    local ev="${2}"
    shift 2
    local values=()

    values+=("${@}")
    if [[ -n "${original_value}" ]]; then
        values+=("${original_value}")
    fi
    export "${ev}=$(KRNOWAK_JOIN_BY ':' "${values[@]}")"
}

KRNOWAK_REBUILD_FUNC_PATH() {
    local original_value="${1}"
    shift
    KRNOWAK_REBUILD_LISTLIKE "${original_value}" 'PATH' "${@}"
}

KRNOWAK_REBUILD_FUNC_PS1DATA() {
    local original_value="${1}"
    shift
    local values=()
    local joined=''

    if [[ -n "${original_value}" ]]; then
        values+=("${original_value}")
    fi
    values+=("${@}")
    if [[ ${#values[@]} -gt 0 ]]; then
        joined=$(KRNOWAK_JOIN_BY '], [' "${values[@]}")
        export PS1="[\u@\h ([${joined}]) \W]\\$ "
    else
        export PS1='[\u@\h \W]\\$ '
    fi
}

setup_env_var PATH
setup_env_var PS1DATA
setup_env_var GOPATH
