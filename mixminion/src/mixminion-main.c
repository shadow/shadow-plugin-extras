/*
 * See LICENSE for licensing information
 */

#include "mixminion.h"

int main(int argc, char *argv[]) {
    Mixminion *m = mixminion_new(argc, argv, NULL);
    mixminion_free(m);
}