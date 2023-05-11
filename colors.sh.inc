function setup_ls_colors {
    declare -g -A overridden_colors original_colors
    local fixed_ls_colors var original_color

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
        # shellcheck disable=SC2034
        original_colors["${var}"]="${original_color}"
    done
}

function adapt_to_night_light {
    local \
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
    if [[ "${fixed_ls_colors}" != ':'* ]]; then
        fixed_ls_colors=":${fixed_ls_colors}"
        drop_at_start=x
    fi
    # append : if there is none in the end of the string
    if [[ "${fixed_ls_colors}" != *':' ]]; then
        fixed_ls_colors="${fixed_ls_colors}:"
        drop_at_end=x
    fi
    local source_name='original_colors'
    if [[ "${night_light_active}" = *'true'* ]] && [[ "${disabled_until_tomorrow}" = *'false'* ]]; then
        source_name='overridden_colors'
    fi
    local -n colors_source="${source_name}"
    local -a sed_parameters
    local color var
    for var in "${!colors_source[@]}"; do
        color="${colors_source[$var]}"
        sed_parameters+=('-e' "s/\(:${var}=\)[^:]\+\(:\)/\1${color}\2/")
    done
    fixed_ls_colors=$(echo "${fixed_ls_colors}" | sed "${sed_parameters[@]}")
    if [[ -n "${drop_at_start}" ]]; then
        fixed_ls_colors="${fixed_ls_colors:1}"
    fi
    if [[ -n "${drop_at_end}" ]]; then
        fixed_ls_colors="${fixed_ls_colors:0:-1}"
    fi
    export LS_COLORS="${fixed_ls_colors}"
}
