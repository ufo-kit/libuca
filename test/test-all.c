
#include <glib.h>
#include "uca-camera.h"
#include "uca-mock-camera.h"

typedef struct {
    UcaCamera *camera;
} Fixture;

typedef void (*UcaFixtureFunc) (Fixture *fixture, gconstpointer data);

static void fixture_setup(Fixture *fixture, gconstpointer data)
{
    const gchar *type = (gchar *) data;
    GError *error = NULL;
    fixture->camera = uca_camera_new(type, &error);
    g_assert(error == NULL);
    g_assert(fixture->camera);
}

static void fixture_teardown(Fixture *fixture, gconstpointer data)
{
    g_object_unref(fixture->camera);
}

static void on_property_change(gpointer instance, GParamSpec *pspec, gpointer user_data)
{
    gboolean *success = (gboolean *) user_data;
    *success = TRUE;
}

static void test_factory()
{
    GError *error = NULL;
    UcaCamera *camera = uca_camera_new("fox994m3a0yxmy", &error);
    g_assert_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_FOUND);
    g_assert(camera == NULL);
}

static void test_recording(Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    UcaCamera *camera = UCA_CAMERA(fixture->camera);

    uca_camera_stop_recording(camera, &error);
    g_assert_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING);
    g_error_free(error);

    error = NULL;
    uca_camera_start_recording(camera, &error);
    g_assert_no_error(error);
    uca_camera_stop_recording(camera, &error);
    g_assert_no_error(error);
}

static void test_recording_signal(Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA(fixture->camera);
    gboolean success = FALSE;
    g_signal_connect(G_OBJECT(camera), "notify::is-recording", 
            (GCallback) on_property_change, &success);

    uca_camera_start_recording(camera, NULL);
    g_assert(success == TRUE);

    success = FALSE;
    uca_camera_stop_recording(camera, NULL);
    g_assert(success == TRUE);
}

static void grab_func(gpointer data, gpointer user_data)
{
    gboolean *success = (gboolean *) user_data;
    *success = TRUE;
}

static void test_recording_async(Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA(fixture->camera);

    gboolean success = FALSE;
    uca_camera_set_grab_func(camera, grab_func, &success);

    g_object_set(G_OBJECT(camera),
            "framerate", 10,
            "transfer-asynchronously", TRUE,
            NULL);

    GError *error = NULL;
    uca_camera_start_recording(camera, &error);
    g_assert_no_error(error);

    g_usleep(G_USEC_PER_SEC / 8);

    uca_camera_stop_recording(camera, &error);
    g_assert_no_error(error);
}

static void test_recording_property(Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA(fixture->camera);

    gboolean is_recording = FALSE;
    uca_camera_start_recording(camera, NULL);
    g_object_get(G_OBJECT(camera),
            "is-recording", &is_recording,
            NULL);
    g_assert(is_recording == TRUE);

    uca_camera_stop_recording(camera, NULL);
    g_object_get(G_OBJECT(camera),
            "is-recording", &is_recording,
            NULL);
    g_assert(is_recording == FALSE);
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


int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_test_add_func("/factory", test_factory);

    gchar **types = uca_camera_get_types();

    /*
     * paths and test_funcs MUST correspond!
     */
    static const gchar *paths[] = {
        "/recording",
        "/recording/signal",
        "/recording/asynchronous",
        "/properties/base",
        "/properties/recording",
        "/properties/binnings",
        NULL
    };

    static UcaFixtureFunc test_funcs[] = {
        test_recording,
        test_recording_signal,
        test_recording_async,
        test_base_properties,
        test_recording_property,
        test_binnings_properties,
    };

    for (guint i = 0; i < g_strv_length(types); i++) {
        guint j = 0;

        while (paths[j] != NULL) {
            gchar *new_path = g_strdup_printf("/%s%s", types[i], paths[j]);
            g_test_add(new_path, Fixture, types[i], fixture_setup, test_funcs[j], fixture_teardown);
            g_free(new_path);
            j++;
        }
    }

    gint result = g_test_run();
    g_strfreev(types);
    return result;
}
