
![image](https://github.com/DreamMaoMao/dwl/assets/30348075/af974dcc-df17-42cd-83bc-9d80498db29c)


# dwl - dwm for Wayland

#改动日志
- waybar支持
- 增加假全屏模式
- 去掉推荐窗口大小逻辑,解决qq等应用平铺时混乱的bug
- 增加向前和向后切换工作区功能
- 增加将窗口往前和往后工作区中移动的功能
- 增加切换到特定数字工作区功能
- 增加将窗口移动到特定数字工作区的功能
- 新增加自定义真全屏模式,解决系统自带全屏函数经常崩溃的问题
- 新增系统自带全屏,假全屏,自定义真全屏都可以在新打开窗口的时候自动退出参与平铺
- 新增加border颜色标识窗口处于假全屏没模式
- 新增可以通过方向确认聚焦的窗口
- 新增中键触发假全屏
- 增加鼠标滚轮加super键可以切换工作区
- 增加overview模式
- 增加左下鼠标热区
- 增加overview模式,左键跳转,右键关闭
- overview跳转前后保持浮动和全屏窗口和还原
- 增加支持键盘数字锁可以在config.h设置默认打开功能

# 运行需要的相关工具包
```
sudo pacman -S network-manager-applet
sudo pacman -S nemo
sudo pacman -S konsole
sudo pacman -S gnome-system-monitor 
sudo pacman -S fcitx-qt5 fcitx fcitx-configtool
sudo pacman -S brightnessctl 
sudo pacman -S bluez bluez-utils 
sudo pacman -S sysstat
sudo pacman -S libpulse
sudo pacman -S fish
sudo pacman -S base-devel
sudo pacman -S meson ninja
sudo pacman -S inetutils 
sudo pacman -S networkmanager 
sudo pacman -S gdm

yay -S blueman acpi mako jq alsa-utils polkit-gnome  light  nemo swappy swaybg lm_sensors  network-manager-applet playerctl python3  wlsunset  xorg-xinit xorg-xwayland wlroots wayland-protocols pavucontrol ttf-jetbrains-mono-nerd eww-wayland wofi xdg-desktop-portal-wlr


```


# 拷贝相关配置dwm
```
cd ~/.config
git clone https://github.com/DreamMaoMao/dwl.git

cd dwl

//编译安装
meson setup --buildtype=release build  -Dprefix=/usr/local
sudo ninja -C build install

//拷贝配置
cp mako -r ~/.config/
cp fish -r ~/.config/
cp wofi -r ~/.config/
cp konsole -r ~/.local/share/
cp eww -r ~/.config/

sed -i s#/home/user#$HOME# dwl.desktop
sudo cp dwl.desktop /usr/share/wayland-sessions/
```

# 终端设置(默认用的konsole)
```
chsh -s /usr/bin/fish

git clone https://github.com/oh-my-fish/oh-my-fish
cd oh-my-fish
bin/install --offline
omf install bira
```
# service
```
sudo systemctl start bluetooth

sudo systemctl enable bluetooth

sudo systemctl enable gdm

sudo systemctl enable NetworkManager

```


# 按键配置修改请查看config.h里的注释