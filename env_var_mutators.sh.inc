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

clean_from_env_var() {
    local ev="${1}"
    local id="${2}"
    declare -a new_array
    declare -n id_array="KRNOWAK_ARRAY_${ev}"
    declare -n id_to_value_map="KRNOWAK_MAP_${ev}"

    for i in "${!id_array[@]}"; do
        if [[ "${id_array[i]}" != "${id}" ]]; then
            new_array+=( "${id_array[i]}" )
        fi
    done
    id_array=("${new_array[@]}")
    unset new_array
    unset id_to_value_map["${id}"]
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

KRNOWAK_REBUILD_SINGULAR_NEWEST() {
    local original_value="${1}"
    local ev="${2}"
    shift 2
    local value=''

    if [[ ${#} -gt 0 ]]; then
        value="${1}"
    else
        value="${original_value}"
    fi
    export "${ev}=${value}"
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

KRNOWAK_REBUILD_FUNC_GOPATH() {
    local original_value="${1}"
    shift
    KRNOWAK_REBUILD_LISTLIKE "${original_value}" 'GOPATH' "${@}"
}

function setup_ls_colors {
    declare -g -A overridden_colors original_colors
    declare fixed_ls_colors

    overridden_colors=(
        ['di']='01;33'
    )
    fixed_ls_colors="${LS_COLORS}"
    # prepend : if there is none in the begining of the string
    if [[ "${fixed_ls_colors:0:1}" != ':' ]]; then
        fixed_ls_colors=":${fixed_ls_colors}"
    fi
    # append : if there is none in the end of the string
    if [[ "${fixed_ls_colors: -1}" != ':' ]]; then
        fixed_ls_colors="${fixed_ls_colors}:"
    fi
    for var in "${!overridden_colors[@]}"; do
        original_color="$(echo "${fixed_ls_colors}" | grep -oe ":${var}=[^:]\+" | cut -f2 -d=)"
        original_colors[${var}]="${original_color}"
    done
}

function adapt_to_night_light {
    declare \
        properties \
        night_light_active \
        disabled_until_tomorrow \
        fixed_ls_colors \
        drop_at_start \
        drop_at_end

    properties=$(gdbus introspect \
                       --session \
                       --dest org.gnome.SettingsDaemon.Color \
                       --object-path /org/gnome/SettingsDaemon/Color \
                       --only-properties)
    night_light_active=$(echo "${properties}" \
                             | grep 'NightLightActive' \
                             | cut -d= -f2)
    disabled_until_tomorrow=$(echo "${properties}" \
                                  | grep 'DisabledUntilTomorrow' \
                                  | cut -d= -f2)
    fixed_ls_colors="${LS_COLORS}"
    # prepend : if there is none in the begining of the string
    if [[ "${fixed_ls_colors:0:1}" != ':' ]]; then
        fixed_ls_colors=":${fixed_ls_colors}"
        drop_at_start=1
    else
        drop_at_start=0
    fi
    # append : if there is none in the end of the string
    if [[ "${fixed_ls_colors: -1}" != ':' ]]; then
        fixed_ls_colors="${fixed_ls_colors}:"
        drop_at_end=1
    else
        drop_at_end=0
    fi
    if [[ "${night_light_active}" =~ 'true' ]] && [[ "${disabled_until_tomorrow}" =~ 'false' ]]; then
        declare -n colors_source=overridden_colors
    else
        declare -n colors_source=original_colors
    fi
    declare -a sed_parameters
    declare color
    for var in "${!colors_source[@]}"; do
        color="${colors_source[$var]}"
        sed_parameters+=('-e' "s/\(:${var}=\)[^:]\+\(:\)/\1${color}\2/")
    done
    fixed_ls_colors="$(echo "${fixed_ls_colors}" | sed "${sed_parameters[@]}")"
    if [[ "${drop_at_start}" -eq 1 ]]; then
        fixed_ls_colors="${fixed_ls_colors:1}"
    fi
    if [[ "${drop_at_end}" -eq 1 ]]; then
        fixed_ls_colors="${fixed_ls_colors:0:-1}"
    fi
    export LS_COLORS="${fixed_ls_colors}"
}

setup_env_var PATH
setup_env_var PS1DATA
setup_env_var GOPATH
