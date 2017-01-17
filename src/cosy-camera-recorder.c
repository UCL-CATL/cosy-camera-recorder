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
#include <gst/gst.h>
#include <zmq.h>
#include <string.h>

#define REPLIER_ENDPOINT "tcp://*:6001"

typedef struct _CcrApp CcrApp;

struct _CcrApp
{
	GMainLoop *main_loop;
	GstElement *pipeline;

	/* ZeroMQ */
	void *context;
	void *replier;

	GTimer *timer;

	guint recording : 1;
};

/* Prototypes */
static void create_pipeline (CcrApp *app);

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
bus_message_error_cb (GstBus     *bus,
		      GstMessage *message,
		      CcrApp     *app)
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
		    CcrApp     *app)
{
	g_print ("End of stream.\n\n");

	destroy_pipeline (app);
	create_pipeline (app);
}

static void
create_pipeline (CcrApp *app)
{
	GstBus *bus;
	GstElement *v4l2src;
	GstElement *queue;
	GstElement *mpeg4enc;
	GstElement *mp4mux;
	GstElement *filesink;
	gchar *filename;

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

	filename = get_video_filename ();
	g_object_set (filesink,
		      "location", filename,
		      NULL);

	gst_bin_add_many (GST_BIN (app->pipeline), v4l2src, queue, mpeg4enc, mp4mux, filesink, NULL);

	if (!gst_element_link_many (v4l2src, queue, mpeg4enc, mp4mux, filesink, NULL))
	{
		g_warning ("Failed to link GStreamer elements.");
	}

	gst_element_set_state (app->pipeline, GST_STATE_PAUSED);

	g_print ("Listening to ZeroMQ requests.\n");
	g_print ("Will save the video to: %s\n", filename);

	g_free (filename);
}

/* Receives the next zmq message part as a string.
 * Free the return value with g_free() when no longer needed.
 */
static char *
receive_next_message (void *socket)
{
	zmq_msg_t msg;
	int n_bytes;
	char *str = NULL;
	int ok;

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

static char *
start_recording (CcrApp *app)
{
	char *reply;

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

	gst_element_set_state (app->pipeline, GST_STATE_PLAYING);

	reply = g_strdup ("ack");
	return reply;
}

static char *
stop_recording (CcrApp *app)
{
	char *reply;

	g_print ("Stop recording\n");
	app->recording = FALSE;

	gst_element_send_event (app->pipeline, gst_event_new_eos ());

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
	char *request;
	char *reply = NULL;

	request = receive_next_message (app->replier);
	if (request == NULL)
	{
		return;
	}

	g_print ("Request from cosy-pupil-client: %s\n", request);

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

	g_print ("Send reply to cosy-pupil-client...\n");
	zmq_send (app->replier,
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
	int timeout_ms;
	int ok;

	app->main_loop = g_main_loop_new (NULL, FALSE);

	app->context = zmq_ctx_new ();

	app->replier = zmq_socket (app->context, ZMQ_REP);
	ok = zmq_bind (app->replier, REPLIER_ENDPOINT);
	if (ok != 0)
	{
		g_error ("Error when creating zmq socket at \"" REPLIER_ENDPOINT "\": %s.\n"
			 "Is another cosy-fmri-camera-recorder process running?",
			 strerror (errno));
	}

	/* Non-blocking */
	timeout_ms = 0;
	ok = zmq_setsockopt (app->replier,
			     ZMQ_RCVTIMEO,
			     &timeout_ms,
			     sizeof (int));
	if (ok != 0)
	{
		g_error ("Error when setting zmq socket option for the replier: %s",
			 strerror (errno));
	}

	app->timer = NULL;
	app->recording = FALSE;

	create_pipeline (app);

	/* ZeroMQ polling every 5ms */
	g_timeout_add (5, timeout_cb, app);
}

static void
app_finalize (CcrApp *app)
{
	destroy_pipeline (app);

	zmq_close (app->replier);
	app->replier = NULL;

	zmq_ctx_destroy (app->context);
	app->context = NULL;

	if (app->timer != NULL)
	{
		g_timer_destroy (app->timer);
		app->timer = NULL;
	}

	g_main_loop_unref (app->main_loop);
}

int
main (int    argc,
      char **argv)
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
