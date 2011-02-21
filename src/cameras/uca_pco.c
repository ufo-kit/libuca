
#include <clser.h>
#include <libpco/libpco.h>
#include "uca.h"
#include "uca_pco.h"

struct pco_edge_t *pco;

static void uca_pco_destroy(struct uca_t *uca)
{
    pco_destroy(pco);
}

int uca_pco_init(struct uca_t *uca)
{
    pco = pco_init();
    if (!pco_active(pco)) {
        pco_destroy(pco);
        return 0;
    }

    pco_scan_and_set_baud_rate(pco);

    /* Camera found, set function pointers... */
    uca->cam_destroy = &uca_pco_destroy;

    /* ... and some properties */
    pco_get_actual_size(pco, &uca->image_width, &uca->image_height);
    return 1;
}
