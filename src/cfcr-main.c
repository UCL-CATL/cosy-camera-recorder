#include <stdlib.h>
#include <locale.h>
#include <gst/gst.h>

typedef struct _CfcrApp CfcrApp;

struct _CfcrApp
{
	GMainLoop *main_loop;
	GstElement *pipeline;
};

static void
list_devices (void)
{
	GstDeviceMonitor *monitor;
	GList *devices;
	GList *l;

	monitor = gst_device_monitor_new ();
	gst_device_monitor_add_filter (monitor, NULL, NULL);

	devices = gst_device_monitor_get_devices (monitor);

	for (l = devices; l != NULL; l = l->next)
	{
		GstDevice *device = l->data;
		gchar *display_name;

		display_name = gst_device_get_display_name (device);
		g_print ("Device: %s\n", display_name);
		g_free (display_name);
	}

	g_list_free_full (devices, gst_object_unref);
	gst_object_unref (monitor);
}

static void
bus_message_error_cb (GstBus     *bus,
		      GstMessage *message,
		      CfcrApp    *app)
{
	GError *error;
	gchar *debug;

	gst_message_parse_error (message, &error, &debug);
	g_print ("Error: %s\n", error->message);
	g_error_free (error);
	g_free (debug);

	g_main_loop_quit (app->main_loop);
}

/* End-of-stream */
static void
bus_message_eos_cb (GstBus     *bus,
		    GstMessage *message,
		    CfcrApp    *app)
{
	g_main_loop_quit (app->main_loop);
}

static void
create_video_capture_pipeline (CfcrApp *app)
{
	GstBus *bus;
	GstElement *v4l2src;
	GstElement *queue;
	GstElement *mpeg4enc;
	GstElement *mp4mux;
	GstElement *filesink;

	g_assert (app->pipeline == NULL);
	app->pipeline = gst_pipeline_new ("video-capture-pipeline");

	bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));
	gst_bus_add_signal_watch (bus);

	g_signal_connect (bus,
			  "message::error",
			  G_CALLBACK (bus_message_error_cb),
			  app);

	g_signal_connect (bus,
			  "message::eos",
			  G_CALLBACK (bus_message_eos_cb),
			  app);

	gst_object_unref (bus);

	v4l2src = gst_element_factory_make ("v4l2src", NULL);
	if (v4l2src == NULL)
	{
		g_error ("Failed to create v4l2src GStreamer element.");
	}

	g_object_set (v4l2src,
		      "num-buffers", 50,
		      NULL);

	queue = gst_element_factory_make ("queue", NULL);
	if (queue == NULL)
	{
		g_error ("Failed to create queue GStreamer element.");
	}

	mpeg4enc = gst_element_factory_make ("avenc_mpeg4", NULL);
	if (mpeg4enc == NULL)
	{
		g_error ("Failed to create avenc_mpeg4 GStreamer element.");
	}

	mp4mux = gst_element_factory_make ("mp4mux", NULL);
	if (mp4mux == NULL)
	{
		g_error ("Failed to create mp4mux GStreamer element.");
	}

	filesink = gst_element_factory_make ("filesink", NULL);
	if (filesink == NULL)
	{
		g_error ("Failed to create filesink GStreamer element.");
	}

	g_object_set (filesink,
		      "location", "video.mp4",
		      NULL);

	gst_bin_add_many (GST_BIN (app->pipeline), v4l2src, queue, mpeg4enc, mp4mux, filesink, NULL);

	if (!gst_element_link_many (v4l2src, queue, mpeg4enc, mp4mux, filesink, NULL))
	{
		g_warning ("Failed to link GStreamer elements.");
	}

	gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
}

int
main (int    argc,
      char **argv)
{
	CfcrApp app = { 0 };

	setlocale (LC_ALL, "en_US.utf8");

	gst_init (&argc, &argv);

	app.main_loop = g_main_loop_new (NULL, FALSE);

	list_devices ();
	create_video_capture_pipeline (&app);

	g_main_loop_run (app.main_loop);

	gst_element_set_state (app.pipeline, GST_STATE_NULL);
	gst_object_unref (app.pipeline);
	g_main_loop_unref (app.main_loop);

	return EXIT_SUCCESS;
}
