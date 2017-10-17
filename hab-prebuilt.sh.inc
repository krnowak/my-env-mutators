add_to_env_var PATH hab "$(echo $PWD | cut -d/ -f-3)/bin"
add_to_env_var PS1DATA hab hab-prebuilt
