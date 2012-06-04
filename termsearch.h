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
  int matches;
  int capacity;
  single_result *results;
};

bool contained_in_results(pos position);
void search_scrollback(void);
void add_result(int startx, int starty, int endx, int endy);
void init_search(void);
bool search_control_active(void);
void toggle_search_control(void);
void remove_search_control_subclassing(void);
RECT search_control_rectangle(void);
void reposition_search_control(void);

#endif
