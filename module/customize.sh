SKIPUNZIP=0
ui_print ""
ui_print "  __     __  ____  ____   ___   ___  _____ "
ui_print "  \ \   / / / ___||  _ \ / _ \ / _ \|  ___|"
ui_print "   \ \ / /  \___ \| |_) | | | | | | | |_   "
ui_print "    \ V /    ___) |  __/| |_| | |_| |  _|  "
ui_print "     \_/    |____/|_|    \___/ \___/|_|    "
ui_print ""
ui_print "  V Spoofer v2.0 (Zygisk)"
ui_print "  Author   : @Xverno"
ui_print "  Telegram : t.me/VernoTalks"
ui_print ""
sleep 1
D=/data/adb/vspoofer
mkdir -p $D
[ -f $D/config ] || printf 'MODE=perapp\nPRESET=rog7\nENGINE=zygisk\n' > $D/config
[ -f $D/games.list ] || : > $D/games.list
[ -f $D/custom.conf ] || : > $D/custom.conf
set_perm_recursive $MODPATH 0 0 0755 0644
set_perm $MODPATH/vspoof.sh 0 0 0755
set_perm $MODPATH/service.sh 0 0 0755
set_perm $MODPATH/uninstall.sh 0 0 0755
ui_print "  Enable Zygisk, reboot, then open the WebUI."
ui_print ""
