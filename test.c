#include <stdio.h>
#include <stdlib.h>

#include "libpagemap.h"

int main(void) {
    pagemap_tbl * table = NULL;
    pagemap_t * tmp = NULL;

    if ((table = init_pgmap_table(table)) == NULL) {
        printf("INIT ERROR");
        return 1;
    }
    if (!open_pgmap_table(table,0x0)) {
        printf("OPEN ERROR");
        return 1;
    }
    printf("All pids:\n");
    reset_table_pos(table);
    while((tmp = iterate_over_all(table))) {
        printf("%d\n", tmp->pid);
    }
    close_pgmap_table(table);

    return 0;
}
