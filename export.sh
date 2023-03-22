#!/bin/sh

HOMEDIR=/usr
QTDIR=$HOMEDIR/qt5.12.2-rk3399-release
export LD_LIBRARY_PATH=${QTDIR}/lib
export QT_PLUGIN_PATH=${QTDIR}/plugins
export QT_QPA_PLATFORM_PLUGIN_PATH=${QTDIR}/plugins/platforms
#export QT_QPA_EGLFS_FB=/dev/fb0
#export QT_QPA_EGLFS_INTEGRATION=eglfs_kms
#export QT_QPA_EVDEV_KEYBOARD_PARAMETERS="keymap=./file/uu.qmap"
#export QT_QPA_EGLFS_HIDECURSOR=1
#export QT_QPA_DEFAULT_PLATFORM=xcb
#export QT_QPA_FONTDIR=${HOMEDIR}/lib/fonts
#export QT_QPA_FONTDIR=${HOMEDIR}/fonts
export QML2_IMPORT_PATH=${QTDIR}/qml
#export QMLSCENE_DEVICE=softwarecontext
#export EGLFS_X11_SIZE=1024x600
#export EGLFS_X11_FULLSCREEN=0
#export QT_QPA_EGLFS_PHYSICAL_WIDTH=2048
#export QT_QPA_EGLFS_PHYSICAL_HEIGHT=600
#export QT_OPENGL_NO_SANITY_CHECK=yes
export TZ=/etc/localtime
