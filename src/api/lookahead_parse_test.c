#include "lookahead_parsing.h"


void printLookaheadInfo(struct lookahead_info * linfo) {
    printf ("mapped_memblock_length: %d\n", linfo->mapped_memblock_length);
    printf ("start_offset: %ld\n", linfo->lmap.start_offset);
    printf ("finish_offset: %ld\n", linfo->lmap.finish_offset);
    printf ("number of values: %ld\n", linfo->lmap.number_of_values);

    for (int i = 0; i < linfo->lmap.number_of_values; i++) {
        printf ("Lookahead[%d] = %ld\n", i, linfo->lmap.lookahead_values[i]);
    }

}
int main() {
    struct lookahead_info bbl_linfo, loop_linfo;
    if (LoadLookahead("/home/vignesh/Titan/scripts/.ttn/lookahead/bbl_lookahead.info",
        &bbl_linfo)) {
        printf ("BBL Lookahead loaded successfully \n");
        printLookaheadInfo(&bbl_linfo);
    }

    if (LoadLookahead("/home/vignesh/Titan/scripts/.ttn/lookahead/loop_lookahead.info",
        &loop_linfo)) {
        printf ("Loop Lookahead loaded successfully \n");
        printLookaheadInfo(&loop_linfo);
    }
    return 0;
}