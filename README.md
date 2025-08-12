# fokus

A minimalist terminal‐based focus timer and stopwatch with daily logging, built on ncurses. 

## Features
- Stopwatch to track elapsed time while focusing.
- Countdown timer with adjustable duration.
- Daily log of minutes focused saved in a local file (~/.config/fokus.log).
- Interactive terminal UI with pages for stopwatch, timer, and logs.
- Vim-like control scheme.
- Distraction free minimalism.

## Screenshots
![Stopwatch Page](assets/stopwatch.png)

![Timer Page](assets/timer.png)

![Logs Page](assets/logs.gif)

## Prerequisites
- "ncurses" development library
- C compiler (e.g., gcc, clang)

#### Debian / Ubuntu
```bash
sudo apt update
sudo apt install build-essential libncurses-dev
```
#### Fedora
```bash
sudo dnf install gcc ncurses-devel
```

## Installation
#### Arch (AUR)
```bash
paru -S fokus
# or
yay -S fokus
```
#### Manual (Other Distros)
1. Clone the repository:
```bash
git clone https://git.sr.ht/~fijarom/fokus
```
2. Build the executable:
```bash
gcc fokus.c -o fokus -lncurses
```
3. (Optional) Install system-wide:
```bash
sudo install -Dm755 fokus /usr/local/bin/fokus
```
## Usage
Run the program:
```bash
./fokus
# or, if installed system-wide:
fokus
```
## Controls
- `[space]` : Start/Reset stopwatch or timer
- `[q]`     : Quit
- `[h]`/`[l]`: Switch pages
- `[j]`/`[k]`: Adjust timer minutes or scroll logs

## Configuration
Default timer duration is set in `~/.config/fokus/fokus.conf` under the `default-timer` key.

## LICENSE
This project is licensed under the terms of the GPL-3.0 license. See the [COPYING](./COPYING) file for details.

