
#include <glib.h>
#include "uca-camera.h"
#include "uca-plugin-manager.h"

typedef struct {
    UcaPluginManager *manager;
    UcaCamera *camera;
} Fixture;

static gchar *
build_mock_plugin_path (void)
{
    gchar *cwd;
    gchar *plugin_path;

    cwd = g_get_current_dir ();
    plugin_path = g_build_filename (cwd, "plugins", "mock", NULL);
    g_free (cwd);
    return plugin_path;
}

static void
fixture_setup (Fixture *fixture, gconstpointer data)
{
    gchar *plugin_path;
    GError *error = NULL;

    plugin_path = build_mock_plugin_path ();
    g_setenv ("UCA_CAMERA_PATH", plugin_path, TRUE);
    g_free (plugin_path);

    fixture->manager = uca_plugin_manager_new ();
    fixture->camera = uca_plugin_manager_get_camera (fixture->manager,
                                                     "mock", &error, NULL);
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
    UcaCamera *camera = uca_plugin_manager_get_camera (fixture->manager,
                                                       "fox994m3a0yxmy", &error, NULL);
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
            "frames-per-second", 10.0,
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
    g_object_get (G_OBJECT (camera), "is-recording", &is_recording, NULL);
    g_assert (is_recording == TRUE);
    g_assert (uca_camera_is_recording (camera));

    uca_camera_stop_recording (camera, NULL);
    g_object_get (G_OBJECT (camera), "is-recording", &is_recording, NULL);
    g_assert (is_recording == FALSE);
    g_assert (!uca_camera_is_recording (camera));
}

static void
test_recording_buffered (Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA (fixture->camera);
    GError *error = NULL;
    guint width, height, bitdepth;
    gsize buffer_size;
    gchar *buffer;

    g_object_get (G_OBJECT (camera),
                  "roi-width", &width,
                  "roi-height", &height,
                  "sensor-bitdepth", &bitdepth,
                  NULL);

    buffer_size = width * height * (bitdepth <= 8 ? 1 : 2);
    buffer = g_malloc0 (buffer_size);

    g_object_set (G_OBJECT (camera),
                  "buffered", TRUE,
                  "num-buffers", 5,
                  NULL);

    uca_camera_start_recording (camera, &error);
    g_assert_no_error (error);

    for (int i = 0; i < 10; i++) {
        g_assert (uca_camera_grab (camera, (gpointer) buffer, &error));
        g_assert_no_error (error);
    }

    uca_camera_stop_recording (camera, &error);
    g_assert_no_error (error);

    g_free (buffer);
}

static void
test_metadata (Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA (fixture->camera);
    GError *error = NULL;
    guint width, height, bitdepth;
    gsize buffer_size;
    gchar *buffer;
    GHashTable *metadata = g_hash_table_new (g_str_hash, g_str_equal);

    g_object_get (G_OBJECT (camera),
                  "roi-width", &width,
                  "roi-height", &height,
                  "sensor-bitdepth", &bitdepth,
                  NULL);

    buffer_size = width * height * (bitdepth <= 8 ? 1 : 2);
    buffer = g_malloc0 (buffer_size);

    g_object_set (G_OBJECT (camera),
                  "buffered", FALSE,
                  NULL);

    uca_camera_start_recording (camera, &error);
    g_assert_no_error (error);

    for (int i = 0; i < 10; i++) {
        g_assert (uca_camera_grab_with_metadata (camera, (gpointer) buffer,(gpointer) metadata ,&error));
        g_assert_no_error (error);
        g_assert(metadata != NULL);
        g_assert (g_hash_table_size (metadata) > 0);
        g_assert (g_hash_table_contains (metadata, "timestamp"));
        g_assert (g_hash_table_contains (metadata, "frame_number"));
        const guint f = *(guint *) g_hash_table_lookup(metadata, "frame_number");
        g_assert (f == i);
    }

    uca_camera_stop_recording (camera, &error);
    g_assert_no_error (error);

    g_free (buffer);
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
test_property_units (Fixture *fixture, gconstpointer data)
{
    /* Default camera properties */
    g_assert (uca_camera_get_unit (fixture->camera, "sensor-width") == UCA_UNIT_PIXEL);
    g_assert (uca_camera_get_unit (fixture->camera, "name") == UCA_UNIT_NA);

    /* Mock-specific properties */
    g_assert (uca_camera_get_unit (fixture->camera, "frames-per-second") == UCA_UNIT_COUNT);
}

static void
test_signal (Fixture *fixture, gconstpointer data)
{
    UcaCamera *camera = UCA_CAMERA (fixture->camera);
    gboolean success = FALSE;
    g_signal_connect (camera, "notify::frames-per-second", (GCallback) on_property_change, &success);
    g_object_set (G_OBJECT (camera), "frames-per-second", 30.0, NULL);
    g_assert (success == TRUE);
}

static void
test_overwriting_units (Fixture *fixture, gconstpointer data)
{
    uca_camera_register_unit (fixture->camera, "sensor-width", UCA_UNIT_PIXEL);
}

static void
test_can_be_written (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    /* read-only cannot ever be written */
    g_assert (!uca_camera_is_writable_during_acquisition (fixture->camera, "name"));

    /* unset properties cannot be written */
    g_assert (!uca_camera_is_writable_during_acquisition (fixture->camera, "roi-width"));

    /* check trivial cases */
    uca_camera_set_writable (fixture->camera, "roi-width", TRUE);
    g_assert (uca_camera_is_writable_during_acquisition (fixture->camera, "roi-width"));

    uca_camera_set_writable (fixture->camera, "roi-height", FALSE);
    g_assert (!uca_camera_is_writable_during_acquisition (fixture->camera, "roi-height"));

    /* Now, do a real test */
    uca_camera_set_writable (fixture->camera, "roi-height", TRUE);
    uca_camera_start_recording (fixture->camera, &error);
    g_assert_no_error (error);
    g_object_set (fixture->camera, "roi-height", 128, NULL);
#if (GLIB_CHECK_VERSION (2, 34, 0))
    uca_camera_set_writable (fixture->camera, "roi-x0", FALSE);
    g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Property 'roi-x0' cant be changed during acquisition");
    g_object_set (fixture->camera, "roi-x0", 64, NULL);
    g_test_assert_expected_messages ();
#endif
    uca_camera_stop_recording (fixture->camera, &error);
    g_assert_no_error (error);
}

static void
test_factory_hashtable (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    guint checkvalue = 42;

    gchar *foo = "roi-x0";
    gchar *bar = "roi-y0";
    GValue baz = { 0 };
    g_value_init(&baz, G_TYPE_UINT);
    g_value_set_uint(&baz, checkvalue);

    GHashTable *ght = g_hash_table_new (NULL, NULL);
    g_hash_table_insert(ght, foo, &baz);
    g_hash_table_insert(ght, bar, &baz);

    UcaCamera *camera = uca_plugin_manager_get_camerah (fixture->manager,
                                                     "mock", ght, &error);
    g_hash_table_destroy(ght);

    g_assert (error == NULL);
    g_assert (camera);

    guint roi_x0 = 0;
    g_object_get (G_OBJECT (camera), "roi-x0", &roi_x0, NULL);
    g_assert (roi_x0 == checkvalue);

    guint roi_y0 = 0;
    g_object_get (G_OBJECT (camera), "roi-y0", &roi_y0, NULL);
    g_assert (roi_y0 == checkvalue);

    g_object_unref(camera);
}

int main (int argc, char *argv[])
{
    gsize n_tests;

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init ();
#endif

    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://ufo.kit.edu/ufo/ticket");

    struct {
        const gchar *name;
        void (*test_func) (Fixture *fixture, gconstpointer data);
    }
    tests[] = {
        {"/factory", test_factory},
        {"/factory/hashtable", test_factory_hashtable},
        {"/signal", test_signal},
        {"/recording", test_recording},
        {"/recording/signal", test_recording_signal},
        {"/recording/asynchronous", test_recording_async},
        {"/recording/buffered", test_recording_buffered},
        {"/recording/metadata", test_metadata},
        {"/properties/base", test_base_properties},
        {"/properties/recording", test_recording_property},
        {"/properties/frames-per-second", test_fps_property},
        {"/properties/units", test_property_units},
        {"/properties/units/overwrite", test_overwriting_units},
        {"/properties/can-be-written", test_can_be_written},
    };

    n_tests = sizeof(tests) / sizeof(tests[0]);

    for (gsize i = 0; i < n_tests; i++)
        g_test_add (tests[i].name, Fixture, NULL, fixture_setup, tests[i].test_func, fixture_teardown);

    return g_test_run ();
}
