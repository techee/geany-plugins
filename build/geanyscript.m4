AC_DEFUN([GP_CHECK_GEANYSCRIPT],
[
    GP_ARG_DISABLE([GeanyScript], [auto])
    GP_COMMIT_PLUGIN_STATUS([GeanyScript])
    AC_CONFIG_FILES([
        geanyscript/Makefile
        geanyscript/src/Makefile
    ])
])
