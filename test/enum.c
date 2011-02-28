
#include <stdio.h>
#include "uca.h"
#include "uca-cam.h"

int count_dots(const char *s)
{
    int res = 0;
    while (*(s++) != '\0')
        if (*s == '.')
            res++;
    return res;
}

void print_level(int depth)
{
    for (int i = 0; i < depth; i++)
        printf("|  ");
    printf("`-- ");
}

int main(int argc, char *argv[])
{
    struct uca_t *uca = uca_init();
    if (uca == NULL) {
        printf("Couldn't find a camera\n");
        return 1;
    }

    /* take first camera */
    struct uca_camera_t *cam = uca->cameras;

    char string_value[256];
    uint32_t uint32_value;
    uint8_t uint8_value;

    const char *unit_map[] = {
        "px", "bits", "ns", "µs", "ms", "s", "rows", "" 
    };

    while (cam != NULL) {
        for (int i = 0; i < UCA_PROP_LAST; i++) {
            struct uca_property_t *prop = uca_get_full_property(i);
            print_level(count_dots(prop->name));
            printf("%s = ", prop->name);
            switch (prop->type) {
                case uca_string:
                    if (cam->get_property(cam, i, string_value) != UCA_ERR_PROP_INVALID) {
                        printf("%s ", string_value);
                    }
                    else
                        printf("n/a");
                    break;
                case uca_uint32t:
                    if (cam->get_property(cam, i, &uint32_value) != UCA_ERR_PROP_INVALID) {
                        printf("%i %s", uint32_value, unit_map[prop->unit]);
                    }
                    else
                        printf("n/a");
                    break;
                case uca_uint8t:
                    if (cam->get_property(cam, i, &uint8_value) != UCA_ERR_PROP_INVALID) {
                        printf("%i %s", uint8_value, unit_map[prop->unit]);
                    }
                    else
                        printf("n/a");
                    break;
            }
            printf("\n");
        }
        cam = cam->next;
    }
    
    uca_destroy(uca);
    return 0;
}
