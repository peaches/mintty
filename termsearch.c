
#include "termsearch.h"
#include "termpriv.h"

#include <commctrl.h>

#define F_KEY 0x46

struct search_results search_results;

HWND search_wnd;
WNDPROC default_edit_proc;

bool alt = false;
bool search_control_showing = true;
int search_width = 150;
int search_height = 20;
int margin = 1;

bool search_control_active(void)
{
  return GetFocus() == search_wnd;
}

void toggle_search_control(void)
{
  ShowWindow(search_wnd, search_control_showing ? SW_HIDE: SW_SHOW);
  search_control_showing = !search_control_showing;

  if (search_control_showing)
    SetFocus(search_wnd);

  search_scrollback();
}

// Get the area that the search control currently occupies.
RECT search_control_rectangle(void)
{
  RECT cr, search;

  if (search_control_showing) {
    GetClientRect(wnd, &cr);
    search.left = cr.right - search_width - margin;
    search.top = cr.bottom - search_height - margin;
    search.right = cr.right - margin;
    search.bottom = cr.bottom - margin;
  }
  else {
    search.left = 0;
    search.top = 0;
    search.right = 0;
    search.bottom = 0;
  }

  return search;
}

void init_search(void)
{
  RECT cr;
  GetClientRect(wnd, &cr);
  HINSTANCE inst = GetModuleHandle(NULL);

  search_wnd = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                              cr.right - search_width - margin, cr.bottom - search_height - margin,
                              search_width, search_height,
                              wnd, (HMENU)IDC_EDIT, inst, NULL);
  toggle_search_control();  // This first call hides the control

  // subclassing
  default_edit_proc = (WNDPROC)SetWindowLong(search_wnd, GWL_WNDPROC, (long)edit_proc);

  search_results.matches = 0;
  search_results.capacity = 2;  // Give it a capacity of 2, no particular reason
  search_results.results = newn(single_result, search_results.capacity);
}

void remove_search_control_subclassing(void)
{
  SetWindowLong(search_wnd, GWL_WNDPROC, (long)default_edit_proc);
}


// While the search box has focus, this allows Alt + F or Escape to
// hide it.
LRESULT edit_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg) {
    when WM_KEYDOWN or WM_SYSKEYDOWN:
      switch(wp) {
        when F_KEY:
          if (alt) {
            toggle_search_control();
            return 0;
          }
        when VK_MENU:
          alt = true;
          return 0;
        when VK_ESCAPE:
          toggle_search_control();
          return 0;
      }
    when WM_KEYUP or WM_SYSKEYUP:
      if (wp == VK_MENU) {
        alt = false;
        return 0;
      }
  }

  return CallWindowProc(default_edit_proc, hwnd, msg, wp, lp);
}

// Adds a search result to the existing set of results. The capacity of the
// results array will double if it gets maxed.
void add_result(int startx, int starty, int endx, int endy)
{
  if (search_results.matches == search_results.capacity) {
    search_results.capacity *= 2;
    search_results.results = renewn(search_results.results, search_results.capacity);
  }

  search_results.results[search_results.matches].start.x = startx;
  search_results.results[search_results.matches].start.y = starty;
  search_results.results[search_results.matches].end.x = endx;
  search_results.results[search_results.matches].end.y = endy;
  search_results.matches++;
}

// A range binary search, that is, anything between the start of a search
// result and the end of a search result should be a match.
int search_compare(const void * pkey, const void * pelem)
{
  pos * scrpos = (pos *)pkey;
  single_result * elem = (single_result *)pelem;

  // Checks to see if the current position is in the range of this element.
  bool in_range = posle(elem->start, *scrpos) && poslt(*scrpos, elem->end);

  if (!in_range) {
    // If not, we check to see if they are on the same line.
    if (scrpos->y == elem->start.y) {
      // On the same line, is the current position before or after this element?
      return scrpos->x - elem->start.x;
    }

    // They are not on the same line, is the current line above or below?
    return scrpos->y - elem->start.y;
  }

  return 0;
}

// Comparison for the quicksort to sort all the search results.
int sort_compare(const void * a, const void * b)
{
  single_result * first = (single_result *)a;
  single_result * second = (single_result *)b;

  if (first->start.y == second->start.y) {
    return first->start.x - second->start.x;
  }

  return first->start.y - second->start.y;
}

// Returns true if the position is contained within a range of a result.
bool contained_in_results(pos position)
{
  single_result * result = (single_result *)
    bsearch(&position, search_results.results, search_results.matches, sizeof(single_result), search_compare);

  return result != NULL;
}

bool test_results(int x, int y)
{
  pos position;
  position.x = x;
  position.y = y;

  single_result * result = (single_result *)
    bsearch(&position, search_results.results, search_results.matches, sizeof(single_result), search_compare);

  return result != NULL;
}

// The search logic. What this does is combine the current line and a little bit
// of the previous line to search for results that are straddling. Otherwise, it
// would do searches line by line.
void search_scrollback(void)
{
  char query[80];
  query[0] = 0;

  // Only get the text from the control if it is currently being shown.
  if (search_control_showing)
    GetDlgItemText (wnd, IDC_EDIT, query, 512);

  // The max straddle length of this search result.
  int straddle_length = strlen(query) - 1;

  search_results.matches = 0;
  if (straddle_length < 0) {
    // Search is empty, update and leave
    win_update();
    return;
  }

  int current = term.curs.y;
  int previous = current - 1;
  termline *line = 0;
  termline *prev_line = 0;
  char *chars = 0;

  int sbtop = -sblines();
  
  while (current >= sbtop) {
    line = fetch_line(current);

    if (previous >= sbtop) {
      prev_line = fetch_line(previous);
    }

    int size = line->size + straddle_length;
    chars = newn(char, size + 1);

    // Create a string that contains the last few charactesr of the previous line
    // and put it at the beginning of the search query.
    int offset = 0;
    if (prev_line) {
      int i = prev_line->size;
      int j = 0;
      for (; i >= 0 && j <= straddle_length; i--, j++) {
        chars[straddle_length - j] = (char)(prev_line->chars + i)->chr;
      }
      offset = straddle_length;
    }

    // Append the current string.
    for (int i = 0; i < line->size; i++) {
      chars[i + offset] = (char)(line->chars + i)->chr;
      chars[i + offset + 1] = 0;
    }

    // Start the search for the query in this string
    char * p = chars;
    while ((p = strstr(p, query)) != NULL) {
      int row = current;
      int col = p - chars;
      int end_col = col + strlen(query);
      if (prev_line) {
        end_col -= straddle_length;
        if (col < straddle_length) {
          // If the results are within the first straddled characters, it
          // means the result belongs to the previous line.
          row = previous;
          col = prev_line->size - straddle_length + col;
        }
        else
        {
          col -= straddle_length;
        }
      }

      // The ending row is always the current one, we ensured it by selecting an
      // overlapping length of query_length - 1.
      add_result(col, row, end_col, current);
      p++;
    }

    free(chars);
    release_line(line);
    if (prev_line) {
      release_line(prev_line);
      prev_line = 0;
    }

    current--;
    previous--;
  }

  // Sort the results!
  qsort(search_results.results, search_results.matches, sizeof(single_result), sort_compare);
  win_update();
}

