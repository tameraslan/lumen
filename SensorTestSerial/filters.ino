// median filtering?
  /*
 * Comparison function for qsort(): return an integer "less than, equal
 * to, or greater than zero if the first argument is considered to be
 * respectively less than, equal to, or greater than the second."
 */
int compare(const void *pa, const void *pb) {
    int a = *(int *)pa;
    int b = *(int *)pb;
    return a<b ? -1 : a>b ? +1 : 0;
}

#define FILTER_LENGTH 11  // length of the median filter, should be odd

/*
 * Input: raw sample.
 * Ouput: filtered sample.
 */
int median_filter(int data) {
    static int input_buffer[FILTER_LENGTH];
    static int input_index;
    int sorted_buffer[FILTER_LENGTH];

    // Store the current data point.
    input_buffer[input_index] = data;
    if (++input_index >= FILTER_LENGTH) input_index = 0;

    // Sort a copy of the input buffer.
    for (int i = 0; i < FILTER_LENGTH; i++)
        sorted_buffer[i] = input_buffer[i];
    qsort(sorted_buffer, FILTER_LENGTH, sizeof *sorted_buffer, compare);

    // Return median.
    return sorted_buffer[FILTER_LENGTH/2];
}
