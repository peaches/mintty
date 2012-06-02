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

extern struct search_results search_results;
extern HWND search_wnd;

bool contained_in_results(pos position);
void search_scrollback(void);
void add_result(int startx, int starty, int endx, int endy);
void init_search_results(void);
bool search_control_active(void);

#endif
