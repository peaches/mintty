
#include "termsearch.h"
#include "termpriv.h"

#include <commctrl.h>

struct search_results search_results;
HWND search_wnd;

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

int search_compare(const void * pkey, const void * pelem)
{
  pos * scrpos = (pos *)pkey;
  single_result * elem = (single_result *)pelem;

  bool in_range = posle(elem->start, *scrpos) && poslt(*scrpos, elem->end);

  if (!in_range) {
      if (scrpos->y == elem->start.y) {
        return scrpos->x - elem->start.x;
      }

    return scrpos->y - elem->start.y;
  }

  return 0;
}

int sort_compare(const void * a, const void * b)
{
  single_result * first = (single_result *)a;
  single_result * second = (single_result *)b;

  if (first->start.y == second->start.y) {
    return first->start.x - second->start.x;
  }

  return first->start.y - second->start.y;
}

bool contained_in_results(pos position)
{
  qsort(search_results.results, search_results.matches, sizeof(single_result), sort_compare);
  single_result * result = (single_result *)
    bsearch(&position, search_results.results, search_results.matches, sizeof(single_result), search_compare);

  return result != NULL;
}

void init_search_results()
{
  HINSTANCE inst = GetModuleHandle(NULL);

  search_wnd = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                              600, 10, 100, 100,
                              wnd, (HMENU)IDC_EDIT, inst, NULL);
  SetWindowPos(search_wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

  search_results.matches = 0;
  search_results.capacity = 2;  /* Give it a capacity of 2, no reason */
  search_results.results = newn(single_result, search_results.capacity);
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

void print_results()
{
  printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
  qsort(search_results.results, search_results.matches, sizeof(single_result), sort_compare);
  for (int i = 0; i < search_results.matches; i++) {
    printf("(%d, %d) -> (%d, %d)\n", 
      search_results.results[i].start.x, search_results.results[i].start.y,
      search_results.results[i].end.x, search_results.results[i].end.y);
  }

  /*
  printf("%d\n", test_results(8, 1));
  printf("%d\n", test_results(6, 3));
  printf("%d\n", test_results(6, 5));
  printf("%d\n", test_results(6, 7));
  printf("%d\n", test_results(6, 9));
  printf("%d\n", test_results(6, 11));
  printf("%d\n", test_results(6, 13));
  printf("%d\n", test_results(125, 2));
  printf("%d\n", test_results(126, 2));
  printf("%d\n", test_results(0, 3));
  printf("%d\n", test_results(2, 3));
  printf("%d\n\n", test_results(3, 3));

  printf("%d\n", test_results(8, 1));
  printf("%d\n", test_results(11, 1));
  printf("%d\n\n", test_results(17, 1));
  */

  fflush(stdout);
}

void search_scrollback(void)
{
  printf("searching results\n");
  fflush(stdout);
  char query[512];
  GetDlgItemText (wnd, IDC_EDIT, query, 512); 

  int straddle_length = strlen(query) - 1;
  if (straddle_length < 0) {
    return;
  }
  search_results.matches = 0;

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

    int offset = 0;
    if (prev_line) {
      int i = prev_line->size;
      int j = 0;
      for (; i >= 0 && j <= straddle_length; i--, j++) {
        chars[straddle_length - j] = (char)(prev_line->chars + i)->chr;
      }
      offset = straddle_length;
    }

    for (int i = 0; i < line->size; i++) {
      chars[i + offset] = (char)(line->chars + i)->chr;
      chars[i + offset + 1] = 0;
    }

    char * p = chars;
    while ((p = strstr(p, query)) != NULL) {
      int row = current;
      int col = p - chars;
      int end_col = col + strlen(query);
      if (prev_line) {
        end_col -= straddle_length;
        if (col < straddle_length) {
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
  print_results();
}

