#include <stdio.h>
#include <stdlib.h>

#include "libpagemap.h"

int main(void) {
    pagemap_tbl * table = NULL;
    pagemap_t * tmp = NULL;
    unsigned long free,shared,nonshared;

    if (getuid() != 0) {
        fprintf(stderr, "Please use root in this case");
        return 1;
    }

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
        printf("pid:%d uss:%lu pss:%lu swap:%lu shr:%lu res:%lu n_drt:%lu n_uptd:%lu n_wbck:%lu n_err:%lu\n",
                tmp->pid, 
                tmp->uss*4,
                tmp->pss*4,
                tmp->swap*4,
                tmp->shr*4,
                tmp->res*4,
                tmp->n_drt*4,
                tmp->n_uptd*4,
                tmp->n_wback*4,
                tmp->n_err*4
                );
    }
    printf("%d\n", get_physical_pgmap(table,&shared,&free,&nonshared));
    printf("Memory stats in kB - shared: %lu free: %lu nonshared:%lu\n", shared*4, free*4, nonshared*4);
    close_pgmap_table(table);

    return 0;
}
