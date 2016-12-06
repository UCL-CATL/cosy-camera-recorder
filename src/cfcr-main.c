#include <stdlib.h>
#include <locale.h>
#include <gst/gst.h>

typedef struct _CfcrApp CfcrApp;

struct _CfcrApp
{
	GMainLoop *main_loop;
	GstElement *pipeline;
	guint bus_watch_id;
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

static gboolean
bus_watch_cb (GstBus     *bus,
	      GstMessage *message,
	      gpointer    user_data)
{
	CfcrApp *app = user_data;
	GstMessageType msg_type;

	g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

	msg_type = GST_MESSAGE_TYPE (message);

	if (msg_type == GST_MESSAGE_ERROR)
	{
		GError *error;
		gchar *debug;

		gst_message_parse_error (message, &error, &debug);
		g_print ("Error: %s\n", error->message);
		g_error_free (error);
		g_free (debug);

		g_main_loop_quit (app->main_loop);
	}
	else if (msg_type == GST_MESSAGE_EOS)
	{
		/* end-of-stream */
		g_main_loop_quit (app->main_loop);
	}

	return TRUE;
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
	g_assert (app->bus_watch_id == 0);
	app->bus_watch_id = gst_bus_add_watch (bus, bus_watch_cb, app);
	gst_object_unref (bus);

	v4l2src = gst_element_factory_make ("v4l2src", NULL);
	g_object_set (v4l2src,
		      "num-buffers", 50,
		      NULL);

	queue = gst_element_factory_make ("queue", NULL);
	mpeg4enc = gst_element_factory_make ("avenc_mpeg4", NULL);
	mp4mux = gst_element_factory_make ("mp4mux", NULL);

	filesink = gst_element_factory_make ("filesink", NULL);
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
	g_source_remove (app.bus_watch_id);
	g_main_loop_unref (app.main_loop);

	return EXIT_SUCCESS;
}
