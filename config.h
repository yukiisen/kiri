#ifndef CONFIG_H
#define CONFIG_H

// penality
#define SCORE_GAP_LEADING -0.015
#define SCORE_GAP_TRAILING -0.01
#define SCORE_GAP_INNER -0.025
#define SCORE_MATCH_CONSECUTIVE 1.0
#define SCORE_MATCH_SLASH 0.9
#define SCORE_MATCH_WORD 0.8
#define SCORE_MATCH_CAPITAL 0.7
#define SCORE_MATCH_DOT 0.6

#define SCORING_STEP 400

// ui stuff
#define INPUT_TIMEOUT 50 // in ms
#define INTERFACE_GAP_VERTICAL 10
#define INTERFACE_GAP_HORIZONTAL 2
#define SCROLLOFF 10

#define DEFAULT_PROMPT "> "
#define DEFAULT_TTY "/dev/tty"
#define FALLBACK_COMMAND "ls"
#define DEFAULT_PREVIEW "cat {}"

#define BORDER_TOP_LEFT     "┌"
#define BORDER_TOP_RIGHT    "┐"
#define BORDER_BOTTOM_LEFT  "└"
#define BORDER_BOTTOM_RIGHT "┘"
#define BORDER_HORIZONTAL   "─"
#define BORDER_VERTICAL     "│"

#endif
