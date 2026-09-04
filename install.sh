#!/bin/bash

# Ensure script is run with root privileges for system-wide installation
if [ "$EUID" -ne 0 ]; then
  echo "[-] Requesting root privileges for system installation..."
  exec sudo "$0" "$@"
fi

echo "==========================="
echo "   DuckyWM Installer       "
echo "==========================="
echo "1) X11 Protocol"
echo "2) Wayland Protocol (wlroots)"
read -p "Please select your display server (1 or 2): " choice

mkdir -p bin

if [ "$choice" = "1" ]; then
    echo "[+] Compiling DuckyWM for X11..."
    gcc -O2 -Wall -Wextra src/main_x11.c -o bin/duckywm -lX11
    SESSION_DIR="/usr/share/xsessions"
elif [ "$choice" = "2" ]; then
    echo "[+] Compiling DuckyWM for Wayland (wlroots 0.20)..."
    if [ -f "src/main_wayland.c" ]; then
        PKG_CONFIG_PATH="/usr/lib64/pkgconfig" gcc -O2 -Wall -Wextra -DWLR_USE_UNSTABLE \
            src/main_wayland.c -o bin/duckywm \
            $(pkg-config --cflags --libs wlroots-0.20 wayland-server xkbcommon)
        SESSION_DIR="/usr/share/wayland-sessions"
    else
        echo "[-] Error: src/main_wayland.c not found."
        exit 1
    fi
else
    echo "[-] No option selected!"
    exit 1
fi

# Install binary system-wide
echo "[+] Installing binary to /usr/local/bin/duckywm..."
cp bin/duckywm /usr/local/bin/duckywm
chmod +x /usr/local/bin/duckywm

# Register session for SDDM / Login Managers
echo "[+] Registering session..."
mkdir -p "$SESSION_DIR"
cat << EOF > "$SESSION_DIR/duckywm.desktop"
[Desktop Entry]
Name=DuckyWM
Comment=A custom window manager/compositor
Exec=/usr/local/bin/duckywm
Type=Application
DesktopNames=DuckyWM
EOF

echo "[+] Installation complete! DuckyWM is ready to use from your login screen."
