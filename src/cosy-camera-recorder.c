/*
 * This file is part of cosy-camera-recorder.
 *
 * Copyright (C) 2017 - Université Catholique de Louvain
 *
 * cosy-camera-recorder is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * cosy-camera-recorder is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * cosy-camera-recorder.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Sébastien Wilmet
 */

#include <stdlib.h>
#include <locale.h>
#include <zmq.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/encoding-profile.h>
#define GST_USE_UNSTABLE_API
#include <gst/basecamerabinsrc/gstcamerabin-enum.h>

#define REPLIER_ENDPOINT "tcp://*:6001"

typedef struct _CcrApp CcrApp;

struct _CcrApp
{
	GMainLoop *main_loop;
	GstElement *pipeline;
	GstElement *camerabin;

	void *zeromq_context;
	void *zeromq_replier;

	GTimer *timer;

	guint recording : 1;
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

	g_print ("\n");

	g_list_free_full (devices, gst_object_unref);
	gst_object_unref (monitor);
}

static gchar *
get_video_filename (void)
{
	GDateTime *current_time;
	gchar *filename;

	current_time = g_date_time_new_now_local ();

	filename = g_strdup_printf ("cosy-camera-recorder-videos/%d_%.2d_%.2d-%.2d:%.2d:%.2d.mp4",
				    g_date_time_get_year (current_time),
				    g_date_time_get_month (current_time),
				    g_date_time_get_day_of_month (current_time),
				    g_date_time_get_hour (current_time),
				    g_date_time_get_minute (current_time),
				    g_date_time_get_second (current_time));

	g_date_time_unref (current_time);
	return filename;
}

static void
destroy_pipeline (CcrApp *app)
{
	if (app->pipeline != NULL)
	{
		gst_element_set_state (app->pipeline, GST_STATE_NULL);
		gst_object_unref (app->pipeline);
		app->pipeline = NULL;
	}
}

static void
bus_message_cb (GstBus     *bus,
		GstMessage *msg,
		CcrApp     *app)
{
	GstMessageType type;

	type = GST_MESSAGE_TYPE (msg);

	if (type == GST_MESSAGE_ERROR)
	{
		GError *error;
		gchar *debug;

		gst_message_parse_error (msg, &error, &debug);
		g_print ("Error: %s\n%s\n", error->message, debug);
		g_error_free (error);
		g_free (debug);

		g_main_loop_quit (app->main_loop);
	}
	else if (type == GST_MESSAGE_EOS)
	{
		g_print ("End of stream.\n\n");
		g_main_loop_quit (app->main_loop);
	}
	else if (type == GST_MESSAGE_ELEMENT &&
		 g_strcmp0 (GST_MESSAGE_SRC_NAME (msg), "camerabin") == 0)
	{
		const GstStructure *structure;

		structure = gst_message_get_structure (msg);
		g_print ("camerabin: %s\n", gst_structure_get_name (structure));
	}
}

static void
set_video_profile (GstElement *camerabin)
{
	GstEncodingContainerProfile *container_prof;
	GstEncodingVideoProfile *video_prof;
	GstCaps *caps;

	caps = gst_caps_new_simple ("video/quicktime",
				    "variant", G_TYPE_STRING, "iso",
				    NULL);
	container_prof = gst_encoding_container_profile_new ("MP4 video",
							     "Standard MP4/MPEG-4",
							     caps,
							     NULL);
	gst_caps_unref (caps);

	caps = gst_caps_new_simple ("video/mpeg",
				    "mpegversion", G_TYPE_INT, 4,
				    NULL);
	video_prof = gst_encoding_video_profile_new (caps, NULL, NULL, 0);
	gst_caps_unref (caps);

	gst_encoding_container_profile_add_profile (container_prof,
						    GST_ENCODING_PROFILE (video_prof));

	g_object_set (camerabin,
		      "video-profile", container_prof,
		      NULL);

	gst_encoding_profile_unref (container_prof);
}

static void
create_pipeline (CcrApp *app)
{
	GstBus *bus;

	g_assert (app->pipeline == NULL);
	g_assert (app->camerabin == NULL);

	app->pipeline = gst_pipeline_new ("video-capture-pipeline");

	bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));
	gst_bus_add_signal_watch (bus);

	g_signal_connect (bus,
			  "message",
			  G_CALLBACK (bus_message_cb),
			  app);

	gst_object_unref (bus);

	app->camerabin = gst_element_factory_make ("camerabin", "camerabin");
	if (app->camerabin == NULL)
	{
		g_error ("Failed to create the camerabin GStreamer element.");
	}

	g_object_set (app->camerabin,
		      "mode", MODE_VIDEO,
		      NULL);

	set_video_profile (app->camerabin);

	gst_bin_add (GST_BIN (app->pipeline), app->camerabin);
	gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
}

/* Receives the next zmq message part as a string.
 * Free the return value with g_free() when no longer needed.
 */
static gchar *
receive_next_message (void *socket)
{
	zmq_msg_t msg;
	gint n_bytes;
	gchar *str = NULL;
	gint ok;

	ok = zmq_msg_init (&msg);
	g_return_val_if_fail (ok == 0, NULL);

	n_bytes = zmq_msg_recv (&msg, socket, 0);
	if (n_bytes > 0)
	{
		void *raw_data;

		raw_data = zmq_msg_data (&msg);
		str = g_strndup (raw_data, n_bytes);
	}

	ok = zmq_msg_close (&msg);
	if (ok != 0)
	{
		g_free (str);
		g_return_val_if_reached (NULL);
	}

	return str;
}

static gchar *
start_recording (CcrApp *app)
{
	gchar *reply;
	gchar *location;

	g_print ("Start recording\n");
	app->recording = TRUE;

	if (app->timer == NULL)
	{
		app->timer = g_timer_new ();
	}
	else
	{
		g_timer_start (app->timer);
	}

	location = get_video_filename ();
	g_object_set (app->camerabin,
		      "location", location,
		      NULL);
	g_print ("Will save the video to: %s\n", location);
	g_free (location);

	g_signal_emit_by_name (app->camerabin, "start-capture");

	reply = g_strdup ("ack");
	return reply;
}

static gchar *
stop_recording (CcrApp *app)
{
	gchar *reply;

	g_print ("Stop recording\n");
	app->recording = FALSE;

	g_signal_emit_by_name (app->camerabin, "stop-capture");

	g_object_set (app->camerabin,
		      "location", NULL,
		      NULL);

	if (app->timer != NULL)
	{
		g_timer_stop (app->timer);
		reply = g_strdup_printf ("%lf", g_timer_elapsed (app->timer, NULL));
	}
	else
	{
		reply = g_strdup ("no timer");
	}

	return reply;
}

static void
read_request (CcrApp *app)
{
	gchar *request;
	gchar *reply = NULL;

	request = receive_next_message (app->zeromq_replier);
	if (request == NULL)
	{
		return;
	}

	g_print ("ZeroMQ request received: %s\n", request);

	if (g_str_equal (request, "start"))
	{
		reply = start_recording (app);
	}
	else if (g_str_equal (request, "stop"))
	{
		reply = stop_recording (app);
	}
	else
	{
		g_warning ("Unknown request: %s", request);
		reply = g_strdup ("unknown request");
	}

	g_print ("Send reply...\n");
	zmq_send (app->zeromq_replier,
		  reply,
		  strlen (reply),
		  0);
	g_print ("done.\n");

	g_free (request);
	g_free (reply);
}

static gboolean
timeout_cb (gpointer user_data)
{
	CcrApp *app = user_data;

	read_request (app);

	return G_SOURCE_CONTINUE;
}

static void
app_init (CcrApp *app)
{
	gint timeout_ms;
	gint ok;

	app->main_loop = g_main_loop_new (NULL, FALSE);

	app->zeromq_context = zmq_ctx_new ();

	app->zeromq_replier = zmq_socket (app->zeromq_context, ZMQ_REP);
	ok = zmq_bind (app->zeromq_replier, REPLIER_ENDPOINT);
	if (ok != 0)
	{
		g_error ("Error when creating ZeroMQ socket at \"" REPLIER_ENDPOINT "\": %s.\n"
			 "Is another cosy-camera-recorder process running?",
			 g_strerror (errno));
	}

	/* Non-blocking */
	timeout_ms = 0;
	ok = zmq_setsockopt (app->zeromq_replier,
			     ZMQ_RCVTIMEO,
			     &timeout_ms,
			     sizeof (gint));
	if (ok != 0)
	{
		g_error ("Error when setting ZeroMQ socket option for the replier: %s",
			 g_strerror (errno));
	}

	app->timer = NULL;
	app->recording = FALSE;

	create_pipeline (app);

	/* ZeroMQ polling every 5ms */
	g_timeout_add (5, timeout_cb, app);
	g_print ("Listening to ZeroMQ requests.\n");
}

static void
app_finalize (CcrApp *app)
{
	destroy_pipeline (app);

	zmq_close (app->zeromq_replier);
	app->zeromq_replier = NULL;

	zmq_ctx_destroy (app->zeromq_context);
	app->zeromq_context = NULL;

	if (app->timer != NULL)
	{
		g_timer_destroy (app->timer);
		app->timer = NULL;
	}

	g_main_loop_unref (app->main_loop);
}

gint
main (gint    argc,
      gchar **argv)
{
	CcrApp app = { 0 };

	setlocale (LC_ALL, "en_US.utf8");

	gst_init (&argc, &argv);

	list_devices ();

	app_init (&app);
	g_main_loop_run (app.main_loop);
	app_finalize (&app);

	return EXIT_SUCCESS;
}
