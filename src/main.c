#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <wait.h>

#include "../config.h"
#include "interface.h"
#include "match.h"
#include "options.h"
#include "term.h"

#define LIST_IMPL
#include "types.h"

typedef struct {
    int fd;
    int pid;
} Proc;

static String query = {0};
static CliArgs options = {0};
static Scores matches = {0};
static Proc input = {0};
static Terminal *term = NULL;
static Entries *entries = NULL;


// input buffer
static char i_buf[MAX_TEXT_LEN];

ssize_t readline (int fd, char *buf, ssize_t maxlen);
int open_input_stream(Proc *p);
void close_input_stream (Proc *p);

static inline void cleanup () {
    list_free(entries);
    if (term != NULL) destroy_term(term);
    close_input_stream(&input);
}

void handle_sig () {
    draw_outline(&options, term);
    draw_query(&options, term, &query);
    draw_entries(term, &matches);
    draw_preview(&options, term, &matches);
    term_flush(term);
}

// KEybindings
static void action_up () {
    select_ent(selected == matches.length - 1? 0 : selected + 1);
    draw_entries(term, &matches);
    draw_preview(&options, term, &matches);
    term_flush(term);
}

static void action_down () {
    select_ent(selected == 0? matches.length - 1 : selected - 1);
    draw_entries(term, &matches);
    draw_preview(&options, term, &matches);
    term_flush(term);
}

static void action_enter () {
    char *output = query.bytes;
    if (selected < matches.length) output = matches.pairs[selected].entry;
    destroy_term(term); // we need to close output streams to write
    term = NULL;
    printf("%s\n", output);
    cleanup(); 
    exit(0);
}

static void action_left () {
    query.cursor_pos = MAX(0, query.cursor_pos - 1);
    draw_query(&options, term, &query);
    term_flush(term);
}

static void action_right () {
    query.cursor_pos = MIN(query.length, query.cursor_pos + 1);
    draw_query(&options, term, &query);
    term_flush(term);
}

static void action_del_wrd (int *p_timeout, int *midx) {
    for (int i = query.cursor_pos; i >= 0; i--) {
        if (query.bytes[i] == ' ' || i == 0) {
            shift_string(&query, i - query.cursor_pos);
            query.cursor_pos = i;
            break;
        }
    }

    // reset entries
    *midx = 0;
    matches.length = 0;
    *p_timeout = 0;

    draw_query(&options, term, &query);
    term_flush(term);
}

static void action_del_all (int *p_timeout, int *midx) {
    query.length = 0;
    query.bytes[0] = 0;
    query.cursor_pos = 0;

    // reset entries
    *midx = 0;
    matches.length = 0;
    *p_timeout = 0;

    draw_query(&options, term, &query);
    term_flush(term);
}

static void action_del (int *p_timeout, int *midx) {
    if (query.cursor_pos == 0) return;

    shift_string(&query, -1);
    query.cursor_pos -= 1;
    
    // reset entries
    *midx = 0;
    matches.length = 0;
    *p_timeout = 0;

    draw_query(&options, term, &query);
    term_flush(term);
}

struct keybinding { const char *key; void (*handler)(int*, int*); };

#define CTRL_KEY(key) (const char[]){key - 'a' + 1 , '\0'}
#define IS_CHAR(c) (c < 127 && c > 31)
#define BACKSPACE (const char[]){ 127 , '\0' }

static const struct keybinding keybindings[] = {
    {CTRL_KEY('m'), 	action_enter},
    {CTRL_KEY('n'), 	action_down},
    {CTRL_KEY('j'), 	action_down},
    {CTRL_KEY('p'), 	action_up},
    {CTRL_KEY('k'), 	action_up},
    {CTRL_KEY('w'), 	action_del_wrd},
    {CTRL_KEY('u'), 	action_del_all},
    {BACKSPACE,     	action_del},
    {"\x1b[D",      	action_left},
    {"\x1bOD",      	action_left},
    {"\x1bOC",      	action_right},
    {"\x1b[C",      	action_right},
    {"\x1b[A",      	action_up},
    {"\x1bOA",      	action_up},
    {"\x1b[B",      	action_down},
    {"\x1bOB",      	action_down},
};

int main(int argc, char **argv) {
    get_options(&options, argv, argc);
    open_input_stream(&input);

    signal(SIGWINCH, handle_sig);

    term = create_term();
    entries = list_create(10);

    struct {
        struct pollfd term;
        struct pollfd input;
    } fds; // this is the same as pollfd fds[2] but allows for key indexing :)

    fds.term.fd = term->input;
    fds.term.events = POLLIN;

    fds.input.fd = input.fd;
    fds.input.events = POLLIN;
    
    int p_ret;
    int p_timeout = -1; // blocking poll
    int midx = 0;
    nfds_t nfds = 2;
    
    draw_outline(&options, term);
    draw_query(&options, term, &query);
    term_flush(term);

    while ((p_ret = poll((struct pollfd*)&fds, nfds, p_timeout)) >= 0 || errno == EINTR) {
        if (p_ret == -1) continue; // the syscall was blocked by a signal, run it again
        if (p_ret == 0) {
            // nothing was polled, do scoring
            for (const int i = midx + SCORING_STEP; midx < i && midx < entries->length; midx++) {
                if (has_match(query.bytes, entries->items[midx])) {
                    score_t score = score_match(query.bytes, entries->items[midx]);
                    matches.pairs[matches.length].entry = entries->items[midx];
                    matches.pairs[matches.length].score = score;
                    matches.length++;
                }
            }

            qsort(matches.pairs, matches.length, sizeof(struct pair_t), cmp_pairs);

            if (midx == entries->length) {
                p_timeout = -1; // if no more jobs do a blocking poll, otherwise do immediate poll
            } else {
                p_timeout = 0; 
            }

            draw_entries(term, &matches);
            draw_preview(&options, term, &matches);
            term_flush(term);

            continue;
        }

        if (fds.term.revents & POLLIN) {
            // key press
            char key_buf[4] = {0};
            key_buf[0] = term_read(term);

            // check if we have an escape sequence and ready bytes.
            if (key_buf[0] == '\x1b') {
                int ret = poll(&fds.term, 1, INPUT_TIMEOUT);

                if (ret < 0) break; // fuck off on errors
                if (ret == 1) term_read_buf(term, key_buf + 1, 2);
            }


            if (strcmp(key_buf, "\x1b") == 0) goto loop_end;
            else if (IS_CHAR(key_buf[0])) {
                if (shift_string(&query, 1) == true) {
                    query.bytes[query.cursor_pos++] = key_buf[0];

                    // reset entries
                    midx = 0;
                    matches.length = 0;
                    p_timeout = 0;
                    
                    draw_query(&options, term, &query);
                    term_flush(term);
                }
            } else {
                static const int n = sizeof(keybindings) / sizeof(struct keybinding);
                for (uint i = 0; i < n; i++) {
                    // a state struct should probably be used but meh
                    if (strcmp(key_buf, keybindings[i].key) == 0) keybindings[i].handler(&p_timeout, &midx); 
                }
            }
        } 

        if ((fds.input.revents & POLLHUP) && !(fds.input.revents & POLLIN)) {
            nfds = 1; // peer hung up and no additional data, stop polling this file
        }

        if (fds.input.revents & POLLIN) {
            // new line
            ssize_t n = readline(input.fd, i_buf, MAX_TEXT_LEN);

            if (n == -1) perror("fuck something broke!");
            else if (n > 0) list_append(entries, i_buf, n); // don't add empty strings

            p_timeout = 0;
        }
    }

loop_end:

    if (p_ret < 0) {
        perror("Poll");
    }

    cleanup();

    return 1;
}


ssize_t readline (int fd, char *buf, ssize_t maxlen) {
    ssize_t i = 0;
    int c;
    int ret;

    while ((ret = read(fd, &c, 1)) > 0) {
        if (c == '\n')
            break;

        if (i < maxlen - 1)
            buf[i++] = (char)c;
        else 
            break;
    }

    buf[i] = '\0';

    if (ret < 0) return -1; // return errors
    return i;
}

int open_input_stream (Proc *p) {
    if (isatty(STDIN_FILENO)) { // no input piped, use a preset command.
        int fds[2]; // 0 read, 1 write
        int ret = pipe(fds);

        if (ret == -1) {
            perror("pipe");
            return -1;
        }

        ret = fork();

        if (ret == 0) {
            // new proc
            dup2(fds[1], STDOUT_FILENO);
            close(fds[0]);

            static char *const argv[] = { "/bin/sh", "-c", FALLBACK_COMMAND, NULL };

            execvp("/bin/sh", argv); // change this to use shell
            perror("execvp");
            return -1; // this should be unreachable
        } else if (ret > 0) {
            // parent proc
            close(fds[1]);

            p->fd = fds[0];
            p->pid = ret;
            return 0;
        } else {
            perror("fork");
            return -1;
        }
    } else {
        p->fd = STDIN_FILENO;
        p->pid = -1;
        return 0;
    }
}

void close_input_stream (Proc *p) {
    if (isatty(STDIN_FILENO)) {
        close(p->fd);
        int status;
        waitpid(p->pid, &status, 0); // consider killing the process here
    }
}
