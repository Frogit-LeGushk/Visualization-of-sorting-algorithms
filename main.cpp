/// #define ASSEMBLY_LIB
/** C/C++ lib**/
#include <curses.h>
#include "sorts.h"


/** C++ lib **/
#include <iostream>
#include <fstream>
#include <math.h>


/** C lib **/
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <limits.h>


/** Linux specific include **/
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <fcntl.h>


using namespace std;


/** const macroses **/
#define MAX_SATE_ARR 256        // max count of column in text-mode window
#define CNT_COLORS 8            // all exist in ncurses
#define MAX_NSEC 999999999      // ~ 1 sec
#define STACK_SIZE 8192         // 8kB
#define CNT_SORTS 5             // count of implemented sorts


/** variable macroses **/
#define MAX_SPEED 5.0           // in fact - max time delay
#define MIN_SPEED 0.0           // in fact - min time delay
#define DELTA_SPEED 0.025       // change delta speed by one scroll
#define DEFAULT_FASTER_MUL 60   // how many times faster delay < slow delay
#define DEFAULT_SPEED (MAX_SPEED+MIN_SPEED)/2.0
                                // in default 50% speed of sorting


/****************************
 *  global definition/      *
 *  implementation          *
 *      ;                   *
 *  ATTENTION!: add static  *
 *  functions have          *
 *  prefix "__"             *
 *  without prefix function *
 *  for lib assembly        *
 ****************************/


extern "C" {

/** main state of ncurses **/
/** is critical section **/
struct State {
    int arr_var[MAX_SATE_ARR];
    int arr_clr[MAX_SATE_ARR];
    int bg_clr;
    int iline_clr;
    int arr_clr_pare[CNT_COLORS];
    int row, col;
    struct timespec fast = {0, MAX_NSEC/DEFAULT_FASTER_MUL};
    struct timespec slow = {0, MAX_NSEC};
    double speed = DEFAULT_SPEED;
    bool isInf = false;
    char const *nameSort;
    char const *complexity;
    int fasterMul = DEFAULT_FASTER_MUL;
    bool isDone = false;
};


typedef void (*callback) (int *arr, int n);

/** spin-lock for critical section **/
static pthread_spinlock_t lock;

enum { delay_duration = 1000 };
enum { key_escape = 27 };
enum { fast_delay, slow_delay };

/** all exist colors in ncurses in ascending order **/
static int const arr_all_clr[CNT_COLORS] = {
    COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
    COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
};

/** meta info about sorts **/
static char const *arr_sortnames[CNT_SORTS] = {
    "choicemethodsort", "insertsort", "quicksort",
    "mergesort", "heapsort"
};
static char const *arr_sortcomplexity[CNT_SORTS] = {
    "O(n^2)", "O(n^2)", "O(n*log(n))",
    "O(n*log(n))", "O(n*log(n))"
};

/** container of pointers to sort-functions **/
static callback arr_callbacks[CNT_SORTS];

}


/****************************
 *  handler functions       *
 *  for ncurses and         *
 *  draw/change state       *
 ****************************/

static void __add_arr_callback(callback cb)
{
    static int top = 0;
    arr_callbacks[top++] = cb;
}
static int __setpare(int fg, int bg)
{
    int n = 8 * bg + fg + 1;
    init_pair(n, fg, bg);
    return n;
}
static void __draw_col(int col_cp, int bg_cp, int x, int h, int row)
{
    int attr;
    register int i;
    for(i = 1; i < row; i++)
    {
        move(i, x);
        if(i < row-h)
            attr = COLOR_PAIR(bg_cp);
        else
            attr = COLOR_PAIR(col_cp);
        attrset(attr);
        addch(' ');
    }
    refresh();
}
static struct State * __getState()
{
    static struct State state;
    return &state;
}
static void __set_arr_clr(int i, int clr)
{
    struct State *p = __getState();

    assert(!pthread_spin_lock(&lock));
    p->arr_clr[i] = p->arr_clr_pare[clr];
    assert(!pthread_spin_unlock(&lock));
}
static void __add_diff_speed(double diff)
{
    struct State *p = __getState();

    assert(!pthread_spin_lock(&lock));
    p->speed += diff;
    if(p->speed < MIN_SPEED)
        p->speed = MIN_SPEED;
    if(p->speed > MAX_SPEED)
        p->speed = MAX_SPEED;
    assert(!pthread_spin_unlock(&lock));
}
static void __draw_infoline()
{
    struct State *p = __getState();
    int speed_precent;

    move(0,0);
    attrset(COLOR_PAIR(p->iline_clr) | A_BOLD);
    for(register int j = 0; j < p->col; j++)
        addch(' ');

    move(0,0);
    speed_precent = (1 - (p->speed - MIN_SPEED) / (MAX_SPEED)) * 100.0;
    printw("Name: \"%s\" | Speed: %d%% | Complexity: %s | Done: %s", p->nameSort,
           speed_precent, p->complexity, p->isDone ? "Yes (Esc)" : "No (-)");

    refresh();
}
static void __draw_state(int delay)
{
    struct State *p = __getState();

    assert(!pthread_spin_lock(&lock));
    for(int i = 0; i < p->col; i++)
        __draw_col(p->arr_clr[i], p->bg_clr, i, p->arr_var[i], p->row);
    __draw_infoline();

    struct timespec t_fast = {p->fast.tv_sec, p->fast.tv_nsec};
    struct timespec t_slow = {p->slow.tv_sec, p->slow.tv_nsec};

    t_fast.tv_sec *= p->speed;
    t_fast.tv_nsec *= p->speed;
    t_slow.tv_sec *= p->speed;
    t_slow.tv_nsec *= p->speed;
    assert(!pthread_spin_unlock(&lock));

    t_fast.tv_sec += t_fast.tv_nsec / MAX_NSEC;
    t_fast.tv_nsec %= MAX_NSEC;
    t_slow.tv_sec += t_slow.tv_nsec / MAX_NSEC;
    t_slow.tv_nsec %= MAX_NSEC;

    if(delay == fast_delay)
        nanosleep(&t_fast, NULL);
    if(delay == slow_delay)
        nanosleep(&t_slow, NULL);
}


/****************************
 *  handler functions       *
 *  for sorts               *
 ****************************/

extern "C" {

typedef void (*sort_cb_t)(int *,int);

bool check_sort(int *arr, int n)
{
    bool isCorrect = true;

    for(register int i = 0; i < n - 1; i++)
        if(arr[i] > arr[i+1])
            {isCorrect = false; break;}
    return isCorrect;
}
double mesure_time()
{
    static bool first_call = true;
    static clock_t start_cl;
    static clock_t end_cl;
    double seconds;

    if(first_call)
    {
        start_cl = clock();
        seconds = -1;
    }
    else
    {
        end_cl = clock();
        seconds = (end_cl - start_cl) / (double)CLOCKS_PER_SEC;
    }

    first_call = !first_call;
    return seconds;
}
void fill_random(int *arr, int n, int max_num, bool can_neg, int offset = 0)
{
    static bool isFirstInit = true;
    if(isFirstInit)
    {
        srand(time(NULL));
        isFirstInit = false;
    }

    for(register int i = 0; i < n; i++)
        arr[i] = (rand() % max_num) * ((i % 2 && can_neg) ? -1 : 1) + offset;
}
void testing_sort(int *arr, int n, int max_num, bool can_neg,
                         sort_cb_t callback, char *name_sort, char * fname)
{
    assert(arr && n > 0 && max_num > 0 && callback && name_sort && fname);

    fill_random(arr, n, max_num, can_neg);
    mesure_time();
        callback(arr, n);
    double seconds = mesure_time();
    bool res_status = check_sort(arr, n);

    FILE *fp = fopen(fname, "wb");
    struct ResultSort res = {"", n, max_num, seconds, can_neg, res_status};
    strcpy(res.name_sort, name_sort);
    fseek(fp, 0, SEEK_END);
    fwrite((void *)(&res), sizeof(struct ResultSort), 1, fp);
    fclose(fp);
}

}



/****************************
 *  1) choice method sort   *
 *     O(n^2)               *
 ****************************/

/** find min index element's if array in range [start,n] **/
static int __find_min(int *arr, int start, int n)
{
    int min_el = arr[start];
    int min_ind = start;
    register int i;

    for(i = start + 1; i <= n; i++)
    {
        if(arr[i] < min_el)
        {
            min_el = arr[i];
            min_ind = i;
        }

        #if !ASSEMBLY_LIB
        __set_arr_clr(i, COLOR_YELLOW);
            __draw_state(fast_delay);
        __set_arr_clr(i, COLOR_RED);
        #endif // !ASSEMBLY_LIB
    }

    return min_ind;
}

extern "C" {

/** implementation **/
void choicemethodsort(int *arr, int n)
{
    int i;
    int min_ind;

    for(i = 0; i < n - 1; i++)
    {
        #if !ASSEMBLY_LIB
        __set_arr_clr(i, COLOR_GREEN);
            __draw_state(fast_delay);
        #endif // !ASSEMBLY_LIB

        min_ind = __find_min(arr, i, n - 1);

        #if !ASSEMBLY_LIB
        __set_arr_clr(min_ind, COLOR_YELLOW);
            __draw_state(slow_delay);
        __set_arr_clr(min_ind, COLOR_GREEN);
        __set_arr_clr(i, COLOR_YELLOW);
        #endif // !ASSEMBLY_LIB

        swap(arr[i], arr[min_ind]);

        #if !ASSEMBLY_LIB
            __draw_state(slow_delay);
        __set_arr_clr(i, COLOR_RED);
        __set_arr_clr(min_ind, COLOR_RED);
        #endif // !ASSEMBLY_LIB
    }
}

}



/****************************
 *  2) insert sort          *
 *     O(n^2)               *
 ****************************/

extern "C" {

/** implementation **/
void insertsort(int *arr, int n)
{
    int i;
    register int j;
    for(i = 1; i < n; i++)
    {
        j = i;
        while(arr[j - 1] > arr[j] && j > 0)
        {
            swap(arr[j-1], arr[j]);
            j--;

            #if !ASSEMBLY_LIB
            __set_arr_clr(j, COLOR_YELLOW);
            __set_arr_clr(i, COLOR_GREEN);
                __draw_state(fast_delay);
            __set_arr_clr(i, COLOR_RED);
            __set_arr_clr(j, COLOR_RED);
            #endif // !ASSEMBLY_LIB
        }
    }
}

}



/****************************
 *  3) quick sort           *
 *     O(n*log(n))          *
 ****************************/

/** sorting around support element **/
static int __partition(int *arr, int l, int h)
{
    register int i;
    int p = h;
    int firsthigh = l;

    #if !ASSEMBLY_LIB
    __set_arr_clr(p, COLOR_BLUE);
        __draw_state(fast_delay);
    #endif // !ASSEMBLY_LIB

    for(i = l; i < h; i++)
    {
        #if !ASSEMBLY_LIB
        __set_arr_clr(i, COLOR_YELLOW);
        __set_arr_clr(firsthigh, COLOR_GREEN);
            __draw_state(slow_delay);
        __set_arr_clr(firsthigh, COLOR_RED);
        __set_arr_clr(i, COLOR_RED);
        #endif // !ASSEMBLY_LIB

        if(arr[i] < arr[p])
        {
            swap(arr[i], arr[firsthigh]);

            #if !ASSEMBLY_LIB
            __set_arr_clr(i, COLOR_GREEN);
            __set_arr_clr(firsthigh, COLOR_YELLOW);
                __draw_state(slow_delay);
            __set_arr_clr(firsthigh, COLOR_RED);
            __set_arr_clr(i, COLOR_RED);
            #endif // !ASSEMBLY_LIB

            firsthigh++;
        }

    }

    swap(arr[p], arr[firsthigh]);

    #if !ASSEMBLY_LIB
    __set_arr_clr(p, COLOR_GREEN);
    __set_arr_clr(firsthigh, COLOR_BLUE);
        __draw_state(fast_delay);
    __set_arr_clr(p, COLOR_RED);
    __set_arr_clr(firsthigh, COLOR_RED);
    #endif // !ASSEMBLY_LIB

    return firsthigh;
}

/** recursive implementation **/
static void __quicksort(int *arr, int l, int h)
{
    if(h > l)
    {
        int p = __partition(arr, l, h);
        __quicksort(arr, l, p-1);
        __quicksort(arr, p+1, h);
    }
}

extern "C" {

/** implementation wrapper (interface) **/
void quicksort(int *arr, int n)
{
    __quicksort(arr, 0, n-1);
}

}



/****************************
 *  4) merge sort           *
 *     O(n*log(n))          *
 ****************************/

/** support structure **/
struct queue {
    int *arr;
    int top;
    int size;
};

/** merge two part sorted array in one contiguous **/
static void __merge(int *arr, int low, int middle, int high)
{
    int *arr_l = (int *)malloc((middle-low+1)*sizeof(int));
    int *arr_r = (int *)malloc((high-middle)*sizeof(int));
    assert(arr_l && arr_r);

    struct queue qleft = {arr_l, 0, middle-low+1};
    struct queue qright = {arr_r, 0, high-middle};

    register int i;
    for(i = low; i <= middle; i++)
        qleft.arr[qleft.top++] = arr[i];
    for(i = middle + 1; i <= high; i++)
        qright.arr[qright.top++] = arr[i];

    qleft.top = 0;
    qright.top = 0;

    for(i = low; i <= high; i++)
    {
        if(qleft.top < qleft.size && qright.top < qright.size)
        {
            #if !ASSEMBLY_LIB
            __set_arr_clr(i, COLOR_GREEN);
            __set_arr_clr(low + qleft.top, COLOR_YELLOW);
            __set_arr_clr(middle + 1 + qright.top, COLOR_YELLOW);
                __draw_state(slow_delay);
            __set_arr_clr(i, COLOR_RED);
            __set_arr_clr(low + qleft.top, COLOR_RED);
            __set_arr_clr(middle + 1 + qright.top, COLOR_RED);
            #endif // !ASSEMBLY_LIB

            if(qleft.arr[qleft.top] <= qright.arr[qright.top])
                arr[i] = qleft.arr[qleft.top++];
            else
                arr[i] = qright.arr[qright.top++];
        }
        else
        if(qleft.top < qleft.size)
        {
            #if !ASSEMBLY_LIB
            __set_arr_clr(i, COLOR_GREEN);
            __set_arr_clr(low + qleft.top, COLOR_YELLOW);
                __draw_state(slow_delay);
            __set_arr_clr(i, COLOR_RED);
            __set_arr_clr(low + qleft.top, COLOR_RED);
            #endif // !ASSEMBLY_LIB

            arr[i] = qleft.arr[qleft.top++];
        }
        else
        if(qright.top < qright.size)
        {
            #if !ASSEMBLY_LIB
            __set_arr_clr(i, COLOR_GREEN);
            __set_arr_clr(middle + 1 + qright.top, COLOR_YELLOW);
                __draw_state(slow_delay);
            __set_arr_clr(i, COLOR_RED);
            __set_arr_clr(middle + 1 + qright.top, COLOR_RED);
            #endif // !ASSEMBLY_LIB

            arr[i] = qright.arr[qright.top++];
        }

        #if !ASSEMBLY_LIB
        __set_arr_clr(i, COLOR_BLUE);
            __draw_state(slow_delay);
        __set_arr_clr(i, COLOR_RED);
        #endif // !ASSEMBLY_LIB
    }

    free(qleft.arr);
    free(qright.arr);
}

/** recursive implementation **/
static void __mergesort(int *arr, int low, int high)
{
    if(low < high)
    {
        int middle = (high + low) / 2;
        __mergesort(arr, low, middle);
        __mergesort(arr, middle + 1, high);
        __merge(arr, low, middle, high);
    }
}

extern "C" {

/** implementation wrapper (interface) **/
void mergesort(int *arr, int n)
{
    __mergesort(arr, 0, n-1);
}

}



/****************************
 *  5) heap sort            *
 *     O(n*log(n))          *
 ****************************/

/** default structure of heap **/
struct heap {
    int *arr;
    int top;
    int size;
};

/** navigation functions in heap **/
inline int __parent_key(int n)
{
    return ((n == 1) ? (-1) : (n/2));
}
inline int __rchild_key(int n)
{
    return (2*n + 1);
}
inline int __lchild_key(int n)
{
    return (2*n);
}

/** partial ordering functions **/
static void __bubble_up(struct heap *hs, int top)
{
    int pk = __parent_key(top);
    if(pk == -1) return;

    #if !ASSEMBLY_LIB
    __set_arr_clr(top-1, COLOR_GREEN);
    __set_arr_clr(pk-1, COLOR_YELLOW);
        __draw_state(slow_delay);
    __set_arr_clr(pk-1, COLOR_RED);
    __set_arr_clr(top-1, COLOR_RED);
    #endif // !ASSEMBLY_LIB

    if(hs->arr[pk] > hs->arr[top])
    {
        swap(hs->arr[pk], hs->arr[top]);

        #if !ASSEMBLY_LIB
        __set_arr_clr(top-1, COLOR_YELLOW);
        __set_arr_clr(pk-1, COLOR_GREEN);
            __draw_state(slow_delay);
        __set_arr_clr(pk-1, COLOR_RED);
        __set_arr_clr(top-1, COLOR_RED);
        #endif // !ASSEMBLY_LIB

        __bubble_up(hs, pk);
    }
}
static void __bubble_down(struct heap *hs, int top)
{
    int lc_key = __lchild_key(top);
    int rc_key = __rchild_key(top);

    int min_k = top;
    int min = hs->arr[top];

    #if !ASSEMBLY_LIB
    __set_arr_clr(top-1, COLOR_GREEN);
    if(lc_key <= hs->top)
        __set_arr_clr(lc_key-1, COLOR_YELLOW);
    if(rc_key <= hs->top)
        __set_arr_clr(rc_key-1, COLOR_YELLOW);
    #endif // !ASSEMBLY_LIB

    if(lc_key <= hs->top && hs->arr[lc_key] < min)
    {
        min_k = lc_key;
        min = hs->arr[lc_key];
    }
    if(rc_key <= hs->top && hs->arr[rc_key] < min)
    {
        min_k = rc_key;
        min = hs->arr[rc_key];
    }

    #if !ASSEMBLY_LIB
        __draw_state(slow_delay);
    __set_arr_clr(top-1, COLOR_RED);
    if(lc_key <= hs->top)
        __set_arr_clr(lc_key-1, COLOR_RED);
    if(rc_key <= hs->top)
        __set_arr_clr(rc_key-1, COLOR_RED);
    #endif // !ASSEMBLY_LIB

    if(min_k != top)
    {
        swap(hs->arr[top], hs->arr[min_k]);

        #if !ASSEMBLY_LIB
        __set_arr_clr(top-1, COLOR_YELLOW);
        __set_arr_clr(min_k-1, COLOR_GREEN);
            __draw_state(slow_delay);
        __set_arr_clr(min_k-1, COLOR_RED);
        __set_arr_clr(top-1, COLOR_RED);
        #endif // !ASSEMBLY_LIB

        __bubble_down(hs, min_k);
    }
}

/** analogue push/pop operations in heap **/
static void __insert_heap(struct heap *hs, int x)
{
    hs->arr[++(hs->top)] = x;

    #if !ASSEMBLY_LIB
    __set_arr_clr(hs->top-1, COLOR_GREEN);
        __draw_state(slow_delay);
    __set_arr_clr(hs->top-1, COLOR_RED);
    #endif // !ASSEMBLY_LIB

    __bubble_up(hs, hs->top);
}
static int __min_heap(struct heap *hs)
{
    #if !ASSEMBLY_LIB
    __set_arr_clr(0, COLOR_BLUE);
    __set_arr_clr(hs->top-1, COLOR_GREEN);
        __draw_state(slow_delay);
    __set_arr_clr(hs->top-1, COLOR_RED);
    __set_arr_clr(0, COLOR_RED);
    #endif // !ASSEMBLY_LIB

    int min = hs->arr[1];

    hs->arr[1] = hs->arr[hs->top];
    hs->arr[hs->top] = 0;
    hs->top -= 1;

    __bubble_down(hs, 1);
    return min;
}

extern "C" {

/** implementation **/
void heapsort(int *arr, int n)
{
    register int i;

    int *heap_arr = (int *)malloc((n+1) * sizeof(int));
    assert(heap_arr);

    memcpy((void *)heap_arr, (void *)arr, n * sizeof(int));
    memset((void *)arr, 0, n * sizeof(int));

    struct heap sh = {arr-1, 0, (n+1)};

    for(i = 0; i < n; i++)
        __insert_heap(&sh, heap_arr[i]);

    for(i = 0; i < n; i++)
        heap_arr[i] = __min_heap(&sh);

    memcpy((void *)arr, (void *)heap_arr, n * sizeof(int));
        __draw_state(fast_delay);
}

}



/****************************
 *  multi threads           *
 *  handlers                *
 ****************************/

/** main function called in new thread **/
static void *__thread_callback(void * arg)
{
    struct State *p = __getState();
    ((callback)arg)(p->arr_var, p->col);
    p->isDone = true;
    __draw_state(fast_delay);
    return NULL;
}


#if !ASSEMBLY_LIB
int main(int argc, char **argv)
{
    int i;                          // iterator
    int key;                        // pressed key number
    struct State *p = __getState(); // critical section
    pthread_t thread;               // thread id

    /** initialize spin-lock for critical section **/
    assert(!pthread_spin_init(&lock, PTHREAD_PROCESS_SHARED));

    /** ncurses setup **/
    initscr();
    getmaxyx(stdscr, p->row, p->col);
    cbreak();
    noecho();
    keypad(stdscr, 1);
    curs_set(0);
    start_color();
    timeout(delay_duration);

    /** initialize color pares **/
    for(i = 0; i < CNT_COLORS; i++)
        p->arr_clr_pare[i] = __setpare(arr_all_clr[i], arr_all_clr[i]);

    /** initialize default colors state **/
    for(i = 0; i < p->col; i++)
        p->arr_clr[i] = p->arr_clr_pare[COLOR_RED];
    p->bg_clr = arr_all_clr[COLOR_BLACK];
    p->iline_clr = __setpare(COLOR_WHITE, COLOR_BLACK);

    /** initialize sorts-functions as callback argument in thread **/
    __add_arr_callback(choicemethodsort);
    __add_arr_callback(insertsort);
    __add_arr_callback(quicksort);
    __add_arr_callback(mergesort);
    __add_arr_callback(heapsort);

    /** main loop **/
    for(i = 0; i < CNT_SORTS; i++)
    {
        fill_random(p->arr_var, p->col, p->row - 2, false, 1);
        p->isDone = false;
        p->nameSort = arr_sortnames[i];
        p->complexity = arr_sortcomplexity[i];

        pthread_create(&thread, NULL, __thread_callback, (void *)arr_callbacks[i]);
        sleep(1);

        while((key = getch()) != key_escape)
        {
            switch(key)
            {
            case KEY_UP:
                __add_diff_speed(-DELTA_SPEED);
                break;
            case KEY_DOWN:
                __add_diff_speed(DELTA_SPEED);
                break;
            }
        }

        pthread_join(thread, NULL);
    }

    /** reset state of terminal **/
    endwin();
    return 0;
}
#endif // !ASSEMBLY_LIB
