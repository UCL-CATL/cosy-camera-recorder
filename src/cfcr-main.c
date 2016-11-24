#include <stdlib.h>
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

int
main (int    argc,
      char **argv)
{
	gst_init (&argc, &argv);

	list_devices ();

	return EXIT_SUCCESS;
}
