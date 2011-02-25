
#include <stdio.h>
#include "uca.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("usage: uca <property-name>\n");
        return 1;
    }
    else {
        int property = uca_get_property_id(argv[1]);
        if (property == UCA_PROP_INVALID) {
            printf("Property invalid!\n");
            return 1;
        }

        struct uca_t *uca = uca_init();
        if (uca == NULL) {
            printf("Couldn't find a camera\n");
            return 1;
        }

        uint32_t value; /* this type should be right, most of the time */
        if (uca->cam_get_property(uca, property, &value) == UCA_PROP_INVALID)
            printf("Property not supported on this camera\n");
        else
            printf("%s = %u\n", argv[1], value);

        uca_destroy(uca);
    }
    return 0;
}
