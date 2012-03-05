
#include <glib.h>
#include "uca-camera.h"
#include "uca-mock-camera.h"

typedef struct {
    UcaMockCamera *camera;
} Fixture;

static void fixture_setup(Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    fixture->camera = uca_mock_camera_new(&error);
    g_assert(error == NULL);
    g_assert(fixture->camera);
}

static void fixture_teardown(Fixture *fixture, gconstpointer data)
{
    g_object_unref(fixture->camera);
}

static void test_recording(Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    UcaCamera *camera = UCA_CAMERA(fixture->camera);

    uca_camera_stop_recording(camera, &error);
    g_assert(error != NULL);

    error = NULL;
    uca_camera_start_recording(camera, &error);
    g_assert(error == NULL);
    uca_camera_stop_recording(camera, &error);
    g_assert(error == NULL);
}

static void test_base_properties(Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA(fixture->camera);
    guint n_properties = 0;
    GParamSpec **properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(camera), &n_properties);
    GValue val = {0};

    for (guint i = 0; i < n_properties; i++) {
        g_value_init(&val, properties[i]->value_type);
        g_object_get_property(G_OBJECT(camera), properties[i]->name, &val);
        g_value_unset(&val);
    }

    g_free(properties);
}

static void test_binnings_properties(Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA(fixture->camera);

    GValueArray *array = NULL;
    g_object_get(G_OBJECT(camera),
            "sensor-horizontal-binnings", &array,
            NULL);

    GValue *value = g_value_array_get_nth(array, 0);
    g_assert(value != NULL);
    g_assert(g_value_get_uint(value) == 1);
}

static void signal_handler(gpointer instance, const gchar *name, gpointer user_data)
{
    g_print("name: %s\n", name);
    gboolean *success = (gboolean *) user_data;
    *success = TRUE;
}

static void test_signal(Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA(fixture->camera);
    gboolean success = FALSE;
    g_signal_connect(camera, "property-changed", (GCallback) signal_handler, &success);
    g_object_set(G_OBJECT(camera),
            "framerate", 30,
            NULL);
    g_assert(success == TRUE);
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_test_add("/recording", Fixture, NULL, fixture_setup, test_recording, fixture_teardown);
    g_test_add("/properties/base", Fixture, NULL, fixture_setup, test_base_properties, fixture_teardown);
    g_test_add("/properties/binnings", Fixture, NULL, fixture_setup, test_binnings_properties, fixture_teardown);
    g_test_add("/signal", Fixture, NULL, fixture_setup, test_signal, fixture_teardown);

    return g_test_run();
}
