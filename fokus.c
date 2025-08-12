/*
 * fokus - A minimalist terminal‐based focus timer and stopwatch with daily logging, built on ncurses.
 * Copyright (C) 2025  Arda Yılmaz
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <ncurses.h>

#include <unistd.h>

#include <time.h>

#include <stdbool.h>

#include <sys/time.h>

#include <string.h>

#include <stdio.h>

#include <sys/stat.h>

#include <pwd.h>

#include <stdlib.h>

#include <fcntl.h>

#include <errno.h>

#include <sys/file.h>

#define MAX_LOG_ENTRIES 36500

typedef struct {
  char date[11];
  int minutes;
}
LogEntry;

char log_filepath[512];
char lock_filepath[512];
char config_filepath[512];

const char * config_header =
  "# fokus config file\n"
"\n"
"# Default countdown timer duration (in minutes)\n"
"# Must be between 1 and 999\n"
"default-timer=30\n";

int read_or_create_config() {
  FILE * f = fopen(config_filepath, "r");
  int default_timer_val = 30;

  if (!f) {

    f = fopen(config_filepath, "w");
    if (f) {
      fputs(config_header, f);
      fclose(f);
    }
    return default_timer_val;
  } else {
    char line[128];
    while (fgets(line, sizeof(line), f)) {
      int val;
      if (sscanf(line, "default-timer=%d", & val) == 1) {
        if (val < 1) val = 1;
        if (val > 999) val = 999;
        default_timer_val = val;
        break;
      }
    }
    fclose(f);
  }
  return default_timer_val;
}

void init_log_path() {
  const char * home = getenv("HOME");
  if (!home) {
    struct passwd * pw = getpwuid(getuid());
    home = pw ? pw -> pw_dir : ".";
  }

  char config_dir[512];
  snprintf(config_dir, sizeof(config_dir), "%s/.config/fokus", home);
  struct stat st = {
    0
  };
  if (stat(config_dir, & st) == -1) {
    char home_config[512];
    snprintf(home_config, sizeof(home_config), "%s/.config", home);
    if (stat(home_config, & st) == -1) {
      if (mkdir(home_config, 0700) == -1 && errno != EEXIST) {
        fprintf(stderr, "Failed to create directory %s\n", home_config);
        exit(1);
      }
    }
    if (mkdir(config_dir, 0700) == -1 && errno != EEXIST) {
      fprintf(stderr, "Failed to create directory %s\n", config_dir);
      exit(1);
    }
  }

  snprintf(log_filepath, sizeof(log_filepath), "%s/fokus.log", config_dir);
  snprintf(lock_filepath, sizeof(lock_filepath), "%s/fokus.lock", config_dir);
  snprintf(config_filepath, sizeof(config_filepath), "%s/fokus.conf", config_dir);

  FILE * f = fopen(log_filepath, "a");
  if (f) fclose(f);

  FILE * lf = fopen(lock_filepath, "a");
  if (lf) fclose(lf);
}

int read_logs(LogEntry E[], int max) {
  FILE * f = fopen(log_filepath, "r");
  if (!f) return 0;
  int cnt = 0;
  while (cnt < max && fscanf(f, "%10s %d", E[cnt].date, & E[cnt].minutes) == 2) {
    cnt++;
  }
  fclose(f);
  return cnt;
}

void save_log(const char * d, int add) {
  LogEntry E[MAX_LOG_ENTRIES];
  int cnt = read_logs(E, MAX_LOG_ENTRIES);
  bool found = false;
  for (int i = 0; i < cnt; i++) {
    if (!strcmp(E[i].date, d)) {
      E[i].minutes += add;
      found = true;
      break;
    }
  }
  if (!found && cnt < MAX_LOG_ENTRIES) {
    strcpy(E[cnt].date, d);
    E[cnt].minutes = add;
    cnt++;
  }

  FILE * f = fopen(log_filepath, "w");
  if (!f) {
    fprintf(stderr, "Failed to open log file for writing.\n");
    return;
  }
  if (flock(fileno(f), LOCK_EX) != 0) {
    fprintf(stderr, "Failed to lock log file for writing.\n");
    fclose(f);
    return;
  }
  for (int i = 0; i < cnt; i++) {
    fprintf(f, "%s %d\n", E[i].date, E[i].minutes);
  }
  fflush(f);
  flock(fileno(f), LOCK_UN);
  fclose(f);
}

void draw_frame(int top, int left, int height, int width) {
  for (int i = 0; i < width; i++) {
    mvaddch(top, left + i, ACS_HLINE);
    mvaddch(top + height - 1, left + i, ACS_HLINE);
  }
  for (int i = 0; i < height; i++) {
    mvaddch(top + i, left, ACS_VLINE);
    mvaddch(top + i, left + width - 1, ACS_VLINE);
  }
  mvaddch(top, left, ACS_ULCORNER);
  mvaddch(top, left + width - 1, ACS_URCORNER);
  mvaddch(top + height - 1, left, ACS_LLCORNER);
  mvaddch(top + height - 1, left + width - 1, ACS_LRCORNER);
}

void draw_slider(int top, int height, int total_lines, int offset) {
  if (total_lines <= height) return;

  float ratio = (float) height / total_lines;
  int slider_height = (int)(height * ratio);
  if (slider_height < 1) slider_height = 1;

  int slider_pos = (int)((float) offset / (total_lines - height) * (height - slider_height));
  if (slider_pos < 0) slider_pos = 0;
  if (slider_pos + slider_height > height)
    slider_pos = height - slider_height;

  for (int i = 0; i < height; i++) {
    if (i >= slider_pos && i < slider_pos + slider_height)
      mvaddch(top + i, COLS - 3, ACS_CKBOARD);
    else
      mvaddch(top + i, COLS - 3, ' ');
  }
}

void draw_stopwatch(int s, int ms, int rows, int cols) {
  int m = s / 60, sec = s % 60;
  char lbl[] = "Stopwatch:";
  int base = rows / 2;
  int total_len = (int) strlen(lbl) + 1 + 8;
  int col = (cols - total_len) / 2;

  int frame_h = 3;
  int frame_w = 30;
  int frame_top = base - 1;
  int frame_left = (cols - frame_w) / 2;
  draw_frame(frame_top, frame_left, frame_h, frame_w);

  attron(COLOR_PAIR(1));
  mvprintw(base, col, "%s", lbl);
  attroff(COLOR_PAIR(1));
  char ts[16];
  snprintf(ts, sizeof(ts), "%02d:%02d.%02d", m, sec, ms);
  mvprintw(base, col + (int) strlen(lbl) + 1, "%s", ts);
}

void draw_timer(int s, int ms, int rows, int cols, bool fin) {
  if (s < 0) s = 0;
  int m = s / 60, sec = s % 60;
  char lbl[] = "Timer:";
  int base = rows / 2;
  int total_len = (int) strlen(lbl) + 1 + 8;
  int col = (cols - total_len) / 2;

  int frame_h = 3;
  int frame_w = 30;
  int frame_top = base - 1;
  int frame_left = (cols - frame_w) / 2;
  draw_frame(frame_top, frame_left, frame_h, frame_w);

  attron(COLOR_PAIR(1));
  mvprintw(base, col, "%s", lbl);
  attroff(COLOR_PAIR(1));
  char ts[16];
  snprintf(ts, sizeof(ts), " %02d:%02d.%02d", m, sec, ms);
  if (fin) {
    attron(COLOR_PAIR(4));
    mvprintw(base, col + (int) strlen(lbl), "%s", ts);
    attroff(COLOR_PAIR(4));
  } else {
    mvprintw(base, col + (int) strlen(lbl), "%s", ts);
  }
}

void draw_minutes(int m, int rows, int cols) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%d minutes focused today", m);
  attron(COLOR_PAIR(5));
  int len = (int) strlen(buf);
  int col = (cols - len) / 2;
  mvprintw(rows / 2 + 2, col, "%s", buf);
  attroff(COLOR_PAIR(5));
}

void draw_logs(int rows, int cols, int offset) {
  LogEntry E[MAX_LOG_ENTRIES];
  int cnt = read_logs(E, MAX_LOG_ENTRIES);

  int frame_top = 2;
  int frame_left = 2;
  int frame_h = rows - 4;
  int frame_w = cols - 5;

  draw_frame(frame_top, frame_left, frame_h, frame_w);

  char hdr[] = "Date        Minutes";
  int visible_lines = frame_h - 2;

  attron(COLOR_PAIR(1));
  mvprintw(frame_top + 1, frame_left + (frame_w - (int) strlen(hdr)) / 2, "%s", hdr);
  attroff(COLOR_PAIR(1));

  int display_count = cnt - offset;
  if (display_count > visible_lines - 1)
    display_count = visible_lines - 1;
  if (display_count < 0)
    display_count = 0;

  for (int i = 0; i < display_count; i++) {
    char ln[64];
    snprintf(ln, sizeof(ln), "%-12s %6d", E[offset + i].date, E[offset + i].minutes);
    mvprintw(frame_top + 2 + i, frame_left + (frame_w - (int) strlen(ln)) / 2, "%s", ln);
  }

  draw_slider(frame_top + 1, visible_lines, cnt, offset);
}

void init_colors() {
  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_GREEN, -1);
    init_pair(2, COLOR_WHITE, -1);
    init_pair(3, COLOR_CYAN, -1);
    init_pair(4, COLOR_MAGENTA, -1);
    init_pair(5, COLOR_YELLOW, -1);
  }
}

void draw_page_indicator(int rows, int cols, int page) {
  char pg[32];
  snprintf(pg, sizeof(pg), "< Page %d of 3 >", page);
  attron(COLOR_PAIR(3));
  mvprintw(1, (cols - (int) strlen(pg)) / 2, "%s", pg);
  attroff(COLOR_PAIR(3));
}

void draw_footer(int rows, int cols) {
  const char * full_ft = "[space] Start/Reset  [q] Quit  [h]/[l] Change Page  [j]/[k] Adjust/Scroll";
  const char * short_ft = "[space] [q] [h]/[l] [j]/[k]";

  int len_full = (int) strlen(full_ft);
  int len_short = (int) strlen(short_ft);

  mvhline(rows - 2, 0, ' ', cols);

  attron(COLOR_PAIR(2));
  if (cols >= len_full) {
    mvprintw(rows - 2, (cols - len_full) / 2, "%s", full_ft);
  } else if (cols >= len_short) {
    mvprintw(rows - 2, (cols - len_short) / 2, "%s", short_ft);
  }
  attroff(COLOR_PAIR(2));
}

int main() {
  initscr();
  cbreak();
  noecho();
  curs_set(0);
  nodelay(stdscr, TRUE);
  keypad(stdscr, TRUE);

  init_log_path();

  int lock_fd = open(lock_filepath, O_RDWR | O_CREAT, 0600);
  if (lock_fd == -1) {
    endwin();
    fprintf(stderr, "Cannot open lock file for locking.\n");
    return 1;
  }
  if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    while (1) {
      erase();
      int rows = getmaxy(stdscr), cols = getmaxx(stdscr);
      const char * msg = "Another instance of fokus is already running.";
      const char * msg2 = "Press 'q' to quit.";
      mvprintw(rows / 2, (cols - (int) strlen(msg)) / 2, "%s", msg);
      mvprintw(rows / 2 + 1, (cols - (int) strlen(msg2)) / 2, "%s", msg2);
      refresh();

      int ch = getch();
      if (ch == 'q') {
        endwin();
        close(lock_fd);
        return 0;
      }
      usleep(100000);
    }
  }

  init_colors();

  int setm = read_or_create_config();

  struct timeval last;
  gettimeofday( & last, NULL);

  time_t now = time(NULL);
  struct tm * tm = localtime( & now);

  static char today[11];
  if (today[0] == '\0') {
    snprintf(today, sizeof(today), "%04d-%02d-%02d",
      tm -> tm_year + 1900, tm -> tm_mon + 1, tm -> tm_mday);
  }

  LogEntry tmp[MAX_LOG_ENTRIES];
  int lc = read_logs(tmp, MAX_LOG_ENTRIES), today_m = 0;
  for (int i = 0; i < lc; i++) {
    if (!strcmp(tmp[i].date, today)) {
      today_m = tmp[i].minutes;
      break;
    }
  }

  int es = 0, ems = 0;
  int cd_s = setm * 60, cd_ms = 0;
  bool run_sw = false, run_tm = false, countdown_logged = false;
  int page = 1, log_offset = 0;

  while (1) {
    erase();
    int rows = getmaxy(stdscr), cols = getmaxx(stdscr);
    draw_page_indicator(rows, cols, page);

    int ch = getch();
    if (ch == 'q') break;

    if (!run_sw && !run_tm) {
      if (ch == 'h' && page > 1) page--;
      if (ch == 'l' && page < 3) page++;
    }

    if (page == 3) {
      if (ch == 'j') log_offset++;
      if (ch == 'k') log_offset--;
      lc = read_logs(tmp, MAX_LOG_ENTRIES);
      int max_off = lc > (rows - 5 - 2) ? lc - (rows - 5 - 2) : 0;
      if (log_offset < 0) log_offset = 0;
      if (log_offset > max_off) log_offset = max_off;
    } else {
      if (ch == ' ') {
        if (page == 1) {
          if (run_sw) {
            int mins = es / 60;
            if (mins > 0) {
              save_log(today, mins);
              lc = read_logs(tmp, MAX_LOG_ENTRIES);
              today_m = 0;
              for (int i = 0; i < lc; i++) {
                if (!strcmp(tmp[i].date, today)) {
                  today_m = tmp[i].minutes;
                  break;
                }
              }
            }
            run_sw = false;
            es = ems = 0;
          } else {
            run_sw = true;
            run_tm = false;
            countdown_logged = false;
          }
        } else if (page == 2) {
          if (run_tm || (cd_s == 0 && cd_ms == 0)) {
            run_tm = false;
            cd_s = setm * 60;
            cd_ms = 0;
            countdown_logged = false;
          } else {
            run_tm = true;
            run_sw = false;
          }
        }
      }
      if (page == 2 && !run_tm) {
        if (ch == 'k' && setm < 999) {
          setm++;
          cd_s = setm * 60;
          cd_ms = 0;
        }
        if (ch == 'j' && setm > 1) {
          setm--;
          cd_s = setm * 60;
          cd_ms = 0;
        }
      }
    }

    struct timeval cur;
    gettimeofday( & cur, NULL);
    int dms = (cur.tv_sec - last.tv_sec) * 1000 + (cur.tv_usec - last.tv_usec) / 1000;
    if (dms >= 10) {
      now = time(NULL);
      tm = localtime( & now);
      char cd[11];
      snprintf(cd, sizeof(cd), "%04d-%02d-%02d",
        tm -> tm_year + 1900, tm -> tm_mon + 1, tm -> tm_mday);

      if (strcmp(cd, today)) {

      }

      if (run_sw) {
        ems += dms;
        while (ems >= 1000) {
          es++;
          ems -= 1000;
        }
      }

      if (run_tm) {
        cd_ms -= dms;
        while (cd_ms < 0 && cd_s > 0) {
          cd_s--;
          cd_ms += 1000;
        }
        if (cd_s <= 0 && cd_ms <= 0 && !countdown_logged) {
          save_log(today, setm);
          lc = read_logs(tmp, MAX_LOG_ENTRIES);
          today_m = 0;
          for (int i = 0; i < lc; i++) {
            if (!strcmp(tmp[i].date, today)) {
              today_m = tmp[i].minutes;
              break;
            }
          }
          run_tm = false;
          countdown_logged = true;
          cd_s = cd_ms = 0;
        }
      }

      last = cur;
    }

    if (page == 1) {
      draw_stopwatch(es, ems / 10, rows, cols);
      if (!run_sw && !run_tm) draw_minutes(today_m, rows, cols);
    } else if (page == 2) {
      draw_timer(cd_s, cd_ms / 10, rows, cols, (cd_s == 0 && cd_ms <= 0));
      if (!run_tm) draw_minutes(today_m, rows, cols);
    } else {
      draw_logs(rows, cols, log_offset);
    }

    draw_footer(rows, cols);
    refresh();
    usleep(10000);
  }

  flock(lock_fd, LOCK_UN);
  close(lock_fd);
  endwin();
  return 0;
}
