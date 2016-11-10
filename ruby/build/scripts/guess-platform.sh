###################################################################################
# (c) Copyright IBM Corp. 2000, 2016
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
##################################################################################


thirty_two_bit=0

usage() {
    echo "usage: guess-platform.sh [--32|--64]" >&2
    echo "Outputs the value that can be assigned to the PLATFORM variable in" >&2
    echo "the jit makefiles. If neither --32 or --64 is provided, --64 is   " >&2
    echo "assumed." >&2
    exit 1
}

error() {
    echo "$0: $1" >&2
    exit 1
}

# chooseplatform 32bit_platform 64bit_platform
chooseplatform() {
    if [[ $thirty_two_bit -eq 1 ]] ; then
        platform=$1
    else
        platform=$2
    fi
}

if [ $# -eq 0 ]; then
    arg="--64"
elif [ $# -eq 1 ]; then
    arg=$1
else
    usage
fi

case $arg in
    --32)
        thirty_two_bit=1
        ;;
    --64)
        thirty_two_bit=0
        ;;
    *)
        usage
        ;;
esac

system=$(uname -s)
case $system in
    AIX)
        chooseplatform "ppc-aix-vacpp" "ppc64-aix-vacpp"
        ;;
    OS/390)
        chooseplatform "s390-zos-vacpp" "s390-zos64-vacpp"
        ;;
    CYGWIN*)
        chooseplatform "ia32-win32-mvs" "amd64-win64-mvs"
        ;;
    Darwin)
        platform="amd64-osx-clang"
        ;;
    Linux)
        p=$(uname -m)
        case $p in
            i[456]86)
                platform="ia32-linux-gcc"
                ;;
            x86_64)
                chooseplatform "amd64-linux-gcc" "amd64-linux64-gcc"
                ;;
            ppc64)
                chooseplatform "ppc-linux-gcc" "ppc64-linux64-gcc"
                ;;
            ppc64le)
                platform="ppc64le-linux64-gcc"
                ;;
            s390x)
                chooseplatform "s390-linux-gcc" "s390-linux64-gcc"
                ;;
            *)
                error "$0: uname -m returns unknown result \"$p\""
                ;;
        esac
        ;;
    *)
        error "$0: uname -s returns unknown result \"$system\""
        ;;
esac

echo $platform
