#!/bin/bash
#set -x
set -euo pipefail

THIS=$(basename "${0}")

function info {
    printf '%s: %s\n' "${THIS}" "${*}"
}

function fail {
    info "${*}" >&2
    exit 1
}

function join_by() {
    local output_var_name delimiter first

    output_var_name=${1}; shift
    delimiter=${1}; shift
    first=${1-}
    if shift; then
        printf -v "${output_var_name}" '%s' "${first}" "${@/#/${delimiter}}";
    else
        local -n output_ref="${output_var_name}"
        # shellcheck disable=SC2034 # it's a reference to external variable
        output_ref=''
    fi
}

function clean_check {
    local -a ignore_v=(
        LINES
        COLUMNS
    )
    local grep_ignore
    join_by grep_ignore '\|' "${ignore_v[@]}"
    local old old_f new new_f v f
    old=$(declare -p | grep -e '^declare -' | awk '{print $3}' | sed -e 's/=.*//' | sort -u | grep -v -x "${grep_ignore[@]}")
    old_f=$(declare -pF | awk '{print $3}' | sort -u)
    "${@}"
    new=$(declare -p | grep -e '^declare -' | awk '{print $3}' | sed -e 's/=.*//' | sort -u | grep -v -x "${grep_ignore[@]}")
    new_f=$(declare -pF | awk '{print $3}' | sort -u)

    v=$(comm -1 -3 <(echo "${old}") <(echo "${new}"))
    if [[ -n ${v} ]]; then
        info "Leftover variables:"
        echo "${v}"
    fi
    f=$(comm -1 -3 <(echo "${old_f}") <(echo "${new_f}"))
    if [[ -n ${f} ]]; then
        info "Leftover functions:"
        echo "${f}"
    fi
    if [[ -n ${v} ]] || [[ -n ${f} ]]; then
        fail "${1} left some junk"
    fi
}

if [[ ${#} -eq 0 ]]; then
    fail "No command given, try 'help'"
fi

: "${KAVM_CONFIG_DIR:=${HOME}/.config/kavm}"
: "${KAVM_STATE_DIR:=${HOME}/.local/state/kavm}"

# Spec is a string with 8 colon separated fields:
# - long flag name (can be empty)
# - short flag name (can be empty)
# - whether takes argument (empty for no, nonempty for yes)
# - description of the value the flag takes (like <NAME>)
# - description of the flag
# - name of a variable that will store the flag value
# - default value for the variable (must be empty for flags with no argument)
# - whether empty value is allowed (empty for no, nonempty for yes)
#
# Empty long and short flags denote positional argument.
function process_specs {
    local -n flag_to_var=${1}; shift
    local -n flag_to_num=${1}; shift
    local -n var_to_def=${1}; shift
    local -n var_to_empty_allowed=${1}; shift
    local -n var_to_flags=${1}; shift
    local -n var_to_index=${1}; shift
    local -n index_to_var=${1}; shift
    local -n all_vars=${1}; shift
    local -n all_flags=${1}; shift
    local -n all_descs=${1}; shift
    local -n positional_names=${1}; shift

    flag_to_var=()
    flag_to_num=()
    var_to_def=()
    var_to_empty_allowed=()
    var_to_flags=()
    var_to_index=()
    index_to_var=()
    all_vars=()
    all_flags=()
    all_descs=()
    positional_names=()

    local -A flag_to_spec=() var_to_spec=()
    local -a fields

    local short long takes_arg param_desc flag_desc var_name default_value empty_allowed

    local prev_spec prev_def_val_def prev_def_val prev_def_val_spec prev_empty_allowed
    local prev_empty_allowed_def var_flags spec flag positional= positional_index=0
    for spec; do
        mapfile -t fields <<<"${spec//:/$'\n'}"
        if [[ ${#fields[@]} -ne 8 ]]; then
            fail "Invalid arg spec ${spec@Q}"
        fi
        long=$(post_split_unescape "${fields[0]}")
        short=$(post_split_unescape "${fields[1]}")
        takes_arg=$(post_split_unescape "${fields[2]}")
        param_desc=$(post_split_unescape "${fields[3]}")
        flag_desc=$(post_split_unescape "${fields[4]}")
        var_name=$(post_split_unescape "${fields[5]}")
        default_value=$(post_split_unescape "${fields[6]}")
        empty_allowed=$(post_split_unescape "${fields[7]}")
        if [[ -z ${short} ]] && [[ -z ${long} ]]; then
            positional=x
        fi
        if [[ -n ${positional} ]]; then
            if [[ -n ${short} ]] || [[ -n ${long} ]]; then
                fail "All flags should be defined before positional args"
            fi
            if [[ -z ${takes_arg} ]]; then
                fail "Positional argument marked as not taking value in spec ${spec@Q}"
            fi
        fi
        if [[ -n ${takes_arg} ]] && [[ -z ${param_desc} ]]; then
            if [[ -n ${positional} ]]; then
                fail "Empty parameter description for positional argument in spec ${spec@Q}"
            fi
            fail "Empty parameter description for flag taking an argument in spec ${spec@Q}"
        fi
        if [[ -z ${flag_desc} ]]; then
            if [[ -n ${positional} ]]; then
                fail "Empty positional argument description in spec ${spec@Q}"
            fi
            fail "Empty flag description in spec ${spec@Q}"
        fi
        if [[ -z ${var_name} ]]; then
            fail "Empty variable name in spec ${spec@Q}"
        fi
        if [[ -z ${takes_arg} ]] && [[ -n ${default_value} ]]; then
            fail "Non-empty default value ${default_value@Q} for boolean flag in spec ${spec@Q}"
        fi
        if [[ -z ${takes_arg} ]] && [[ -z ${empty_allowed} ]]; then
            fail "Empty value marked as not allowed for boolean flag in spec ${spec@Q}"
        fi
        all_vars+=( "${var_name}" )
        all_descs+=(
            "${param_desc}"
            "${flag_desc}"
            --
        )
        if [[ -n ${long} ]]; then
            flag="--${long}"
            prev_spec=${flag_to_spec["${flag}"]:-}
            if [[ -n ${prev_spec} ]]; then
                fail "Duplicated long flag ${flag@Q} in spec ${spec@Q}, defined earlier in spec ${prev_spec@Q}"
            fi
            flag_to_spec["${flag}"]=${spec}
            flag_to_var["${flag}"]=${var_name}
            flag_to_num["${flag}"]=${takes_arg}
            all_flags+=("${flag}")
            var_flags=${var_to_flags["${var_name}"]:-}
            if [[ -n ${var_flags} ]]; then
                var_flags+=", ${flag@Q}"
            else
                var_flags="${flag@Q}"
            fi
            var_to_flags["${var_name}"]=${var_flags}
        fi
        if [[ -n ${short} ]]; then
            flag="-${short}"
            prev_spec=${flag_to_spec["${flag}"]:-}
            if [[ -n ${prev_spec} ]]; then
                fail "Duplicated short flag -${short@Q} in spec ${spec@Q}, defined earlier in spec ${prev_spec@Q}"
            fi
            flag_to_spec["${flag}"]=${spec}
            flag_to_var["${flag}"]=${var_name}
            flag_to_num["${flag}"]=${takes_arg}
            all_flags+=("${flag}")
            var_flags=${var_to_flags["${var_name}"]:-}
            if [[ -n ${var_flags} ]]; then
                var_flags+=", ${flag@Q}"
            else
                var_flags="${flag@Q}"
            fi
            var_to_flags["${var_name}"]=${var_flags}
        fi
        if [[ -n ${positional} ]]; then
            var_to_index["${var_name}"]=${positional_index}
            index_to_var["${positional_index}"]=${var_name}
        else
            all_flags+=('--')
        fi
        prev_def_val_def=${var_to_def["${var_name}"]+defined}
        if [[ -n ${prev_def_val_def} ]]; then
            prev_def_val=${var_to_def["${var_name}"]}
            if [[ ${default_value} != "${prev_def_val}" ]]; then
                prev_def_val_spec=${var_to_spec["${var_name}"]}
                fail "Inconsistent default value ${default_value@Q} for variable ${var_name@Q} in spec ${spec@Q}, previously defined to be ${prev_def_val@Q} in spec ${prev_def_val_spec@Q}"
            fi
        fi
        prev_empty_allowed_def=${var_to_empty_allowed["${var_name}"]+defined}
        if [[ -n ${prev_empty_allowed_def} ]]; then
            prev_empty_allowed=${var_to_empty_allowed["${var_name}"]}
            if [[ ${empty_allowed:+x} != "${prev_empty_allowed:+x}" ]]; then
                prev_def_val_spec=${var_to_spec["${var_name}"]}
                fail "Inconsistent empty value allowance for variable ${var_name@Q} in spec ${spec@Q}, previously differently in spec ${prev_def_val_spec@Q}"
            fi
        fi
        var_to_empty_allowed["${var_name}"]=${empty_allowed}
        var_to_def["${var_name}"]=${default_value}
        var_to_spec["${var_name}"]=${spec}
        shift
        if [[ -n ${positional} ]]; then
            positional_index=$((positional_index + 1))
        fi
    done
}

function parse_args {
    local -A pa_flag_to_var pa_flag_to_num pa_var_to_def pa_var_to_empty_allowed pa_var_to_flags pa_var_to_index pa_index_to_var
    local -a pa_all_vars pa_all_flag pa_all_flags pa_all_descs pa_positional_names
    local -a specs=()

    while [[ ${1:-'--'} != '--' ]]; do
        specs+=( "${1}" )
        shift
    done
    shift

    clean_check process_specs pa_flag_to_var pa_flag_to_num pa_var_to_def pa_var_to_empty_allowed pa_var_to_flags pa_var_to_index pa_index_to_var pa_all_vars pa_all_flags pa_all_descs pa_positional_names "${specs[@]}"

    local -A assigned_vars=() var_to_used_flag=() var_to_used_index=()
    local var_name
    while [[ ${#} -gt 0 ]]; do
        if [[ ${1} = '--' ]]; then
            break
        fi
        var_name=${pa_flag_to_var["${1}"]:-}
        if [[ -z ${var_name} ]]; then
            if [[ ${var_name} = -* ]]; then
                fail "Unknown flag ${1@Q}"
            else
                break
            fi
        fi
        var_to_used_flag["${var_name}"]=${1}
        local -n var=${var_name}
        if [[ -z ${pa_flag_to_num["${1}"]} ]]; then
            var=x
            shift
        else
            if [[ ${#} -lt 2 ]]; then
                fail "Missing arg for ${1@Q}"
            fi
            var=${2}
            shift 2
        fi
        unset -n var
        assigned_vars["${var_name}"]=x
    done
    local arg index=0
    for arg; do
        var_name=${pa_index_to_var["${index}"]:-}
        if [[ -z ${var_name} ]]; then
            fail "Superfluous positional argument ${arg@Q}"
        fi
        local -n var=${var_name}
        var=${arg}
        unset -n var
        assigned_vars["${var_name}"]=x
        var_to_used_index["${var_name}"]=${index}
        index=$((index + 1))
    done
    for var_name in "${pa_all_vars[@]}"; do
        if [[ -z ${assigned_vars["${var_name}"]:-} ]]; then
            local -n var=${var_name}
            var=${pa_var_to_def["${var_name}"]}
            unset -n var
        fi
    done
    local flag_name flags_for_var
    for var_name in "${pa_all_vars[@]}"; do
        local -n var=${var_name}
        if [[ -z ${var} ]] && [[ -z ${pa_var_to_empty_allowed["${var_name}"]:-} ]]; then
            flag_name=${var_to_used_flag["${var_name}"]:-}
            index=${var_to_used_index["${var_name}"]:-}
            if [[ -n ${index} ]]; then
                fail "Must pass non-empty value to positional argument ${index@Q}"
            fi
            if [[ -n ${flag_name} ]]; then
                fail "Must pass non-empty value to ${flag_name@Q}"
            fi
            index=${pa_var_to_index["${var_name}"]:-}
            if [[ -n ${index} ]]; then
                fail "Default value for positional argument ${index@Q} is empty, but the value can't be empty - pass a non-empty value"
            fi
            flags_for_var=${pa_var_to_flags["${var_name}"]}
            fail "Default value for flags ${flags_for_var} is empty, but the value can't be empty - use one of listed flags to specify a valid value"
        fi
        unset -n var
    done
}

function print_help {
    local text=${1}; shift
    # rest are specs

    local -A ph_flag_to_var ph_flag_to_num ph_var_to_def ph_var_to_empty_allowed ph_var_to_flags ph_var_to_index ph_index_to_var
    local -a ph_all_vars ph_all_flags ph_all_descs ph_positional_names

    clean_check process_specs ph_flag_to_var ph_flag_to_num ph_var_to_def ph_var_to_empty_allowed ph_var_to_flags ph_var_to_index ph_index_to_var ph_all_vars ph_all_flags ph_all_descs ph_positional_names "${@}"

    printf '%s %s\n%s\n' "${THIS}" "${KAVM_CMD}" "${text}"

    if [[ ${#ph_all_flags[@]} -gt 0 ]]; then
        printf '\nFlags:\n'
    fi
    local flags_line
    local param_desc flag_desc
    while [[ ${#ph_all_descs[@]} -gt 2 ]] && [[ ${#ph_all_flags[@]} -gt 1 ]]; do
        param_desc=${ph_all_descs[0]}
        flag_desc=${ph_all_descs[1]}
        ph_all_descs=( "${ph_all_descs[@]:3}" )
        flags_line="${ph_all_flags[0]}${param_desc:+ ${param_desc}}"
        if [[ ${ph_all_flags[1]} == '--' ]]; then
            ph_all_flags=( "${ph_all_flags[@]:2}" )
        else
            flags_line+=", ${ph_all_flags[1]}${param_desc:+ ${param_desc}}"
            ph_all_flags=( "${ph_all_flags[@]:3}" )
        fi
        # TODO: print default values
        printf '    %s\n        %s\n' "${flags_line}" "${flag_desc}"
    done
    if [[ ${#ph_all_descs[@]} -gt 0 ]]; then
        printf '\nPositional:\n'
    fi
    while [[ ${#ph_all_descs[@]} -gt 2 ]]; do
        param_desc=${ph_all_descs[0]}
        flag_desc=${ph_all_descs[1]}
        ph_all_descs=( "${ph_all_descs[@]:3}" )
        # TODO: print default values
        printf '    %s\n        %s\n' "${param_desc}" "${flag_desc}"
    done
}

function ensure_exists {
    local cmd=${1}; shift

    func="kavm_cmd_${cmd}"
    if ! declare -pF "${func}" &>/dev/null; then
        fail "Unknown command ${cmd#Q}, try 'help'"
    fi
}

function kavm_cmd_help {
    local -a specs=(
        '::x:<COMMAND>:A command:cmd::x'
    )
    if [[ -n ${KAVM_HELP:-} ]]; then
        clean_check print_help 'Prints help.' "${specs[@]}"
        return 0
    fi
    local cmd
    clean_check parse_args "${specs[@]}" -- "${@}"
    local func
    if [[ -n "${cmd}" ]]; then
        ensure_exists "${cmd}"
        func="kavm_cmd_${cmd}"
        KAVM_HELP=x
        KAVM_CMD=${cmd}
        clean_check "${func}"
        unset KAVM_HELP
        return 0
    fi
    local -a cmds
    mapfile -t cmds < <(declare -F | cut -d' ' -f3 | grep '^kavm_cmd_' | sed -e 's/kavm_cmd_//' -e 's/_/-/' | sort)

    printf 'Available commands:\n'
    printf '  %s\n' "${cmds[@]}"
    printf '\nPass command name too to get help for it.\n'
}

function kavm_cmd_defaults {
    local -a specs=(
        'subscription:s:x:<ID>:Set default subscription:subscription::x'
        'resource-group:g:x:<NAME>:Set default name for resource group:rg_name::x'
        'location:l:x:<NAME>:Set default Azure Region:location::x'
        'image:i:x:<URN>:Set default URN of the OS image:image::x'
        'vm:v:x:<NAME>:Set default name of a VM:vm_name::x'
        'size:S:x:<TYPE>:Set default machine type:size:Standard_D8ds_v4:x'
        "ip-sku:I:x:<TYPE>:Set default IP SKU type:ip_sku:Standard:x"
        'jump:j:x:<USER>@<IP>:Set default user and IP for jump SSH server:jump::x'
        'user:u:x:<NAME>:Set default user:user::x'
    )
    if [[ -n ${KAVM_HELP:-} ]]; then
        print_help 'Sets defaults for other commands.' "${specs[@]}"
        return 0
    fi

    local subscription rg_name location image vm_name ip_sku size jump user
    parse_args "${specs[@]}" -- "${@}"

    local defaults_file="${KAVM_CONFIG_DIR}/defaults"
    if [[ -e "${defaults_file}" ]]; then
        source "${defaults_file}"
    fi

    local dsub=${subscription:-${KAVM_DEFAULT_SUBSCRIPTION:-}}
    local drg=${rg_name:-${KAVM_DEFAULT_RG:-}}
    local dl=${location:-${KAVM_DEFAULT_LOCATION:-}}
    local dimage=${image:-${KAVM_DEFAULT_IMAGE_URN:-}}
    local dvm=${vm_name:-${KAVM_DEFAULT_VM:-}}
    local dip=${ip_sku:-${KAVM_DEFAULT_IP_SKU:-}}
    local dsize=${size:-${KAVM_DEFAULT_SIZE:-}}
    local djump=${jump:-${KAVM_DEFAULT_JUMP:-}}
    local duser=${user:-${KAVM_DEFAULT_USER:-}}
    mkdir -p "${KAVM_CONFIG_DIR}"
    cat <<EOF >"${defaults_file}"
local KAVM_DEFAULT_SUBSCRIPTION=${dsub@Q}
local KAVM_DEFAULT_RG=${drg@Q}
local KAVM_DEFAULT_LOCATION=${dl@Q}
local KAVM_DEFAULT_IMAGE_URN=${dimage@Q}
local KAVM_DEFAULT_VM=${dvm@Q}
local KAVM_DEFAULT_IP_SKU=${dip@Q}
local KAVM_DEFAULT_SIZE=${dsize@Q}
local KAVM_DEFAULT_JUMP=${djump@Q}
local KAVM_DEFAULT_USER=${duser@Q}
EOF
}

function pre_split_escape {
    local escaped=${1}; shift
    escaped=${escaped//':'/__KAVM_ESCAPED_COLON__}
    printf '%s' "${escaped}"
}

function pre_split_escape_var {
    pre_split_escape "${!1:-}"
}

function post_split_unescape {
    local unescaped=${1}; shift
    unescaped=${unescaped//__KAVM_ESCAPED_COLON__/:}
    printf '%s' "${unescaped}"
}

function kavm_cmd_create {
    local defaults_file="${KAVM_CONFIG_DIR}/defaults"
    if [[ -e "${defaults_file}" ]]; then
        source "${defaults_file}"
    fi

    local -a specs=(
        "subscription:s:x:<ID>:Subscription:subscription:$(pre_split_escape_var KAVM_DEFAULT_SUBSCRIPTION):"
        "resource-group:g:x:<NAME>:Name of the Azure Resource Group:rg_name:$(pre_split_escape_var KAVM_DEFAULT_RG):"
        "location:l:x:<NAME>:Name of the Azure Region:location:$(pre_split_escape_var KAVM_DEFAULT_LOCATION):"
        "image:i:x:<URN>:URN of the OS image:image:$(pre_split_escape_var KAVM_DEFAULT_IMAGE_URN):"
        "user:u:x:<NAME>:Name of the admin account:admin:$(pre_split_escape_var KAVM_DEFAULT_USER):"
        "ssh:h:x:<PATH>:Path to the public key file:ssh_path:~/.ssh/id_rsa.pub:"
        "ip-sku:I:x:<TYPE>:IP SKU type:ip_sku:$(pre_split_escape_var KAVM_DEFAULT_IP_SKU):"
        "size:S:x:<TYPE>:Machine type:size:$(pre_split_escape_var KAVM_DEFAULT_SIZE):"
        "vm:v:x:<NAME>:Name of a VM:vm_name:$(pre_split_escape_var KAVM_DEFAULT_VM):"
    )
    if [[ -n ${KAVM_HELP:-} ]]; then
        print_help 'Creates a VM.' "${specs[@]}"
        return 0
    fi

    local subscription rg_name location vnet_name subnet_name image admin ssh_path ip_sku size vm_name
    parse_args "${specs[@]}" -- "${@}"

    mkdir -p "${KAVM_STATE_DIR}"

    local f
    for f in resource-group vm; do
        if [[ -s "${KAVM_STATE_DIR}/${f}" ]]; then
            fail "There is a ${f@Q} file saved in state dir (${KAVM_STATE_DIR@Q}), looks like you have a vm set up already"
        fi
    done

    info "Looking for jenkins VNet in ${location@Q}"
    local jenkins_vnet_name jenkins_vnet_json
    jenkins_vnet_name="jenkins-vnet-${location}"
    jenkins_vnet_json=$(az network vnet list --subscription "${subscription}" | jq '.[] | select(.name == "'"${jenkins_vnet_name}"'")')
    if [[ -z "${jenkins_vnet_json}" ]]; then
        fail "No jenkins VNet available for region ${location@Q}"
    fi
    local jenkins_subnet_id
    jenkins_subnet_id=$(jq -r '.subnets[0].id' <<<"${jenkins_vnet_json}")
    if [[ -z ${jenkins_subnet_id} ]]; then
        fail "The ${jenkins_vnet_name} VNet has no subnets?"
    fi

    info "Creating resource group ${rg_name@Q}"
    az group create \
       --subscription "${subscription}" \
       --name "${rg_name}" \
       --location "${location}" >"${KAVM_STATE_DIR}/resource-group"

    info "Creating VM ${vm_name@Q}"
    az vm create \
       --subscription "${subscription}" \
       --resource-group "${rg_name}" \
       --name "${vm_name}" \
       --image "${image}" \
       --admin-username "${admin}" \
       --ssh-key-value "${ssh_path}" \
       --public-ip-sku "${ip_sku}" \
       --subnet "${jenkins_subnet_id}" \
       --size "${size}" >"${KAVM_STATE_DIR}/vm"
}

function kavm_cmd_delete {
    if [[ -n ${KAVM_HELP:-} ]]; then
        print_help 'Destroys a VM.'
        return 0
    fi

    local vm_id priv_ip pub_ip
    if [[ -s "${KAVM_STATE_DIR}/vm" ]]; then
        vm_id=$(jq -r '.id' <"${KAVM_STATE_DIR}/vm")
        priv_ip=$(jq -r '.privateIpAddress' <"${KAVM_STATE_DIR}/vm")
        pub_ip=$(jq -r '.publicIpAddress' <"${KAVM_STATE_DIR}/vm")
        info "Destroying VM ${vm_name@Q}"
        az vm delete --yes --ids "${vm_id}"
        info 'Removing VM IP addresses from known hosts'
        ssh-keygen -f ~/.ssh/known_hosts -R "${priv_ip}"
        ssh-keygen -f ~/.ssh/known_hosts -R "${pub_ip}"
        rm -f "${KAVM_STATE_DIR}/vm"
    fi

    local rg_name subscription
    if [[ -s "${KAVM_STATE_DIR}/resource-group" ]]; then
        subscription=$(jq -r '.id' <"${KAVM_STATE_DIR}/resource-group")
        subscription=${subscription##/subscriptions/}
        subscription=${subscription%%/*}
        rg_name=$(jq -r '.name' <"${KAVM_STATE_DIR}/resource-group")
        info "Destroying resource group ${rg_name@Q}"
        az group delete --subscription "${subscription}" --name "${rg_name}" --yes
        rm -f "${KAVM_STATE_DIR}/resource-group"
    fi
}

function kavm_ssh_setup_set {
    local user=${1}; shift
    local jump=''
    if [[ ${#} -gt 0 ]]; then
        jump=${1}; shift
    fi
    local -a ssh_opts=()
    if [[ -n ${jump} ]]; then
        ssh_opts+=( -J "${jump}" )
    fi
    if [[ ! -s "${KAVM_STATE_DIR}/vm" ]]; then
        fail "No known VM"
    fi
    local ip
    ip=$(jq -r '.privateIpAddress' <"${KAVM_STATE_DIR}/vm")

    declare -g KAVM_SSH_SETUP_USER=${user} KAVM_SSH_SETUP_IP=${ip}
    declare -g -a KAVM_SSH_OPTS=( "${ssh_opts[@]}" )
}

function kavm_ssh_setup_unset {
    unset KAVM_SSH_SETUP_USER KAVM_SSH_SETUP_IP KAVM_SSH_OPTS
}

function kavm_ssh {
    ssh "${KAVM_SSH_OPTS[@]}" -A "${KAVM_SSH_SETUP_USER}@${KAVM_SSH_SETUP_IP}" "${@}"
}

function kavm_scp {
    scp "${KAVM_SSH_OPTS[@]}" "${@/#'(VM)'/"${KAVM_SSH_SETUP_USER}@${KAVM_SSH_SETUP_IP}:"}"
}

function kavm_cmd_prepare {
    local defaults_file="${KAVM_CONFIG_DIR}/defaults"
    if [[ -e "${defaults_file}" ]]; then
        source "${defaults_file}"
    fi

    local -a specs=(
        "jump:j:x:<USER@IP>:Jump server to use:jump:$(pre_split_escape_var KAVM_DEFAULT_JUMP):x"
        "user:u:x:<NAME>:User name:user:$(pre_split_escape_var KAVM_DEFAULT_USER):"
    )
    if [[ -n ${KAVM_HELP:-} ]]; then
        print_help 'Prepares the newly created VM.' "${specs[@]}"
        return 0
    fi

    local jump user
    parse_args "${specs[@]}" -- "${@}"
    kavm_ssh_setup_set "${user}" "${jump}"
    local temp_script_root temp_script_user
    temp_script_root=$(mktemp)
    temp_script_user=$(mktemp)
    cat <<EOF >"${temp_script_root}"
#/bin/bash

set -euo pipefail
USER=${user@Q}
EOF
    cat <<'EOF' >>"${temp_script_root}"
echo 'Invoking apt update'
apt update
echo 'Invoking apt upgrade'
apt upgrade -y
echo 'Installing docker and jq'
apt install -y docker.io jq
echo "Adding ${USER} to docker group"
adduser "${USER}" docker
disk=$(df | tail --lines +2 | awk '{print $2" "$6}' | sort --key=1nr | head --lines 1 | awk '{print $2}')
echo "Configuring docker to use ${disk} for storage"
echo '{ "data-root": "'"${disk}"'/docker" }' >/etc/docker/daemon.json
echo "Restarting docker"
systemctl restart docker
mkdir -p "${disk}/work"
echo "Changing ownership of ${disk}/work to ${USER}"
chown "${USER}:${USER}" "${disk}/work"
EOF

    cat <<'EOF' >"${temp_script_user}"
#/bin/bash

set -euo pipefail
disk=$(df | tail --lines +2 | awk '{print $2" "$6}' | sort --key=1nr | head --lines 1 | awk '{print $2}')
echo 'Adding github.com to known hosts'
mkdir -p ~/.ssh
ssh-keyscan -t rsa github.com >>~/.ssh/known_hosts
echo 'Cloning and setting up my env mutators'
cd "${disk}/work"
git clone git@github.com:krnowak/my-env-mutators.git ajwaj
branch=$(git -C ajwaj rev-parse --abbrev-ref HEAD)
mkdir -p my-env-mutators
mv ajwaj "my-env-mutators/${branch}"
ln -sf "${PWD}/my-env-mutators/${branch}/bash_aliases.sh.inc" "${HOME}/.bash_aliases"
shopt -s expand_aliases
source my-env-mutators/${branch}/bash_aliases.sh.inc
echo 'Cloning scripts'
kgit clone git@github.com:flatcar/scripts.git
EOF
    chmod a+x "${temp_script_root}" "${temp_script_user}"
    info 'Copying prepare-root.sh'
    kavm_scp "${temp_script_root}" "(VM)/home/${user}/prepare-root.sh"
    info 'Copying prepare-user.sh'
    kavm_scp "${temp_script_user}" "(VM)/home/${user}/prepare-user.sh"
    rm -f "${temp_script_root}" "${temp_script_user}"
    info 'Invoking prepare-root.sh on VM'
    kavm_ssh sudo bash "/home/${user}/prepare-root.sh"
    info 'Invoking prepare-user.sh on VM'
    kavm_ssh bash "/home/${user}/prepare-user.sh"
    info 'Preparing done, removing the scripts'
    kavm_ssh rm -f "/home/${user}/prepare-root.sh" "/home/${user}/prepare-user.sh"
    info 'Rebooting VM'
    kavm_ssh sudo systemctl reboot
    kavm_ssh_setup_unset
}

function kavm_cmd_ssh {
    local defaults_file="${KAVM_CONFIG_DIR}/defaults"
    if [[ -e "${defaults_file}" ]]; then
        source "${defaults_file}"
    fi

    local -a specs=(
        "jump:j:x:<USER@IP>:Jump server to use:jump:$(pre_split_escape_var KAVM_DEFAULT_JUMP):x"
        "user:u:x:<NAME>:User name:user:$(pre_split_escape_var KAVM_DEFAULT_USER):"
    )
    if [[ -n ${KAVM_HELP:-} ]]; then
        print_help 'SSH into the VM.' "${specs[@]}"
        return 0
    fi

    local jump user
    parse_args "${specs[@]}" -- "${@}"
    kavm_ssh_setup_set "${user}" "${jump}"
    kavm_ssh
    kavm_ssh_setup_unset
}

function kavm_cmd_scp {
    local defaults_file="${KAVM_CONFIG_DIR}/defaults"
    if [[ -e "${defaults_file}" ]]; then
        source "${defaults_file}"
    fi

    local -a specs=(
        "jump:j:x:<USER@IP>:Jump server to use:jump:$(pre_split_escape_var KAVM_DEFAULT_JUMP):x"
        "user:u:x:<NAME>:User name:user:$(pre_split_escape_var KAVM_DEFAULT_USER):"
        '::x:<SOURCE>:Source path, needs to be prefixed with "(VM)" if it is a remote location:src::'
        '::x:<DESTINATION>:Destination path, needs to be prefixed with "(VM)" if it is a remote location:dest::'
    )
    if [[ -n ${KAVM_HELP:-} ]]; then
        print_help 'SSH into the VM.' "${specs[@]}"
        return 0
    fi

    local jump user src dest
    parse_args "${specs[@]}" -- "${@}"
    kavm_ssh_setup_set "${user}" "${jump}"
    kavm_scp "${src}" "${dest}"
    kavm_ssh_setup_unset
}

cmd=${1}; shift
func="kavm_cmd_${cmd}"

ensure_exists "${cmd}"
KAVM_CMD="${cmd}"

clean_check "${func}" "${@}"
