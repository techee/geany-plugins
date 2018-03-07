AC_DEFUN([GP_CHECK_GEANYVIM],
[
    GP_ARG_DISABLE([GeanyVim], [auto])
    GP_COMMIT_PLUGIN_STATUS([GeanyVim])
    AC_CONFIG_FILES([
        geanyvim/Makefile
        geanyvim/src/Makefile
    ])
])
