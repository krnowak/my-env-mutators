#!/bin/bash

kgit_extra_opts=()
kgit_return=1
kgit_call() {
    echo "kgit: Invoking '${@}'" >&2
    "${@}"
}

while true; do
    while [[ ${#} -gt 0 ]] && [[ "${1}" = '-C' ]]; do
        kgit_extra_opts+=('-C' "${2}")
        shift 2
    done

    if [[ ${#} -eq 0 ]]; then
        echo 'kgit: no command given'
        break
    fi

    kgit_exec=('kgit_call' 'git' "${kgit_extra_opts[@]}")

    case "${1}" in
        fetch)
            shift
            "${kgit_exec[@]}" fetch -pPtf "${@}" || break
            ;;
        wk)
            shift
            "${kgit_exec[@]}" worktree list || break
            ;;
        wk-add)
            shift
            "${kgit_exec[@]}" worktree add -b "krnowak/${1}" "../${1}" || break
            cd "../${1}" || break
            ;;
        wk-remove)
            shift
            "${kgit_exec[@]}" worktree remove --force "../${1}" || break
            "${kgit_exec[@]}" branch -D "krnowak/${1}" || break
            ;;
        amend-head)
            shift
            "${kgit_exec[@]}" commit --amend --no-edit || break
            ;;
        merge)
            shift
            kgit_branch=$("${kgit_exec[@]}" branch --show-current) || break
            kgit_remote=$("${kgit_exec[@]}" config "branch.${kgit_branch}.remote") || { echo "kgit: Branch ${kgit_branch} is not a tracking branch" >&2; break; }
            "${kgit_exec[@]}" merge "${kgit_remote}/${kgit_branch}" || break
            ;;
        *)
            echo "kgit: Unknown command '${1}'" >&2
            break
            ;;
    esac
    kgit_return=0
    break
done

trap 'unset "${!kgit_@}"; trap - RETURN' RETURN

return ${kgit_return}
