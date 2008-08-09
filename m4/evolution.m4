AC_MSG_CHECKING(Evolution version)
dnl is this a reasonable thing to do ?
EVOLUTION_VERSION=`pkg-config --modversion evolution-shell 2>/dev/null`
if test -n "$EVOLUTION_VERSION"; then
        EVOLUTION_BASE_VERSION=$EVOLUTION_VERSION
        EVOLUTION_BASE_VERSION_S=""
        EVOLUTION_EXEC_VERSION=`pkg-config --variable=execversion evolution-shell 2>/dev/null`
        if test -n "$EVOLUTION_EXEC_VERSION"; then
                break;
        else
                dnl we need major minor here
                EVOLUTION_EXEC_VERSION=$EVOLUTION_BASE_VERSION
                break;
        fi
else
        evo_versions='12 11 10 8 6 4'
        for i in $evo_versions; do
                EVOLUTION_VERSION=`pkg-config --modversion evolution-shell-2.$i 2>/dev/null`
                if test -n "$EVOLUTION_VERSION"; then
                        EVOLUTION_BASE_VERSION=2.$i
                        EVOLUTION_BASE_VERSION_S="-"$EVOLUTION_BASE_VERSION
                        dnl this might be required for devel version
                        EVOLUTION_EXEC_VERSION=`pkg-config --variable=execversion evolution-shell-2.$i 2>/dev/null`
                        if test -n "$EVOLUTION_EXEC_VERSION"; then
                                break;
                        else
                                EVOLUTION_EXEC_VERSION=$EVOLUTION_BASE_VERSION
                                break;
                        fi
                        break;
                else
                        continue;
                fi
        done
        if test -z "$EVOLUTION_VERSION"; then
                AC_MSG_ERROR(Evolution development libraries not installed)
        fi
fi
AC_SUBST(EVOLUTION_VERSION)
AC_MSG_RESULT($EVOLUTION_VERSION)
AC_SUBST(EVOLUTION_BASE_VERSION)
AC_SUBST(EVOLUTION_EXEC_VERSION)
