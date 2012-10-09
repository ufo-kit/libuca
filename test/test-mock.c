
#include <glib.h>
#include "uca-camera.h"
#include "uca-plugin-manager.h"

typedef struct {
    UcaPluginManager *manager;
    UcaCamera *camera;
} Fixture;

static void
fixture_setup (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    fixture->manager = uca_plugin_manager_new ();
    uca_plugin_manager_add_path (fixture->manager, "./src");

    fixture->camera = uca_plugin_manager_get_camera (fixture->manager, "mock", &error);
    g_assert (error == NULL);
    g_assert (fixture->camera);
}

static void
fixture_teardown (Fixture *fixture, gconstpointer data)
{
    g_object_unref (fixture->camera);
    g_object_unref (fixture->manager);
}

static void
on_property_change (gpointer instance, GParamSpec *pspec, gpointer user_data)
{
    gboolean *success = (gboolean *) user_data;
    *success = TRUE;
}

static void
test_factory (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    UcaCamera *camera = uca_plugin_manager_get_camera (fixture->manager, "fox994m3a0yxmy", &error);
    g_assert_error (error, UCA_PLUGIN_MANAGER_ERROR, UCA_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND);
    g_assert (camera == NULL);
}

static void
test_recording (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    UcaCamera *camera = UCA_CAMERA (fixture->camera);

    uca_camera_stop_recording (camera, &error);
    g_assert_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING);
    g_error_free (error);

    error = NULL;
    uca_camera_start_recording (camera, &error);
    g_assert_no_error (error);
    uca_camera_stop_recording (camera, &error);
    g_assert_no_error (error);
}

static void
test_recording_signal (Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA (fixture->camera);
    gboolean success = FALSE;
    g_signal_connect (G_OBJECT (camera), "notify::is-recording",
            (GCallback) on_property_change, &success);

    uca_camera_start_recording (camera, NULL);
    g_assert (success == TRUE);

    success = FALSE;
    uca_camera_stop_recording (camera, NULL);
    g_assert (success == TRUE);
}

static void
grab_func (gpointer data, gpointer user_data)
{
    guint *count = (guint *) user_data;
    *count += 1;
}

static void
test_recording_async (Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA (fixture->camera);

    guint count = 0;
    uca_camera_set_grab_func (camera, grab_func, &count);

    g_object_set (G_OBJECT (camera),
            "frame-rate", 10.0,
            "transfer-asynchronously", TRUE,
            NULL);

    GError *error = NULL;
    uca_camera_start_recording (camera, &error);
    g_assert_no_error (error);

    /*
     * We sleep for an 1/8 of a second at 10 frames per second, thus we should
     * record 2 frames.
     */
    g_usleep (G_USEC_PER_SEC / 8);

    uca_camera_stop_recording (camera, &error);
    g_assert_no_error (error);
    g_assert_cmpint (count, ==, 2);
}

static void
test_recording_property (Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA (fixture->camera);

    gboolean is_recording = FALSE;
    uca_camera_start_recording (camera, NULL);
    g_object_get (G_OBJECT (camera),
            "is-recording", &is_recording,
            NULL);
    g_assert (is_recording == TRUE);

    uca_camera_stop_recording (camera, NULL);
    g_object_get (G_OBJECT (camera),
            "is-recording", &is_recording,
            NULL);
    g_assert (is_recording == FALSE);
}

static void
test_base_properties (Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA (fixture->camera);
    guint n_properties = 0;
    GParamSpec **properties = g_object_class_list_properties (G_OBJECT_GET_CLASS (camera), &n_properties);
    GValue val = {0};

    for (guint i = 0; i < n_properties; i++) {
        g_value_init (&val, properties[i]->value_type);
        g_object_get_property (G_OBJECT (camera), properties[i]->name, &val);
        g_value_unset (&val);
    }

    g_free (properties);
}

static void
test_fps_property (Fixture *fixture, gconstpointer data)
{
    gdouble frames_per_second;
    gdouble exposure_time = 0.5;

    g_object_set (G_OBJECT (fixture->camera),
                  "exposure-time", exposure_time,
                  NULL);
    g_object_get (G_OBJECT (fixture->camera),
                  "frames-per-second", &frames_per_second,
                  NULL);

    /*
     * The mock camera does not override the "frames-per-second" property, so we
     * check the implementation from the base camera.
     */
    g_assert_cmpfloat (frames_per_second, ==, 1.0 / exposure_time);
}

static void
test_binnings_properties (Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA (fixture->camera);

    GValueArray *array = NULL;
    g_object_get (G_OBJECT (camera),
            "sensor-horizontal-binnings", &array,
            NULL);

    GValue *value = g_value_array_get_nth (array, 0);
    g_assert (value != NULL);
    g_assert (g_value_get_uint (value) == 1);
}

static void
test_signal (Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA (fixture->camera);
    gboolean success = FALSE;
    g_signal_connect (camera, "notify::frame-rate", (GCallback) on_property_change, &success);
    g_object_set (G_OBJECT (camera),
            "frame-rate", 30.0,
            NULL);
    g_assert (success == TRUE);
}

int main (int argc, char *argv[])
{
    g_type_init ();

    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://ufo.kit.edu/ufo/ticket");

    g_test_add ("/factory", Fixture, NULL, fixture_setup, test_factory, fixture_teardown);
    g_test_add ("/recording", Fixture, NULL, fixture_setup, test_recording, fixture_teardown);
    g_test_add ("/recording/signal", Fixture, NULL, fixture_setup, test_recording_signal, fixture_teardown);
    g_test_add ("/recording/asynchronous", Fixture, NULL, fixture_setup, test_recording_async, fixture_teardown);
    g_test_add ("/properties/base", Fixture, NULL, fixture_setup, test_base_properties, fixture_teardown);
    g_test_add ("/properties/recording", Fixture, NULL, fixture_setup, test_recording_property, fixture_teardown);
    g_test_add ("/properties/binnings", Fixture, NULL, fixture_setup, test_binnings_properties, fixture_teardown);
    g_test_add ("/properties/frames-per-second", Fixture, NULL, fixture_setup, test_fps_property, fixture_teardown);
    g_test_add ("/signal", Fixture, NULL, fixture_setup, test_signal, fixture_teardown);

    return g_test_run ();
}
