#include <stdlib.h>
#include <locale.h>
#include <gst/gst.h>

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
create_video_capture_pipeline (void)
{
	GstElement *pipeline;
	GstElement *v4l2src;
	GstElement *queue;
	GstElement *mpeg4enc;
	GstElement *mp4mux;
	GstElement *filesink;

	pipeline = gst_pipeline_new ("video-capture-pipeline");

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

	gst_bin_add_many (GST_BIN (pipeline), v4l2src, queue, mpeg4enc, mp4mux, filesink, NULL);

	if (!gst_element_link_many (v4l2src, queue, mpeg4enc, mp4mux, filesink, NULL))
	{
		g_warning ("Failed to link GStreamer elements.");
	}
}

int
main (int    argc,
      char **argv)
{
	setlocale (LC_ALL, "en_US.utf8");

	gst_init (&argc, &argv);

	list_devices ();
	create_video_capture_pipeline ();

	return EXIT_SUCCESS;
}
