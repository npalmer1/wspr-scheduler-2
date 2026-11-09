#!/usr/bin/env bash
# WSPR Scheduler 2 - install on Ubuntu / Debian / Raspberry Pi OS (from this source folder).
#
#   cd wspr-scheduler-2          (the folder downloaded from GitHub)
#   bash linux/install.sh
#
# It asks for your password (sudo) where it needs it, and:
#   1. installs what the program needs: Qt 6, the MySQL driver, Hamlib (rigctld), WSJT-X (wsprd),
#      MySQL, fonts, the compiler and CMake;
#   2. builds WSPR Scheduler 2 and installs it in /usr/local (WsprSked, with a menu entry);
#   3. lets your user use the rig's serial port and the sound card (dialout and audio groups);
#   4. starts MySQL now and whenever the PC starts, adds the "admin" database user and, if they aren't
#      there yet, the WSPR Scheduler databases (from wspr_db.sql) - existing databases are never touched;
#   5. makes ~/WSPR_Sked (wsprd's files, the help file) and turns on the network time service.
#
# Run it again after "git pull" (or a new download) to update: it only rebuilds and reinstalls the
# program, and never changes the databases or settings that are already there.
#
# Options:
#   --no-wsjtx     don't install WSJT-X (no wsprd: no WSPR receiving)
#   --no-mysql     don't install or set up MySQL (the databases are on another PC)
#   --uninstall    remove the program again (not the databases, settings or ~/WSPR_Sked)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$(cd "$HERE/.." && pwd)"
BUILD="$SRC/build-linux-release"
PREFIX=/usr/local
DATADIR="$HOME/WSPR_Sked"
WITH_WSJTX=1
WITH_MYSQL=1

say()  { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
ok()   { printf '\033[0;32m   %s\033[0m\n' "$*"; }
warn() { printf '\033[0;33m   ! %s\033[0m\n' "$*"; }
die()  { printf '\033[0;31m   %s\033[0m\n' "$*" >&2; exit 1; }

for a in "$@"; do
    case "$a" in
        --no-wsjtx) WITH_WSJTX=0 ;;
        --no-mysql) WITH_MYSQL=0 ;;
        --uninstall)
            if [ -f "$BUILD/install_manifest.txt" ]; then
                say "Removing WSPR Scheduler 2"
                xargs -d '\n' sudo rm -fv < "$BUILD/install_manifest.txt"
                sudo rmdir "$PREFIX/share/wspr-scheduler-2" 2>/dev/null || true
                ok "Removed. The databases, ~/WSPR_Sked and ~/DBcredential are still there."
            else
                die "Nothing to remove (no $BUILD/install_manifest.txt)."
            fi
            exit 0 ;;
        -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
        *) die "Unknown option $a (see --help)" ;;
    esac
done

[ "$(id -u)" -ne 0 ] || die "Run this as your normal user, not with sudo: it asks for the password itself."
command -v apt-get >/dev/null || die "This script is for Ubuntu / Debian / Raspberry Pi OS (apt). See linux/README.md."
[ -f "$SRC/CMakeLists.txt" ] || die "Run it from the WSPR Scheduler 2 source folder (bash linux/install.sh)."

# ---- 1. packages ----------------------------------------------------------------------------------
say "Installing the packages WSPR Scheduler 2 needs (this can take a few minutes)"
PKGS=(build-essential cmake git pkg-config
      qt6-base-dev qt6-tools-dev qt6-multimedia-dev qt6-serialport-dev libqt6sql6-mysql libxkbcommon-dev
      libhamlib-utils fonts-liberation openssl pulseaudio-utils)
[ "$WITH_WSJTX" = 1 ] && PKGS+=(wsjtx)
if [ "$WITH_MYSQL" = 1 ]; then
    # keep MariaDB if that's what this PC has; otherwise MySQL, as Ubuntu suggests
    if dpkg -s mariadb-server >/dev/null 2>&1; then PKGS+=(mariadb-server); else PKGS+=(mysql-server); fi
fi
sudo apt-get update
sudo apt-get install -y "${PKGS[@]}"
ok "Packages installed"

# ---- 2. build and install -------------------------------------------------------------------------
say "Building WSPR Scheduler 2"
cmake -S "$SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" >/dev/null
cmake --build "$BUILD" -j"$(nproc)"
sudo cmake --install "$BUILD"
sudo update-desktop-database "$PREFIX/share/applications" 2>/dev/null || true
sudo gtk-update-icon-cache -q "$PREFIX/share/icons/hicolor" 2>/dev/null || true
ok "Installed $PREFIX/bin/WsprSked (and 'WSPR Scheduler 2' in the applications menu)"

# ---- 3. serial port and sound ---------------------------------------------------------------------
say "Letting $USER use serial ports and sound"
NEWGROUP=0
for g in dialout audio; do
    if id -nG "$USER" | tr ' ' '\n' | grep -qx "$g"; then
        ok "already in the $g group"
    else
        sudo usermod -aG "$g" "$USER"
        ok "added to the $g group"
        NEWGROUP=1
    fi
done

# ---- 4. MySQL ------------------------------------------------------------------------------------
SQL_FILE=""
for f in "$SRC/linux/wspr_db.sql" "$SRC/installer/thirdparty/wspr_db.sql" "$PREFIX/share/wspr-scheduler-2/wspr_db.sql" \
         "$DATADIR/wspr_db.sql" "$HOME/Downloads/wspr_db.sql"; do
    if [ -f "$f" ]; then SQL_FILE="$f"; break; fi
done
if [ "$WITH_MYSQL" = 1 ]; then
    say "Setting up MySQL"
    UNIT=""
    for u in mysql mariadb; do
        if [ "$(systemctl show -p LoadState --value "$u" 2>/dev/null)" = loaded ]; then UNIT=$u; break; fi
    done
    [ -n "$UNIT" ] || die "No mysql or mariadb service found after installing - see linux/README.md."
    sudo systemctl enable --now "$UNIT"
    ok "$UNIT running, and starts with the PC"

    # the admin user WSPR Scheduler logs in as (root on Ubuntu logs in through sudo, no password)
    if [ -n "$(sudo mysql -N -e "SELECT 1 FROM mysql.user WHERE user='admin' LIMIT 1")" ]; then
        ok "database user 'admin' already there (its password is not changed)"
    else
        echo "   WSPR Scheduler logs in to MySQL as 'admin'. Choose its password"
        read -r -s -p "   (Enter for the default, wspr): " PW; echo
        PW="${PW:-wspr}"
        PW_SQL="${PW//\\/\\\\}"; PW_SQL="${PW_SQL//\'/\\\'}"
        sudo mysql <<SQL
CREATE USER IF NOT EXISTS 'admin'@'localhost' IDENTIFIED BY '$PW_SQL';
CREATE USER IF NOT EXISTS 'admin'@'127.0.0.1' IDENTIFIED BY '$PW_SQL';
GRANT ALL PRIVILEGES ON *.* TO 'admin'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON *.* TO 'admin'@'127.0.0.1' WITH GRANT OPTION;
FLUSH PRIVILEGES;
SQL
        ok "database user 'admin' added"
        if [ "$PW" != wspr ]; then
            # WSPR Scheduler reads db/password from its settings when there's no ~/DBcredential yet;
            # saving the password in the program later writes ~/DBcredential as on Windows
            CONF="$HOME/.config/G4GCI/WSPR Sked.conf"
            mkdir -p "$(dirname "$CONF")"
            if ! grep -q '^\[db\]' "$CONF" 2>/dev/null; then
                printf '[db]\npassword=%s\n' "$PW" >> "$CONF"
                chmod 600 "$CONF"
                ok "password saved in $CONF"
            else
                warn "$CONF already has database settings: enter the new password in WSPR Scheduler"
            fi
        fi
    fi

    # the databases: only if they aren't there - never over existing ones
    if [ -n "$(sudo mysql -N -e "SHOW DATABASES LIKE 'wspr'")" ]; then
        ok "WSPR Scheduler databases already there (not changed)"
    elif [ -n "$SQL_FILE" ]; then
        sudo mysql < "$SQL_FILE"
        if [ -n "$(sudo mysql -N -e "SHOW DATABASES LIKE 'wspr'")" ]; then
            ok "WSPR Scheduler databases made from $SQL_FILE"
        else
            warn "$SQL_FILE was imported but there is no 'wspr' database - is it the right file?"
        fi
    else
        warn "wspr_db.sql not found, so the WSPR Scheduler databases were not made."
        warn "Put wspr_db.sql in $SRC/linux/ (or ~/Downloads) and run this script again,"
        warn "or import it yourself:  sudo mysql < wspr_db.sql"
    fi
fi

# ---- 5. data folder, help, clock ------------------------------------------------------------------
say "Finishing"
mkdir -p "$DATADIR"
HELP="$PREFIX/share/wspr-scheduler-2/wspr_sked2_help.rtf"
if [ -f "$HELP" ] && [ ! -f "$DATADIR/wspr_sked2_help.rtf" ]; then
    cp "$HELP" "$DATADIR/"
    ok "help file (F1) copied to $DATADIR"
fi
ok "$DATADIR ready (wsprd's working files)"
if command -v timedatectl >/dev/null; then
    sudo timedatectl set-ntp true 2>/dev/null && ok "network time on (WSPR needs the clock within a second)" \
        || warn "couldn't turn on network time - check the clock with: timedatectl"
fi
[ "$WITH_WSJTX" = 1 ] && { command -v wsprd >/dev/null && ok "wsprd: $(command -v wsprd)" || warn "wsprd not found - receiving won't decode"; }
command -v rigctld >/dev/null && ok "rigctld: $(command -v rigctld)"

say "Done"
echo "   Start it from the applications menu (WSPR Scheduler 2), or type: WsprSked"
if [ "$NEWGROUP" = 1 ]; then
    echo
    warn "Log out and back in (or restart the PC) before using the rig: the new dialout / audio"
    warn "permissions only apply to a new login."
fi
echo "   Then, in WSPR Scheduler 2: Rigctl rigs tab - choose the rig and its port (ttyUSB0 ...), Save;"
echo "   RX & Sound config tab - Update list, choose the rig's sound card for input and output."
