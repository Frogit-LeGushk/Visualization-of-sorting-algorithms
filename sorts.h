#ifndef SORTS_H_INCLUDED
#define SORTS_H_INCLUDED

#include <stdbool.h>

struct ResultSort {
    char name_sort[32];
    int cnt_num;
    int max_num;
    double sec_sort;
    bool can_neg;
    bool res_sort;
};

extern "C" {
    typedef void (*sort_cb_t)(int *,int);
    extern bool check_sort(int *arr, int n);
    extern double mesure_time();
    extern void fill_random(int *arr, int n, int max_num, bool can_neg, int offset);
    extern void testing_sort(int *arr, int n, int max_num, bool can_neg,
                            sort_cb_t callback, char const * name_sort, char const * fname);

    extern void choicemethodsort(int *arr, int n);
    extern void insertsort(int *arr, int n);
    extern void quicksort(int *arr, int n);
    extern void mergesort(int *arr, int n);
    extern void heapsort(int *arr, int n);
}



#endif // SORTS_H_INCLUDED
