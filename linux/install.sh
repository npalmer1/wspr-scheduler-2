#!/usr/bin/env bash
# WSPR Scheduler 2 - install or update on Ubuntu / Debian / Raspberry Pi OS.
#
# Just this script is enough - download it and run it:
#   wget https://raw.githubusercontent.com/npalmer1/wspr-scheduler-2/main/linux/install.sh
#   bash install.sh
# (or, in a folder already cloned from GitHub:  bash linux/install.sh)
#
# It asks for your password (sudo) where it needs it, and:
#   0. installs git if it isn't there, and gets the source: clones it into ~/wspr-scheduler-2, or if
#      that's already there, pulls the latest version - then carries on with the newest copy of itself;
#   1. installs what the program needs: Qt 6, the MySQL driver, Hamlib (rigctld), wsprd (the WSPR
#      decoder, unpacked on its own from Ubuntu's WSJT-X package - not all of WSJT-X),
#      MariaDB (or keeps the MySQL already there), fonts, the compiler and CMake;
#   2. builds WSPR Scheduler 2 and installs it in /usr/local (WsprSked, with a menu entry);
#   3. lets your user use the rig's serial port and the sound card (dialout and audio groups);
#   4. starts MySQL now and whenever the PC starts, adds the "admin" database user and, if they aren't
#      there yet, the WSPR Scheduler databases (from wspr_db.sql) - existing databases are never touched;
#   5. makes ~/WSPR_Sked (wsprd's files, the help file) and turns on the network time service.
#
# Run it again to update: it pulls the latest version, rebuilds and reinstalls the program, and never
# changes the databases or settings that are already there.
#
# Options:
#   --dir DIR      keep the source in DIR instead of ~/wspr-scheduler-2
#   --no-update    don't clone or pull: build the source folder this script is in, as it is
#   --full-wsjtx   install the whole of WSJT-X (the program too), not just its wsprd
#   --no-wsjtx     no wsprd either (no WSPR receiving)
#   --no-mysql     don't install or set up MySQL (the databases are on another PC)
#   --uninstall    remove the program again (not the source, databases, settings or ~/WSPR_Sked)
set -euo pipefail

REPO_URL="${WSPR_SKED_REPO:-https://github.com/npalmer1/wspr-scheduler-2.git}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX=/usr/local
DATADIR="$HOME/WSPR_Sked"
WSPRD=only        # only: just wsprd from the WSJT-X package; full: all of WSJT-X; none
WITH_MYSQL=1
UPDATE=1
UNINSTALL=0
DEST=""

say()  { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
ok()   { printf '\033[0;32m   %s\033[0m\n' "$*"; }
warn() { printf '\033[0;33m   ! %s\033[0m\n' "$*"; }
die()  { printf '\033[0;31m   %s\033[0m\n' "$*" >&2; exit 1; }

# sudo apt-get ..., waiting (up to 10 minutes) if apt is busy - e.g. a fresh Debian / Raspberry Pi OS
# install runs its own software update in the background (packagekitd) as soon as the network is up
apt_get() {
    local log tries=0
    log=$(mktemp)
    while :; do
        if sudo apt-get -o DPkg::Lock::Timeout=600 "$@" 2>&1 | tee "$log"; [ "${PIPESTATUS[0]}" -eq 0 ]; then
            rm -f "$log"; return 0
        fi
        if grep -q -E "Could not get lock|Unable to lock" "$log" && [ $((tries += 1)) -le 60 ]; then
            [ "$tries" = 1 ] && warn "apt is busy (the system's own software update?) - waiting for it to finish..."
            sleep 10
            continue
        fi
        rm -f "$log"; return 1
    done
}

while [ $# -gt 0 ]; do
    case "$1" in
        --dir) [ $# -ge 2 ] || die "--dir needs a folder"; DEST="$2"; shift ;;
        --dir=*) DEST="${1#--dir=}" ;;
        --no-update) UPDATE=0 ;;
        --no-wsjtx|--no-wsprd) WSPRD=none ;;
        --full-wsjtx) WSPRD=full ;;
        --no-mysql) WITH_MYSQL=0 ;;
        --uninstall) UNINSTALL=1 ;;
        -h|--help) sed -n '2,/^set -e/{/^set -e/!p}' "$0"; exit 0 ;;
        *) die "Unknown option $1 (see --help)" ;;
    esac
    shift
done

[ "$(id -u)" -ne 0 ] || die "Run this as your normal user, not with sudo: it asks for the password itself."
command -v apt-get >/dev/null || die "This script is for Ubuntu / Debian / Raspberry Pi OS (apt). See linux/README.md."

# Where the source is: the folder this script is in, if it's a copy of WSPR Scheduler 2 (cloned or
# unpacked from a ZIP); otherwise --dir, or ~/wspr-scheduler-2
INSIDE=""
[ -f "$HERE/../CMakeLists.txt" ] && grep -q "WsprSked" "$HERE/../CMakeLists.txt" && INSIDE="$(cd "$HERE/.." && pwd)"
if [ -z "$DEST" ]; then
    DEST="${INSIDE:-$HOME/wspr-scheduler-2}"
fi
DEST="$(mkdir -p "$(dirname "$DEST")" && cd "$(dirname "$DEST")" && pwd)/$(basename "$DEST")"

if [ "$UNINSTALL" = 1 ]; then
    BUILD="$DEST/build-linux-release"
    [ -f "$BUILD/install_manifest.txt" ] || die "Nothing to remove (no $BUILD/install_manifest.txt)."
    say "Removing WSPR Scheduler 2"
    xargs -d '\n' sudo rm -fv < "$BUILD/install_manifest.txt"
    if [ -f "$BUILD/wsprd-installed" ]; then
        xargs -d '\n' sudo rm -fv < "$BUILD/wsprd-installed"
        rm -f "$BUILD/wsprd-installed"
    fi
    sudo rmdir "$PREFIX/share/wspr-scheduler-2" 2>/dev/null || true
    ok "Removed. The source ($DEST), the databases, ~/WSPR_Sked and ~/DBcredential are still there."
    exit 0
fi

# ---- 0. the source: git, then clone or pull ---------------------------------------------------------
if [ -z "${WSPR_SKED_BOOTSTRAPPED:-}" ] && [ "$UPDATE" = 1 ]; then
    say "Getting the latest WSPR Scheduler 2"
    if command -v git >/dev/null; then
        ok "git is installed"
    else
        apt_get update
        apt_get install -y git
        ok "git installed"
    fi
    clone() {
        if ! git clone "$REPO_URL" "$DEST"; then
            die "Couldn't download $REPO_URL - check the internet connection (and that the repository is public)."
        fi
        ok "Downloaded into $DEST"
    }
    if [ -d "$DEST/.git" ]; then
        cd "$DEST"
        if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
            warn "$DEST has changes of your own, so it wasn't updated:"
            git status --short --untracked-files=no | sed 's/^/     /'
            die "Keep them with 'git stash' (or 'git commit'), or throw them away with 'git checkout .', then run this again - or use --no-update to build it as it is."
        fi
        before="$(git rev-parse HEAD)"
        git pull --ff-only || die "git pull failed - see above. Or use --no-update to build the copy you have."
        if [ "$(git rev-parse HEAD)" = "$before" ]; then ok "$DEST is already the latest version"
        else ok "$DEST updated to the latest version"; fi
    elif [ -e "$DEST" ] && [ -n "$(ls -A "$DEST" 2>/dev/null)" ]; then
        # e.g. unpacked from a ZIP: keep it, but under another name, and clone a proper copy
        OLD="$DEST.old-$(date +%Y%m%d-%H%M%S)"
        mv "$DEST" "$OLD"
        warn "$DEST wasn't a git copy: moved it to $OLD"
        clone
    else
        rmdir "$DEST" 2>/dev/null || true
        clone
    fi
    # carry on with the newest copy of this script (it may have changed)
    export WSPR_SKED_BOOTSTRAPPED=1
    NEXT=(--dir "$DEST" --no-update)
    [ "$WSPRD" = none ] && NEXT+=(--no-wsjtx)
    [ "$WSPRD" = full ] && NEXT+=(--full-wsjtx)
    [ "$WITH_MYSQL" = 0 ] && NEXT+=(--no-mysql)
    exec bash "$DEST/linux/install.sh" "${NEXT[@]}"
fi

SRC="$DEST"
[ -f "$SRC/CMakeLists.txt" ] || die "No WSPR Scheduler 2 source in $SRC."
BUILD="$SRC/build-linux-release"
cd "$SRC"

# ---- 1. packages ----------------------------------------------------------------------------------
say "Installing the packages WSPR Scheduler 2 needs (this can take a few minutes)"
apt_get update   # first, so the package lists below are up to date
PKGS=(build-essential cmake git pkg-config
      qt6-base-dev qt6-tools-dev qt6-multimedia-dev qt6-serialport-dev libqt6sql6-mysql libxkbcommon-dev
      libhamlib-utils fonts-liberation openssl pulseaudio-utils)
[ "$WSPRD" = full ] && PKGS+=(wsjtx)
[ "$WSPRD" = only ] && PKGS+=(libfftw3-single3 libgfortran5)   # all that wsprd itself needs
if [ "$WITH_MYSQL" = 1 ]; then
    # keep whichever this PC already has; otherwise MariaDB - the same on Ubuntu, Debian and Raspberry Pi OS,
    # and what XAMPP uses on Windows (WSPR Scheduler works with MySQL too)
    mysql_state=$(dpkg-query -W -f='${Status}' mysql-server 2>/dev/null || true)
    case "$mysql_state" in
        "install ok installed") PKGS+=(mysql-server) ;;
        ""|"deinstall ok config-files"|"unknown ok not-installed") PKGS+=(mariadb-server) ;;
        *) die "MySQL is half installed or half removed ($mysql_state), so the database can't be set up.
   To remove it completely (this deletes any MySQL databases on this PC), then run this script again:
     sudo dpkg --purge --force-all mysql-server mysql-server-8.0
     sudo apt-get purge -y 'mysql-server*' 'mysql-client*' mysql-common
     sudo apt-get --fix-broken install -y && sudo apt-get autoremove -y
     sudo rm -rf /etc/mysql /var/lib/mysql" ;;
    esac
fi
apt_get install -y "${PKGS[@]}"
ok "Packages installed"

# wsprd on its own (as wsprd.exe on Windows): unpacked from this system's WSJT-X package, without
# installing WSJT-X and the Qt 5 / Boost / ... it would bring with it
if [ "$WSPRD" = only ]; then
    if dpkg -s wsjtx >/dev/null 2>&1; then
        ok "WSJT-X is installed: using its wsprd"
    else
        TMP="$(mktemp -d)"
        if (cd "$TMP" && apt-get download wsjtx >/dev/null 2>&1) \
           && dpkg-deb --fsys-tarfile "$TMP"/wsjtx_*.deb | tar -x -C "$TMP" ./usr/bin/wsprd; then
            sudo install -m 755 "$TMP/usr/bin/wsprd" "$PREFIX/bin/wsprd"
            mkdir -p "$BUILD"
            echo "$PREFIX/bin/wsprd" > "$BUILD/wsprd-installed"
            ok "wsprd installed in $PREFIX/bin (from the WSJT-X package, without the rest of WSJT-X)"
        else
            warn "couldn't get wsprd from the WSJT-X package - run again with --full-wsjtx"
        fi
        rm -rf "$TMP"
    fi
fi

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
if [ "$WSPRD" != none ]; then
    if command -v wsprd >/dev/null; then ok "wsprd: $(command -v wsprd)"; else warn "wsprd not found - receiving won't decode"; fi
fi
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
