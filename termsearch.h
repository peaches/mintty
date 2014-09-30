#ifndef TERMSEARCH_H
#define TERMSEARCH_H

#include "term.h"
#include "winpriv.h"

#define IDC_EDIT  2000

typedef struct {
  pos start;
  pos end;
} single_result;

struct search_results {
  int current;
  int matches;
  int capacity;
  single_result *results;
};

void init_search(void);
single_result * contained_in_results(pos position);
bool is_current_result(single_result * result);
void search_scrollback(void);
void add_result(int startx, int starty, int endx, int endy);
void next_result(void);
void prev_result(void);
bool search_should_translate(MSG * msg);
bool search_control_displayed(void);
void focus_search_control(void);
void toggle_search_control(void);
HBRUSH set_search_control_color(WPARAM wp, LPARAM lp);
void remove_search_control_subclassing(void);
RECT search_control_rectangle(void);
void reposition_search_control(void);

#endif
