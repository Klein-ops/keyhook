#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <string.h>

typedef struct {
    char *device;
    int keycode;
    int count;
    int timeout_ms;
    int debounce_ms;
    char *script;
    int exit_after_trigger;
    int verbose;
    int mode;
    int long_press_ms;
} Config;

void usage(char *name) {
    fprintf(stderr, "Usage: %s -d <dev> -k <key> -s <script> [options]\n", name);
    fprintf(stderr, "  -d  input device path\n");
    fprintf(stderr, "  -k  keycode to monitor\n");
    fprintf(stderr, "  -s  script to execute\n");
    fprintf(stderr, "  -m  mode: 'multi' (default) or 'long'\n");
    fprintf(stderr, "  -c  trigger count for multi-click (default:3)\n");
    fprintf(stderr, "  -t  timeout ms for multi-click (default:1000)\n");
    fprintf(stderr, "  -i  debounce ms (default:50)\n");
    fprintf(stderr, "  -l  long press duration ms (default:3000)\n");
    fprintf(stderr, "  -e  exit after trigger\n");
    fprintf(stderr, "  -v  verbose (print debug info)\n");
    fprintf(stderr, "  -h  show this help\n");
    exit(1);
}

long time_diff_ms(struct timeval *now, struct timeval *last) {
    return (now->tv_sec - last->tv_sec) * 1000 + (now->tv_usec - last->tv_usec) / 1000;
}

void execute_script(Config *cfg) {
    if (cfg->verbose) fprintf(stderr, "[DEBUG] Executing: sh %s\n", cfg->script);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "sh %s", cfg->script);
    system(cmd);
}

void monitor_multi(int fd, Config *cfg) {
    struct input_event ev;
    struct timeval last_ts = {0};
    int press_count = 0;
    int first_press = 1;
    
    if (cfg->verbose) fprintf(stderr, "[DEBUG] Multi-click mode, count=%d, timeout=%dms\n", cfg->count, cfg->timeout_ms);
    
    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type != EV_KEY || ev.code != cfg->keycode || ev.value != 1) continue;
        
        if (first_press) {
            press_count = 1;
            last_ts = ev.time;
            first_press = 0;
            continue;
        }
        
        long diff = time_diff_ms(&ev.time, &last_ts);
        if (diff < cfg->debounce_ms) continue;
        if (diff > cfg->timeout_ms) {
            press_count = 0;
            first_press = 1;
            continue;
        }
        
        press_count++;
        last_ts = ev.time;
        
        if (press_count >= cfg->count) {
            execute_script(cfg);
            if (cfg->exit_after_trigger) exit(0);
            press_count = 0;
            first_press = 1;
        }
    }
}

void monitor_long(int fd, Config *cfg) {
    struct input_event ev;
    struct timeval down_time = {0};
    int key_pressed = 0;
    fd_set fds;
    struct timeval timeout;
    long elapsed_ms;
    
    if (cfg->verbose) fprintf(stderr, "[DEBUG] Long-press mode, duration=%dms (trigger on hold)\n", cfg->long_press_ms);
    
    while (1) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        
        if (!key_pressed) {
            if (select(fd + 1, &fds, NULL, NULL, NULL) < 0) break;
        } else {
            struct timeval now;
            gettimeofday(&now, NULL);
            elapsed_ms = time_diff_ms(&now, &down_time);
            
            if (elapsed_ms >= cfg->long_press_ms) {
                if (cfg->verbose) fprintf(stderr, "[DEBUG] Long-press triggered at %ldms\n", elapsed_ms);
                execute_script(cfg);
                if (cfg->exit_after_trigger) exit(0);
                key_pressed = 0;
                continue;
            }
            
            long remain_ms = cfg->long_press_ms - elapsed_ms;
            timeout.tv_sec = remain_ms / 1000;
            timeout.tv_usec = (remain_ms % 1000) * 1000;
            
            if (select(fd + 1, &fds, NULL, NULL, &timeout) < 0) break;
            
            if (!FD_ISSET(fd, &fds)) {
                if (cfg->verbose) fprintf(stderr, "[DEBUG] Long-press timeout, triggering\n");
                execute_script(cfg);
                if (cfg->exit_after_trigger) exit(0);
                key_pressed = 0;
                continue;
            }
        }
        
        if (read(fd, &ev, sizeof(ev)) <= 0) break;
        
        if (ev.type != EV_KEY || ev.code != cfg->keycode) continue;
        
        if (ev.value == 1) {
            if (!key_pressed) {
                key_pressed = 1;
                down_time = ev.time;
                if (cfg->verbose) fprintf(stderr, "[DEBUG] Key down at %ld.%06ld\n", ev.time.tv_sec, ev.time.tv_usec);
            }
        }
        else if (ev.value == 0) {
            if (key_pressed) {
                key_pressed = 0;
                if (cfg->verbose) {
                    struct timeval now = ev.time;
                    long duration = time_diff_ms(&now, &down_time);
                    fprintf(stderr, "[DEBUG] Key up after %ldms (no trigger)\n", duration);
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    Config cfg = {
        .count = 3,
        .timeout_ms = 1000,
        .debounce_ms = 50,
        .exit_after_trigger = 0,
        .verbose = 0,
        .mode = 0,
        .long_press_ms = 3000
    };
    int opt;
    
    while ((opt = getopt(argc, argv, "d:k:s:c:t:i:m:l:evh")) != -1) {
        switch (opt) {
            case 'd': cfg.device = optarg; break;
            case 'k': cfg.keycode = atoi(optarg); break;
            case 's': cfg.script = optarg; break;
            case 'c': cfg.count = atoi(optarg); break;
            case 't': cfg.timeout_ms = atoi(optarg); break;
            case 'i': cfg.debounce_ms = atoi(optarg); break;
            case 'm':
                if (strcmp(optarg, "long") == 0) cfg.mode = 1;
                else if (strcmp(optarg, "multi") == 0) cfg.mode = 0;
                else usage(argv[0]);
                break;
            case 'l': cfg.long_press_ms = atoi(optarg); break;
            case 'e': cfg.exit_after_trigger = 1; break;
            case 'v': cfg.verbose = 1; break;
            case 'h': usage(argv[0]);
            default: usage(argv[0]);
        }
    }
    
    if (!cfg.device || !cfg.keycode || !cfg.script) usage(argv[0]);
    
    int fd = open(cfg.device, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    
    if (cfg.verbose) fprintf(stderr, "[DEBUG] Monitoring device: %s, keycode: %d\n", cfg.device, cfg.keycode);
    
    if (cfg.mode == 0) monitor_multi(fd, &cfg);
    else monitor_long(fd, &cfg);
    
    close(fd);
    return 0;
}