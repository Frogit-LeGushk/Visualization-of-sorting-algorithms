#include "sorts.h"
#include <iostream>

using namespace std;

int main()
{
    int arr[32];
    fill_random(arr, 32, 10, 0, 0);

    for(int i = 0; i < 32; i++)
        cout << arr[i] << " ";
    cout << endl;

    return 0;
}
