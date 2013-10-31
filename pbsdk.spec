Name: pbsdk
Version: 1.1.0
Release: 3

Summary: SDK for PocketBook 301+,302,360,360+,602,603,902,903
License: distributable
Group: Development/Tools
URL: http://pbsdk.vlasovsoft.net/

BuildArch: noarch
AutoReqProv: no

BuildRequires: hardlink

%define _unpackaged_files_terminate_build 0

# Disable ALT Linux automation.
%define _verify_elf_method none
%define _fixup_method none

%description
SDK for PocketBook 301+,302,360,360+,602,603,902,903

%build
if [ -d %buildroot/.git ] && [ -d %buildroot/usr/local/pocketbook ]; then
	cd %buildroot
	git clean -dfx usr
	hardlink -vvc usr
else
	echo >&2 "Usage: rpm -bb --define 'buildroot $OLDPWD' pbsdk.spec"
	false
fi

# Create empty directories.
mkdir -p usr/local/pocketbook/system/bin
mkdir -p usr/local/pocketbook/system/cache
mkdir -p usr/local/pocketbook/system/favorite
mkdir -p usr/local/pocketbook/system/logo
mkdir -p usr/local/pocketbook/system/mnt
mkdir -p usr/local/pocketbook/system/state
mkdir -p usr/local/pocketbook/system/tmp

%clean
# Do not remove buildroot.
%define __rm :

%files
/usr/bin/arm-*
/usr/bin/pbres

/usr/lib/gcc/arm-*

/usr/include/bookstate.h
/usr/include/inkinternal.h
/usr/include/inkplatform.h
/usr/include/inkview.h

/usr/include/pbframework
/usr/include/synopsis

/usr/lib/libbookstate.so
/usr/lib/libinkview.so
/usr/lib/libpbframework.so
/usr/lib/libsynopsis.so

%dir /usr/arm-linux
/usr/arm-linux/bin
/usr/arm-linux/lib
/usr/arm-linux/include

%dir /usr/arm-none-linux-gnueabi
/usr/arm-none-linux-gnueabi/bin
/usr/arm-none-linux-gnueabi/lib
/usr/arm-none-linux-gnueabi/include

%dir /usr/local/pocketbook
/usr/local/pocketbook/common.mk
/usr/local/pocketbook/sources
/usr/local/pocketbook/system

%changelog
* Thu Oct 31 2013 Alexey Tourbin <at@altlinux.ru> 1.1.0-3
- common.mk: added rule for %%.cc
- common.mk: add -I/usr/include/freetype2 for BUILD=emu
- common.mk: allow undefined symbols in libinkview.so for BUILD=emu
- arm-linux inkinternal.h: added from 15.1
