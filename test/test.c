
#include <stdio.h>
#include "uca.h"

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

    char string_value[256];
    uint32_t uint32_value;
    uint8_t uint8_value;

    const char *unit_map[] = {
        "px",
        "bits",
        "ns",
        "µs",
        "ms",
        "s",
        "rows",
        "" 
    };

    for (int i = 0; i < UCA_PROP_LAST-2; i++) {
        struct uca_property_t *prop = uca_get_full_property(i);
        switch (prop->type) {
            case uca_string:
                if (uca->cam_get_property(uca, i, string_value) != UCA_PROP_INVALID) {
                    print_level(count_dots(prop->name));
                    printf("%s = %s %s ", prop->name, string_value, unit_map[prop->unit]);
                }
                break;
            case uca_uint32t:
                if (uca->cam_get_property(uca, i, &uint32_value) != UCA_PROP_INVALID) {
                    print_level(count_dots(prop->name));
                    printf("%s = %i %s ", prop->name, uint32_value, unit_map[prop->unit]);
                }
                break;
            case uca_uint8t:
                if (uca->cam_get_property(uca, i, &uint8_value) != UCA_PROP_INVALID) {
                    print_level(count_dots(prop->name));
                    printf("%s = %i %s ", prop->name, uint8_value, unit_map[prop->unit]);
                }
                break;
        }
        printf("\n");
    }
    
    uca_destroy(uca);
    return 0;
}
