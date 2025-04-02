



// apt-get update
// apt-get install xorg dbus-x11 openbox
// sudo nvidia-xconfig --mode=1280x720

// startx -- :0

// export DISPLAY=:0
// openbox



// export DISPLAY=:0
// xrandr --newmode "1440x900_60.00" 74.50 1280 1344 1472 1664 720 723 728 748 -hsync +vsync
// xrandr --addmode DVI-D-0 "1440x900_60.00"
// xrandr --output DVI-D-0 --mode "1440x900_60.00"

// xrandr --listmonitors

// xdpyinfo | grep dimensions | awk '{print $2}'


// export DISPLAY=:0
// apt-get install mesa-utils
// glxgears


// glxinfo | grep "OpenGL renderer"