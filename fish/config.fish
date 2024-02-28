if status is-interactive
    # atuin init fish | source
    source ~/.config/fish/conf.d/atuin.fish
    # Commands to run in interactive sessions can go here
end

function fish_greeting
end
set -g fish_color_autosuggestion purple


alias ys='yay -Sy'
alias yr='yay -R'
alias s='sudo'
alias z='zellij'
alias y='yazi'
export PATH=:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/home/wrq/.local/bin:

# add extra PATH
fish_add_path /home/wrq/.nvm/versions/node/v18.14.2/bin
fish_add_path /opt/go/bin
fish_add_path /home/wrq/.cargo/bin

zoxide init fish --cmd j | source


