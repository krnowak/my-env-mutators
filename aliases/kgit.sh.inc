#!/bin/bash

kgit_msg2() {
    echo "kgit: ${*}" >&2
}

kgit_call() {
    kgit_msg2 "Invoking '${*}'"
    "${@}"
}

kgit_req_params_eq() {
    local num
    num=${1}; shift

    [[ ${#} -eq "${num}" ]]
}

kgit_req_params_ge() {
    local num
    num=${1}; shift

    [[ ${#} -ge "${num}" ]]
}

kgit_req_params_le() {
    local num
    num=${1}; shift

    [[ ${#} -le "${num}" ]]
}

kgit_cmd_fetch() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} [args…]"
        kgit_msg2 '    forwards args to "git fetch -pPtf"'
        return 0
    fi
    "${kgit_exec[@]}" fetch -pPtf "${@}" || return 1
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

kgit_cmd_wt_add() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} <name> [args…]"
        kgit_msg2 '    creates a branch krnowak/name, creates a worktree at ../<name> (forwards the args here) and cds to it'
        return 0
    fi
    kgit_req_params_ge 1 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes at least one argument, a base name, rest is forwarded to 'git worktree add'"
        return 1
    }
    local name
    name=${1}; shift
    "${kgit_exec[@]}" worktree add -b "krnowak/${name}" "../${name}" "${@}" || return 1
    kgit_call cd "../${name}" || return 1
}

kgit_cmd_wt_remove() {
    if [[ -n ${KGIT_HELP:-} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} <name>"
        kgit_msg2 '    deletes a worktree at ../<name> and deletes a branch krnowak/<name>'
        return 0
    fi
    kgit_req_params_eq 1 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes one argument, a base name"
        return 1
    }
    local name
    name=${1}; shift
    "${kgit_exec[@]}" worktree remove --force "../${name}" || return 1
    "${kgit_exec[@]}" branch -D "krnowak/${name}" || return 1
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
    local branch
    branch=${1}; shift
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
    local repo
    repo=${1}; shift
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

kgit_list_cmds() {
    kgit_msg2 "Available commands:"
    declare -F | cut -d' ' -f3 | grep '^kgit_cmd_' | sed -e 's/kgit_cmd_//' -e 's/_/-/' | sort || return 1
}

kgit_cmd_help() {
    if [[ -n ${KGIT_HELP} ]]; then
        kgit_msg2 "kgit ${kgit_cmd} [command]"
        kgit_msg2 '    prints available commands or a command help'
        return 0
    fi
    kgit_req_params_le 1 "${@}" || {
        kgit_msg2 "'${kgit_cmd}' takes at most 1 argument, a command name"
        return 1
    }
    if [[ ${#} -gt 0 ]]; then
        local kgit_cmd func
        kgit_cmd=${1}; shift
        func="kgit_cmd_${kgit_cmd//-/_}"

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
    local -a kgit_extra_opts
    kgit_extra_opts=()
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
    kgit_cmd="${1}"; shift
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
