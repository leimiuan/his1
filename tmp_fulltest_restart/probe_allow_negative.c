#include <stdio.h>
#include "utils.h"
int main(void){ double v=safe_get_double_allow_negative(); printf("%.2f\n", v); return 0; }
