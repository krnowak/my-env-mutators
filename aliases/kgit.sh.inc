#!/bin/bash

kgit_extra_opts=()
kgit_return=1

kgit_msg2() {
    echo "kgit: ${*}" >&2
}

kgit_call() {
    kgit_msg2 "Invoking '${@}'"
    "${@}"
}

kgit_req_params_eq() {
    local num="${1}"; shift

    [[ "${#}" -eq "${num}" ]]
}

kgit_req_params_ge() {
    local num="${1}"; shift

    [[ "${#}" -ge "${num}" ]]
}

while true; do
    while [[ ${#} -gt 0 ]] && [[ "${1}" = '-C' ]]; do
        kgit_extra_opts+=('-C' "${2}")
        shift 2
    done

    if [[ ${#} -eq 0 ]]; then
        kgit_msg2 'No command given'
        break
    fi

    kgit_exec=('kgit_call' 'git' "${kgit_extra_opts[@]}")
    kgit_cmd="${1}"
    shift

    case "${kgit_cmd}" in
        fetch)
            "${kgit_exec[@]}" fetch -pPtf "${@}" || break
            ;;
        wk)
            kgit_req_params_eq 0 "${@}" || {
                kgit_msg2 "'${kgit_cmd}' takes no arguments"
                break
            }
            "${kgit_exec[@]}" worktree list || break
            ;;
        wk-add)
            kgit_req_params_ge 1 "${@}" || {
                kgit_msg2 "'${kgit_cmd}' takes at least one argument, a base name, rest is forwarded to 'git worktree add'"
                break
            }
            kgit_name="${1}"; shift
            "${kgit_exec[@]}" worktree add -b "krnowak/${kgit_name}" "../${kgit_name}" "${@}" || break
            kgit_call cd "../${kgit_name}" || break
            ;;
        wk-remove)
            kgit_req_params_eq 1 "${@}" || {
                kgit_msg2 "'${kgit_cmd}' takes one argument, a base name"
                break
            }
            "${kgit_exec[@]}" worktree remove --force "../${1}" || break
            "${kgit_exec[@]}" branch -D "krnowak/${1}" || break
            ;;
        amend-head)
            kgit_req_params_eq 0 "${@}" || {
                kgit_msg2 "'${kgit_cmd}' takes no arguments"
                break
            }
            "${kgit_exec[@]}" commit --amend --no-edit || break
            ;;
        merge)
            kgit_req_params_eq 0 "${@}" || {
                kgit_msg2 "'${kgit_cmd}' takes no arguments"
                break
            }
            kgit_branch=$("${kgit_exec[@]}" branch --show-current) || break
            kgit_remote=$("${kgit_exec[@]}" config "branch.${kgit_branch}.remote") || {
                kgit_msg2 "Branch ${kgit_branch} is not a tracking branch"
                break
            }
            "${kgit_exec[@]}" merge --ff-only "${kgit_remote}/${kgit_branch}" || break
            "${kgit_exec[@]}" submodule update || break
            ;;
        fpush)
            "${kgit_exec[@]}" push --force-with-lease "${@}" || break
            ;;
        clone)
            kgit_req_params_eq 2 "${@}" || {
                kgit_msg2 "'${kgit_cmd}' needs two arguments, repository URL and branch name"
                break
            }
            kgit_repo="${1}"; shift
            kgit_branch="${1}"; shift
            kgit_dir="${kgit_repo}"
            kgit_dir="${kgit_dir##*/}"
            kgit_dir="${kgit_dir%.git}"
            kgit_dir="${kgit_dir}/${kgit_branch}"
            "${kgit_exec[@]}" clone --branch "${kgit_branch}" "${kgit_repo}" "${kgit_dir}" || break
            kgit_call cd "${kgit_dir}" || break
            ;;
        fresh)
            "${kgit_exec[@]}" reset --hard HEAD || break
            "${kgit_exec[@]}" clean -ffdx || break
            ;;
        *)
            kgit_msg2 "Unknown command '${kgit_cmd}'"
            break
            ;;
    esac
    kgit_return=0
    break
done

trap 'unset "${!kgit_@}"; trap - RETURN' RETURN

return ${kgit_return}
