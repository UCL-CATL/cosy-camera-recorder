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

#include <gtk/gtk.h>
#include <cheese/cheese-gtk.h>
#include <cheese/cheese-widget.h>
#include "cheese-widget-private.h"
#include <cheese/cheese-camera.h>
#include <zmq.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#define REPLIER_ENDPOINT "tcp://*:6001"

typedef struct _CcrApp CcrApp;

struct _CcrApp
{
	GtkWindow *window;
	CheeseWidget *cheese_widget;

	void *zeromq_context;
	void *zeromq_replier;

	GTimer *timer;

	guint recording : 1;
};

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
	CheeseCamera *camera;
	gchar *video_filename;

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

	camera = CHEESE_CAMERA (cheese_widget_get_camera (app->cheese_widget));
	video_filename = get_video_filename ();
	cheese_camera_start_video_recording (camera, video_filename);
	g_free (video_filename);

	reply = g_strdup ("ack");
	return reply;
}

static char *
stop_recording (CcrApp *app)
{
	char *reply;
	CheeseCamera *camera;

	g_print ("Stop recording\n");
	app->recording = FALSE;

	camera = CHEESE_CAMERA (cheese_widget_get_camera (app->cheese_widget));
	cheese_camera_stop_video_recording (camera);

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

	request = receive_next_message (app->zeromq_replier);
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
	int timeout_ms;
	int ok;

	app->window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_set_default_size (app->window, 500, 375);
	g_signal_connect (app->window, "destroy", gtk_main_quit, NULL);

	app->cheese_widget = CHEESE_WIDGET (cheese_widget_new ());

	gtk_container_add (GTK_CONTAINER (app->window),
			   GTK_WIDGET (app->cheese_widget));

	gtk_widget_show_all (GTK_WIDGET (app->window));

	app->zeromq_context = zmq_ctx_new ();

	app->zeromq_replier = zmq_socket (app->zeromq_context, ZMQ_REP);
	ok = zmq_bind (app->zeromq_replier, REPLIER_ENDPOINT);
	if (ok != 0)
	{
		g_error ("Error when creating zmq socket at \"" REPLIER_ENDPOINT "\": %s.\n"
			 "Is another cosy-fmri-camera-recorder process running?",
			 strerror (errno));
	}

	/* Non-blocking */
	timeout_ms = 0;
	ok = zmq_setsockopt (app->zeromq_replier,
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

	/* ZeroMQ polling every 5ms */
	g_timeout_add (5, timeout_cb, app);
}

static void
app_finalize (CcrApp *app)
{
	zmq_close (app->zeromq_replier);
	app->zeromq_replier = NULL;

	zmq_ctx_destroy (app->zeromq_context);
	app->zeromq_context = NULL;

	if (app->timer != NULL)
	{
		g_timer_destroy (app->timer);
		app->timer = NULL;
	}
}

int
main (int    argc,
      char **argv)
{
	CcrApp app = { 0 };

	setlocale (LC_ALL, "en_US.utf8");

	if (!cheese_gtk_init (&argc, &argv))
	{
		g_error ("Initialization failed.");
	}

	app_init (&app);
	gtk_main ();
	app_finalize (&app);

	return EXIT_SUCCESS;
}
