#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <ncurses.h>
#include <cstdlib>
#include <unistd.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <cstring>

namespace fs = std::filesystem;

int mpv_sock = -1;

std::vector<std::string> get_songs(const std::string &path) {
    std::vector<std::string> songs;
    for (const auto &entry : fs::directory_iterator(path)) {
        if (entry.path().extension() == ".mp3")
            songs.push_back(entry.path().filename().string());
    }
    return songs;
}

bool connect_mpv() {
    if (mpv_sock != -1) close(mpv_sock);
    mpv_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (mpv_sock < 0) return false;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/mpvsocket");

    if (connect(mpv_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(mpv_sock);
        mpv_sock = -1;
        return false;
    }
    fcntl(mpv_sock, F_SETFL, O_NONBLOCK); // non-blocking
    return true;
}

void mpv_send(const std::string &cmd) {
    if (mpv_sock == -1) return;
    std::string send_str = cmd + "\n";
    send(mpv_sock, send_str.c_str(), send_str.size(), 0);
}

double mpv_get_number(const std::string &prop) {
    if (mpv_sock == -1) return 0;
    std::string cmd = "{\"command\": [\"get_property\", \"" + prop + "\"]}";
    mpv_send(cmd);

    char buffer[512];
    int bytes = recv(mpv_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = 0;
        std::string output(buffer);
        size_t pos = output.find("\"data\":");
        if (pos != std::string::npos) {
            std::stringstream ss(output.substr(pos + 7));
            double val;
            ss >> val;
            return val;
        }
    }
    return 0;
}

void play_song(const std::string &song_path) {
    system("pkill mpv");
    usleep(200000);
    std::string cmd = "mpv --no-video --quiet --no-terminal \"" + song_path + "\" --input-ipc-server=/tmp/mpvsocket &";
    system(cmd.c_str());
    usleep(500000);
    connect_mpv();
}

void draw_box(int starty, int startx, int height, int width) {
    mvaddch(starty, startx, ACS_ULCORNER);
    mvaddch(starty, startx + width - 1, ACS_URCORNER);
    mvaddch(starty + height - 1, startx, ACS_LLCORNER);
    mvaddch(starty + height - 1, startx + width - 1, ACS_LRCORNER);

    for (int i = 1; i < width - 1; i++) {
        mvaddch(starty, startx + i, ACS_HLINE);
        mvaddch(starty + height - 1, startx + i, ACS_HLINE);
    }
    for (int i = 1; i < height - 1; i++) {
        mvaddch(starty + i, startx, ACS_VLINE);
        mvaddch(starty + i, startx + width - 1, ACS_VLINE);
    }
}

void draw_ui(const std::vector<std::string> &songs, int highlight, const std::string &current_song, double pos, double dur, int volume) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    draw_box(0, 0, height - 2, width);

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(1, 2, "🎵 Terminal Music Player");
    attroff(COLOR_PAIR(2) | A_BOLD);

    int now_playing_height = 6;
    int np_start_y = 3;
    int np_width = width - 4;
    draw_box(np_start_y, 2, now_playing_height, np_width);

    attron(COLOR_PAIR(3));
    mvprintw(np_start_y + 1, 4, "▶ Now Playing:");
    if (!current_song.empty()) {
        mvprintw(np_start_y + 2, 4, "%.*s", np_width - 8, current_song.c_str());
        mvprintw(np_start_y + 3, 4, "Duration: %02d:%02d", (int)dur / 60, (int)dur % 60);
        mvprintw(np_start_y + 4, 4, "Position: %02d:%02d", (int)pos / 60, (int)pos % 60);
        mvprintw(np_start_y + 4, np_width - 18, "Vol: %d%%", volume);
    } else {
        mvprintw(np_start_y + 2, 4, "No song playing");
    }
    attroff(COLOR_PAIR(3));

    int pl_start_y = np_start_y + now_playing_height + 1;
    int pl_height = height - pl_start_y - 4;
    int pl_width = width - 4;
    draw_box(pl_start_y, 2, pl_height, pl_width);

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(pl_start_y, 4, "Playlist:");
    attroff(COLOR_PAIR(2) | A_BOLD);

    int max_display = pl_height - 2;
    int start_index = 0;
    if (highlight >= max_display) start_index = highlight - max_display + 1;

    for (int i = 0; i < max_display && (start_index + i) < (int)songs.size(); i++) {
        if (start_index + i == highlight)
            attron(A_REVERSE);
        mvprintw(pl_start_y + 1 + i, 4, "%s", songs[start_index + i].c_str());
        attroff(A_REVERSE);
    }

    attron(COLOR_PAIR(1));
    mvprintw(height - 2, 2, "Controls: ↑/↓ Move  Enter Play  p Pause  n Next  ←/→ Seek  +/- Vol  q Quit");
    attroff(COLOR_PAIR(1));

    refresh();
}

int main() {
    std::string folder;
    std::cout << "Enter music folder path (e.g. /storage/emulated/0/Music): ";
    std::getline(std::cin, folder);

    auto songs = get_songs(folder);
    if (songs.empty()) {
        std::cout << "No MP3 files found in folder.\n";
        return 1;
    }

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    use_default_colors();

    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_CYAN, -1);

    nodelay(stdscr, TRUE);

    int highlight = 0;
    bool running = true;
    std::string current_song;
    double pos = 0, dur = 0;
    int volume = 50;

    while (running) {
        if (mpv_sock != -1 && !current_song.empty()) {
            pos = mpv_get_number("time-pos");
            dur = mpv_get_number("duration");
        } else {
            pos = dur = 0;
        }

        draw_ui(songs, highlight, current_song, pos, dur, volume);

        int ch = getch();
        switch (ch) {
            case KEY_UP:
                highlight = (highlight == 0) ? songs.size() - 1 : highlight - 1;
                break;
            case KEY_DOWN:
                highlight = (highlight == songs.size() - 1) ? 0 : highlight + 1;
                break;
            case 10: // Enter
                current_song = songs[highlight];
                play_song(folder + "/" + current_song);
                break;
            case 'p':
                mpv_send("{\"command\": [\"cycle\", \"pause\"]}");
                break;
            case 'n':
                highlight = (highlight + 1) % songs.size();
                current_song = songs[highlight];
                play_song(folder + "/" + current_song);
                break;
            case KEY_RIGHT:
                mpv_send("{\"command\": [\"seek\", 5]}");
                break;
            case KEY_LEFT:
                mpv_send("{\"command\": [\"seek\", -5]}");
                break;
            case '+':
                volume += 5;
                if (volume > 150) volume = 150;
                mpv_send("{\"command\": [\"set_property\", \"volume\", " + std::to_string(volume) + "]}");
                break;
            case '-':
                volume -= 5;
                if (volume < 0) volume = 0;
                mpv_send("{\"command\": [\"set_property\", \"volume\", " + std::to_string(volume) + "]}");
                break;
            case 'q':
                running = false;
                break;
        }

        usleep(100000);
    }

    endwin();
    system("pkill mpv");
    return 0;
}
