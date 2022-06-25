#include <iostream>
#include <stdlib.h>
#include "sorts.h"


using namespace std;


int main()
{
    const int size_arr = 307'200'000;
    const int max_num = 1'000'000;
    int diff_n = 15'360'000;

    int *arr = (int *)malloc(sizeof(int)*size_arr);

    for(int i = 1; i < size_arr; i++)
    {
        /** insert: insertsort, choicemethodsort, heapsort, mergesort, quicksort
            NULL - output in stdout, otherwize in <char *> file **/
        testing_sort(arr, i, max_num, false, quicksort, "quicksort", NULL);
        i += diff_n - 1;
    }

    return 0;
}
