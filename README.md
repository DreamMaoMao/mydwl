



https://github.com/DreamMaoMao/mydwl/assets/30348075/738fa300-55a2-4ec8-b72a-1c9e27359695





https://github.com/DreamMaoMao/mydwl/assets/30348075/f223b559-47dc-4a4a-929f-7a5da78df023



> [!NOTE]
> 该dwl目前基于wlroots-0.17.2 release 其他wlroots版本可能编译不过去
> waybar要使用下面推荐地定制waybar,不然协议可能不通用

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
- 增加左右切换tag时可以只切换到有client的tag
- obs录屏支持
- 增加config.h可配置新窗口是从头部入栈还是从尾部入栈
- 增加鼠标扩展监控支持,修复原版dwl不支持在xwayland窗口移动鼠标的同时使用滚轮
- 添加网格布局为常规可切换布局
- foreign-toplevel协议支持,支持dunst和waybar的taskbar模块
- 支持客户端激活请求自动跳转和手动跳转的waybar 工作区模块urgen提示
- 支持在waybar上隐藏未使用的tag
- 支持在waybar上点击隐藏和显示窗口
- 支持快捷键隐藏和显示窗口
- 添加sway的便签功能
- 增加所有tag可见的全局窗口global状态
- 平铺窗口可按方向交换位置
- 多屏幕waybar支持
- 支持text-input-v3 协议

# 运行需要的相关工具包
```
sudo pacman -S network-manager-applet
sudo pacman -S nemo
sudo pacman -S foot
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
sudo pacman -S wl-clipboard

yay -S blueman acpi swaync jq alsa-utils polkit-gnome  light  swappy swaybg lm_sensors  network-manager-applet playerctl python3  wlsunset  xorg-xinit xorg-xwayland wlroots wayland-protocols pavucontrol ttf-jetbrains-mono-nerd eww-wayland wofi xdg-desktop-portal-hyprland cpufrequtils system-monitoring-center
cliphist wl-clip-persist  wlogout-git rofi wlr-randr yazi fcitx5 fcitx5-im fcitx5-qt fcitx5-chinese-addon  cinnamon-translations nemo-fileroller

```

# 安装wlroots(0.17.2)
```
git clone -b 0.17.2 https://gitlab.freedesktop.org/wlroots/wlroots.git 
cd wlroots
meson build -Dprefix=/usr
sudo ninja -C build install
```

# 安装waybar
```
git clone https://gitee.com/DreamMaoMao/Waybar.git
cd mywaybar
meson build -Dprefix=/usr
sudo ninja -C build install
```

# 拷贝相关配置
```
cd ~/.config
git clone https://gitee.com/DreamMaoMao/mydwl.git

cd mydwl

//编译安装
meson setup --buildtype=release build  -Dprefix=/usr/local
sudo ninja -C build install

//拷贝配置
cp swaync -r ~/.config/
cp fish -r ~/.config/
cp eww -r ~/.config/

sed -i s#/home/user#$HOME#g dwl.desktop
sudo cp dwl.desktop /usr/share/wayland-sessions/
```

# 终端设置(默认用的foot)
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
