#!/bin/bash

kgit_msg2() {
    echo "kgit: ${*}" >&2
}

kgit_msg2_lines() {
    printf 'kgit: %s\n' "${@}" >&2
}

kgit_call() {
    kgit_msg2 "Invoking ${*@Q}"
    "${@}"
}

kgit_req_params_eq() {
    local num=${1}; shift

    [[ ${#} -eq "${num}" ]]
}

kgit_req_params_ge() {
    local num=${1}; shift

    [[ ${#} -ge "${num}" ]]
}

kgit_req_params_le() {
    local num=${1}; shift

    [[ ${#} -le "${num}" ]]
}

kgit_proc_sub_begin() {
    local -n state_map=${1}; shift
    # rest are commands to execute
    local fd pid

    exec {fd}< <("${@}")
    pid=$!
    state_map['fd']=${fd}
    state_map['pid']=${pid}
}

kgit_proc_sub_end() {
    local -n state_map=${1}; shift
    local fd pid
    fd=${state_map['fd']}
    pid=${state_map['pid']}

    exec {fd}<&-
    wait "${pid}"
}

kgit_proc_sub_to_array() {
    local array_name=${1}; shift
    # rest are commands to execute

    local -A kpsta_state_map=()
    kgit_proc_sub_begin kpsta_state_map "${@}"
    local fd=${kpsta_state_map['fd']}
    mapfile -t "${array_name}" <&"${fd}"
    kgit_proc_sub_end kpsta_state_map
}

kgit_cmd_fetch() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd}"
        kgit_msg2 '    forwards args to "git fetch -pPtf"'
        return 0
    fi
    kgit_req_params_eq 0 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes no arguments"
        return 1
    }
    local -a kcf_remotes
    kgit_proc_sub_to_array kcf_remotes "${kgit_exec[@]}" remote || return 1
    local remote origin_idx=0
    for remote in "${kcf_remotes[@]}"; do
        if [[ ${remote} = 'origin' ]]; then
            break
        fi
        origin_idx=$((origin_idx + 1))
    done
    if [[ ${origin_idx} -lt ${#kcf_remotes[@]} ]]; then
        "${kgit_exec[@]}" fetch -pPtf origin || return 1
        unset "kcf_remotes[${origin_idx}]"
    fi
    for remote in "${kcf_remotes[@]}"; do
        "${kgit_exec[@]}" fetch -ptf "${remote}" || return 1
    done
}

kgit_cmd_wt() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd}"
        kgit_msg2 '    lists available worktrees'
        return 0
    fi
    kgit_req_params_eq 0 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes no arguments"
        return 1
    }
    "${kgit_exec[@]}" worktree list || return 1
}

kgit_find_dir_with_dot_git() {
    local -n out_dir_ref=${1}; shift

    if [[ -e ./.git ]]; then
        out_dir_ref=.
        return 0
    fi
    local path=..

    while [[ ! ${path} -ef / ]]; do
        if [[ -e ${path}/.git ]]; then
            out_dir_ref=${path}
            return 0
        fi
        path+=/..
    done
    kgit_msg2 "we are not in a git repo, aren't we?" >&2
    return 1
}

kgit_cmd_wt_add() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} [<flags>] [--] <name>"
        kgit_msg2 '    creates a branch krnowak/<name>, creates a worktree at location neighbouring the current worktree with passed name and cds to it'
        kgit_msg2 '    flags:'
        kgit_msg2 '    -u <remote>       upstream name used for tracking (defaults to origin)'
        kgit_msg2 '    -s                track upstream branch with the same name'
        kgit_msg2 '    -o <other_name>   track upstream branch <other_name>'
        kgit_msg2 '    -l <local_name>   base on <local_name> branch instead of current branch'
        return 0
    fi
    local top_dir
    kgit_find_dir_with_dot_git top_dir || return 1
    local u=origin
    local ub track_same track_other use_local
    while [[ ${#} -gt 0 ]]; do
        case ${1} in
            -u)
                u=${2}
                shift 2
                ;;
            -s)
                if [[ -n ${track_other:-} || -n ${use_local:-} ]]; then
                    kgit_msg2 '-s, -o and -l are mutually exclusive'
                    return 1
                fi
                track_same=x
                shift
                ;;
            -o)
                if [[ -n ${track_same:-} || -n ${use_local:-} ]]; then
                    kgit_msg2 '-s, -o and -l are mutually exclusive'
                    return 1
                fi
                track_other=x
                ub=${2}
                shift 2
                ;;
            -l)
                if [[ -n ${track_same:-} || -n ${track_other:-} ]]; then
                    kgit_msg2 '-s, -o and -l are mutually exclusive'
                    return 1
                fi
                use_local=x
                ub=${2}
                shift 2
                ;;
            --)
                shift
                break
                ;;
            -*)
                kgit_msg2 "unknown flag ${1@Q}"
                return 1
                ;;
            *)
                break
                ;;
        esac
    done
    kgit_req_params_ge 1 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes at least one argument, a base name"
        return 1
    }
    local name=${1}; shift
    local new_dir=${top_dir}/../${name}
    if [[ -n ${track_same} ]]; then
        ub=krnowak/${name}
    fi
    local -a args=(
        worktree add -b "krnowak/${name}" "${new_dir}"
    )
    if [[ -n ${track_same:-} || -n ${track_other:-} ]]; then
        args+=("${u}/${ub}")
    fi
    if [[ -n ${use_local} ]]; then
        args+=("${ub}")
    fi
    "${kgit_exec[@]}" "${args[@]}" || return 1
    kgit_call cd "${new_dir}" || return 1
}

kgit_cmd_wt_remove() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} <name> [<name2> [<name3> [...]]]"
        kgit_msg2 '    deletes worktrees at ../<name> and deletes a branch krnowak/<name> for all passed names'
        return 0
    fi
    kgit_req_params_ge 1 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes at least one argument, a base name of a worktree"
        return 1
    }
    local top_dir
    kgit_find_dir_with_dot_git top_dir || return 1
    local uid
    uid=$(kgit_call id -u)
    local name
    local -a foreign_files failures
    failures=()
    for name; do
        # In theory, the files not owned by us could be still removed if
        # they are group-writable and we are a part of this group, or if
        # they are world-writable. These are niche cases anyway.
        mapfile -t foreign_files < <(kgit_call find "${top_dir}/../${name}" ! -uid "${uid}" -printf '%p (owned by %u, uid: %U)\n')
        if [[ ${#foreign_files[@]} -gt 0 ]]; then
            kgit_msg2_lines \
                "Worktree ${name} has files/directories not owned by us:" \
                "${foreign_files[@]/#/    }" \
                '' \
                'Remove them or change their ownership, "git worktree remove" handles this badly.'
            failures+=( "${name} (foreign files)" )
            continue
        fi
        if ! "${kgit_exec[@]}" worktree remove --force "${top_dir}/../${name}"; then
            failures+=( "${name} (failed to remove worktree)" )
            continue
        fi
        if ! "${kgit_exec[@]}" branch -D "krnowak/${name}"; then
            failures+=( "${name} (failed to remove branch)" )
            continue
        fi
    done
    if [[ ${#failures[@]} -gt 0 ]]; then
        kgit_msg2_lines \
            '' \
            '' \
            'Failed to remove the following worktrees:' \
            "${failures[@]/#/    }"
        return 1
    fi
    return 0
}

kgit_cmd_amend_head() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd}"
        kgit_msg2 '    amends the commit without editing'
        return 0
    fi
    kgit_req_params_eq 0 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes no arguments"
        return 1
    }
    "${kgit_exec[@]}" commit --amend --no-edit || return 1
}

kgit_cmd_merge() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd}"
        kgit_msg2 '    merges changes from upstream branch to current branch'
        return 0
    fi
    kgit_req_params_eq 0 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes no arguments"
        return 1
    }
    local branch remote
    branch=$("${kgit_exec[@]}" branch --show-current) || return 1
    remote=$("${kgit_exec[@]}" config "branch.${branch}.remote") || {
        kgit_msg2 "Branch ${branch} is not a tracking branch"
        return 1
    }
    "${kgit_exec[@]}" merge --ff-only "${remote}/${branch}" || return 1
    "${kgit_exec[@]}" submodule update || return 1
}

kgit_cmd_checkout() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} <branch>"
        kgit_msg2 '    checks out <branch> and updates submodules'
        return 0
    fi
    kgit_req_params_eq 1 "${0}" || {
        kgit_msg2 "'${kgit_cmd}' takes one argument, a branch name"
        return 1
    }
    local branch=${1}; shift
    "${kgit_exec[@]}" checkout "${branch}" || return 1
    "${kgit_exec[@]}" submodule update || return 1
}

kgit_cmd_fpush() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} [args…]"
        kgit_msg2 '    does a force-with-lease push, forwards args to git push'
        return 0
    fi
    "${kgit_exec[@]}" push --force-with-lease "${@}" || return 1
}

kgit_cmd_clone() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} <url>"
        kgit_msg2 '    clones a repo to <basename of <url> >/<default branch> and cds to it'
        return 0
    fi
    kgit_req_params_eq 1 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' needs one argument, a repository URL"
        return 1
    }
    local repo=${1}; shift
    local default_branch dir disable_extglob
    default_branch=$("${kgit_exec[@]}" ls-remote --symref "${repo}" HEAD | grep '^ref:[[:space:]].*[[:space:]]HEAD$' | sed -e 's#^.*refs/heads/\([^[:space:]]*\)[[:space:]]*HEAD$#\1#')
    kgit_msg2 "Default branch is ${default_branch}"
    dir=${repo}
    disable_extglob=''
    shopt -q extglob || { disable_extglob='x'; shopt -s extglob; }
    dir=${dir%%+(/)}
    [[ -z ${disable_extglob} ]] || shopt -u extglob
    dir=${dir##*/}
    dir=${dir%.git}
    dir="${dir}/${default_branch}"
    "${kgit_exec[@]}" clone "${repo}" "${dir}" || return 1
    kgit_call cd "${dir}" || return 1
}

kgit_cmd_fresh() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd}"
        kgit_msg2 '    nukes the changes'
        return 0
    fi
    kgit_req_params_eq 0 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes no arguments"
        return 1
    }
    "${kgit_exec[@]}" reset --hard HEAD || return 1
    "${kgit_exec[@]}" clean -ffdx || return 1
}

kgit_cmd_st() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd}"
        kgit_msg2 '    prints the status'
        return 0
    fi
    kgit_req_params_eq 0 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes no arguments"
        return 1
    }
    "${kgit_exec[@]}" status || return 1
}

kgit_list_cmds() {
    kgit_msg2 "Available commands:"
    declare -F | cut -d' ' -f3 | grep '^kgit_cmd_' | sed -e 's/kgit_cmd_//' -e 's/_/-/' | sort || return 1
}

kgit_cmd_help() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} [command]"
        kgit_msg2 '    prints available commands or a command help'
        return 0
    fi
    kgit_req_params_le 1 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes at most 1 argument, a command name"
        return 1
    }
    if [[ ${#} -gt 0 ]]; then
        local kgit_cmd=${1}
        shift
        local func="kgit_cmd_${kgit_cmd//-/_}"

        if [[ $(type -t "${func}") = 'function' ]]; then
            KGIT_HELP=x "${func}"
        else
            kgit_msg2 "No such command: '${kgit_cmd}'"
            kgit_list_cmds || return 1
        fi
    else
        kgit_list_cmds || return 1
    fi
}

kgit_main() {
    local -a kgit_extra_opts=()
    if [[ ${#} -gt 0 ]] && [[ ${1} = '-C' ]]; then
        kgit_extra_opts+=( '-C' "$(realpath "${2}")" )
        shift 2
    fi

    if [[ ${#} -eq 0 ]]; then
        kgit_msg2 'No command given'
        return 1
    fi

    local kgit_exec kgit_cmd func
    kgit_exec=('kgit_call' 'git' "${kgit_extra_opts[@]}")
    kgit_cmd=${1}; shift
    func="kgit_cmd_${kgit_cmd//-/_}"

    if [[ $(type -t "${func}") = 'function' ]]; then
        "${func}" "${@}" || return 1
    else
        kgit_msg2 "Unknown command '${kgit_cmd}'" || return 1
    fi
    return 0
}

mapfile -t kgit_funcs < <(declare -F | cut -d' ' -f3 | grep '^kgit_')
if kgit_main "${@}"; then
    unset -f "${kgit_funcs[@]}"
    unset -v "${!kgit_@}"
    return 0
else
    unset -f "${kgit_funcs[@]}"
    unset -v "${!kgit_@}"
    return 1
fi
