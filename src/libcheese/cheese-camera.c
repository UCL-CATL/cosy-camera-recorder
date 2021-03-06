/*
 * Copyright © 2007,2008 Jaap Haitsma <jaap@haitsma.org>
 * Copyright © 2007-2009 daniel g. siegel <dgsiegel@gnome.org>
 * Copyright © 2008 Ryan Zeigler <zeiglerr@gmail.com>
 * Copyright © 2010 Yuvaraj Pandian T <yuvipanda@yuvi.in>
 * Copyright © 2011 Luciana Fujii Pontello <luciana@fujii.eti.br>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#include <string.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <clutter/clutter.h>
#include <clutter-gst/clutter-gst.h>
#include <gst/gst.h>
/* Avoid a warning. */
#define GST_USE_UNSTABLE_API
#include <gst/basecamerabinsrc/gstcamerabin-enum.h>
#include <gst/pbutils/encoding-profile.h>

#include "cheese-camera.h"
#include "cheese-camera-device.h"
#include "cheese-camera-device-monitor.h"

/**
 * SECTION:cheese-camera
 * @short_description: A representation of the video capture device inside
 * #CheeseWidget
 * @stability: Unstable
 * @include: cheese/cheese-camera.h
 *
 * #CheeseCamera represents the video capture device used to drive a
 * #CheeseWidget.
 */

struct _CheeseCameraPrivate
{
  GstBus *bus;

  GstElement *camerabin;
  GstElement *video_filter_bin;

  GstElement *video_source;
  GstElement *camera_source;

  ClutterActor *video_texture;

  GstElement *video_balance;
  GstElement *main_valve;

  gboolean is_recording;
  gboolean pipeline_is_playing;

  guint num_camera_devices;
  CheeseCameraDevice *device;

  /* an array of CheeseCameraDevices */
  GPtrArray *camera_devices;
  guint selected_device;
  CheeseVideoFormat *current_format;

  gchar *initial_name;

  CheeseCameraDeviceMonitor *monitor;
};

G_DEFINE_TYPE_WITH_PRIVATE (CheeseCamera, cheese_camera, G_TYPE_OBJECT)

#define CHEESE_CAMERA_ERROR cheese_camera_error_quark ()

enum
{
  PROP_0,
  PROP_VIDEO_TEXTURE,
  PROP_DEVICE,
  PROP_FORMAT,
  PROP_NUM_CAMERA_DEVICES,
  PROP_LAST
};

enum
{
  VIDEO_SAVED,
  STATE_FLAGS_CHANGED,
  LAST_SIGNAL
};

static guint camera_signals[LAST_SIGNAL];
static GParamSpec *properties[PROP_LAST];

GST_DEBUG_CATEGORY (cheese_camera_cat);
#define GST_CAT_DEFAULT cheese_camera_cat

GQuark cheese_camera_error_quark (void);

GQuark
cheese_camera_error_quark (void)
{
  return g_quark_from_static_string ("cheese-camera-error-quark");
}

/*
 * cheese_camera_bus_message_cb:
 * @bus: a #GstBus
 * @message: the #GstMessage
 * @camera: the #CheeseCamera
 *
 * Process messages create by the @camera on the @bus. Emit
 * ::state-flags-changed if the state of the camera has changed.
 */
static void
cheese_camera_bus_message_cb (GstBus *bus, GstMessage *message, CheeseCamera *camera)
{
  CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);
    GstMessageType type;

    type = GST_MESSAGE_TYPE (message);

    if (type == GST_MESSAGE_WARNING)
    {
      GError *err = NULL;
      gchar *debug = NULL;
      gst_message_parse_warning (message, &err, &debug);

      if (err && err->message) {
        g_warning ("%s: %s\n", err->message, debug);
        g_error_free (err);
      } else {
        g_warning ("Unparsable GST_MESSAGE_WARNING message.\n");
      }

      g_free (debug);
    }
    else if (type == GST_MESSAGE_ERROR)
    {
      GError *err = NULL;
      gchar *debug = NULL;
      gst_message_parse_error (message, &err, &debug);

      if (err && err->message) {
        g_warning ("%s: %s\n", err->message, debug);
        g_error_free (err);
      } else {
        g_warning ("Unparsable GST_MESSAGE_ERROR message.\n");
      }

      cheese_camera_stop (camera);
      g_signal_emit (camera, camera_signals[STATE_FLAGS_CHANGED], 0,
                     GST_STATE_NULL);
      g_free (debug);
    }
    else if (type == GST_MESSAGE_STATE_CHANGED)
    {
      if (strcmp (GST_MESSAGE_SRC_NAME (message), "camerabin") == 0)
      {
        GstState old, new;
        gst_message_parse_state_changed (message, &old, &new, NULL);
        if (new == GST_STATE_PLAYING)
        {
          g_signal_emit (camera, camera_signals[STATE_FLAGS_CHANGED], 0, new);
        }
      }
    }
    else if (type == GST_MESSAGE_ELEMENT)
    {
      const GstStructure *structure;
      if (strcmp (GST_MESSAGE_SRC_NAME (message), "camerabin") == 0)
      {
        structure = gst_message_get_structure (message);
        if (strcmp (gst_structure_get_name (structure), "video-done") == 0)
        {
          g_signal_emit (camera, camera_signals[VIDEO_SAVED], 0);
          priv->is_recording = FALSE;
        }
      }
    }
}

/*
 * cheese_camera_add_device:
 * @monitor: a #CheeseCameraDeviceMonitor
 * @device: a #CheeseCameraDevice
 * @camera: a #CheeseCamera
 *
 * Handle the CheeseCameraDeviceMonitor::added signal and add the new
 * #CheeseCameraDevice to the list of current devices.
 */
static void
cheese_camera_add_device (CheeseCameraDeviceMonitor *monitor,
			  CheeseCameraDevice        *device,
                          CheeseCamera              *camera)
{
    CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  g_ptr_array_add (priv->camera_devices, device);
  priv->num_camera_devices++;

  g_object_notify_by_pspec (G_OBJECT (camera), properties[PROP_NUM_CAMERA_DEVICES]);
}

/*
 * cheese_camera_remove_device:
 * @monitor: a #CheeseCameraDeviceMonitor
 * @device: a #CheeseCameraDevice
 * @camera: a #CheeseCamera
 *
 * Handle the CheeseCameraDeviceMonitor::removed signal and remove the
 * #CheeseCameraDevice from the list of current devices.
 */
static void
cheese_camera_remove_device (CheeseCameraDeviceMonitor *monitor,
			     CheeseCameraDevice        *device,
                             CheeseCamera              *camera)
{
  CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  if (g_ptr_array_remove (priv->camera_devices, (gpointer) device))
  {
     priv->num_camera_devices--;
     g_object_notify_by_pspec (G_OBJECT (camera), properties[PROP_NUM_CAMERA_DEVICES]);
  }
}

/*
 * cheese_camera_detect_camera_devices:
 * @camera: a #CheeseCamera
 *
 * Enumerate the physical camera devices present on the system, and add them to
 * the list of #CheeseCameraDevice objects.
 */
static void
cheese_camera_detect_camera_devices (CheeseCamera *camera)
{
    CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  priv->num_camera_devices = 0;
  priv->camera_devices     = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  priv->monitor = cheese_camera_device_monitor_new ();
  g_signal_connect (G_OBJECT (priv->monitor), "added",
                    G_CALLBACK (cheese_camera_add_device), camera);
  g_signal_connect (G_OBJECT (priv->monitor), "removed",
                    G_CALLBACK (cheese_camera_remove_device), camera);

  cheese_camera_device_monitor_coldplug (priv->monitor);
}

/*
 * cheese_camera_set_camera_source:
 * @camera: a #CheeseCamera
 *
 * Set the currently-selected video capture device as the source for a video
 * steam.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 */
static gboolean
cheese_camera_set_camera_source (CheeseCamera *camera)
{
    CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  guint i;
  CheeseCameraDevice *selected_camera;
  GstElement *src, *filter;
  GstPad *srcpad;

  if (priv->video_source)
    gst_object_unref (priv->video_source);

  /* If we have a matching video device use that one, otherwise use the first */
  priv->selected_device = 0;
  selected_camera = g_ptr_array_index (priv->camera_devices, 0);

  for (i = 1; i < priv->num_camera_devices; i++)
  {
    CheeseCameraDevice *dev = g_ptr_array_index (priv->camera_devices, i);
    if (dev == priv->device)
    {
      selected_camera       = dev;
      priv->selected_device = i;
      break;
    }
  }

  priv->video_source = gst_bin_new (NULL);
  if (priv->video_source == NULL)
  {
    return FALSE;
  }

  src = cheese_camera_device_get_src (selected_camera);
  gst_bin_add (GST_BIN (priv->video_source), src);

  filter = gst_element_factory_make ("capsfilter", "video_source_filter");
  gst_bin_add (GST_BIN (priv->video_source), filter);

  gst_element_link (src, filter);

  srcpad = gst_element_get_static_pad (filter, "src");
  gst_element_add_pad (priv->video_source,
                       gst_ghost_pad_new ("src", srcpad));
  gst_object_unref (srcpad);

  return TRUE;
}

/*
 * cheese_camera_set_error_element_not_found:
 * @error: return location for errors, or %NULL
 * @factoryname: the name of the #GstElement which was not found
 *
 * Create a #GError to warn that a required GStreamer element was not found.
 */
static void
cheese_camera_set_error_element_not_found (GError **error, const gchar *factoryname)
{
  g_return_if_fail (error == NULL || *error == NULL);

  g_set_error (error, CHEESE_CAMERA_ERROR, CHEESE_CAMERA_ERROR_ELEMENT_NOT_FOUND, "%s%s.", _("One or more needed GStreamer elements are missing: "), factoryname);
}

/*
 * cheese_camera_set_video_recording:
 * @camera: a #CheeseCamera
 * @error: a return location for errors, or %NULL
 *
 */
static void
cheese_camera_set_video_recording (CheeseCamera *camera, GError **error)
{
  CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);
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

  /* Fixed framerate, so that the timestamps in Pupil Capture will normally be
   * accurate.
   * FALSE is the default value, but the code is there to be explicit, and to
   * know that the function exists (and in case the API breaks in the future).
   */
  gst_encoding_video_profile_set_variableframerate (video_prof, FALSE);

  gst_encoding_container_profile_add_profile (container_prof,
                                              GST_ENCODING_PROFILE (video_prof));

  g_object_set (priv->camerabin,
                "video-profile", container_prof,
                NULL);

  gst_encoding_profile_unref (container_prof);
}

/*
 * cheese_camera_create_video_filter_bin:
 * @camera: a #CheeseCamera
 * @error: a return location for errors, or %NULL
 *
 * Create the #GstBin for video filtering.
 *
 * Returns: %TRUE if the bin creation was successful, %FALSE and sets @error
 * otherwise
 */
static gboolean
cheese_camera_create_video_filter_bin (CheeseCamera *camera, GError **error)
{
    CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  gboolean ok = TRUE;
  GstPad  *pad;

  priv->video_filter_bin = gst_bin_new ("video_filter_bin");

  if ((priv->main_valve = gst_element_factory_make ("valve", "main_valve")) == NULL)
  {
    cheese_camera_set_error_element_not_found (error, "main_valve");
    return FALSE;
  }
  if ((priv->video_balance = gst_element_factory_make ("videobalance", "video_balance")) == NULL)
  {
    cheese_camera_set_error_element_not_found (error, "videobalance");
    return FALSE;
  }

  gst_bin_add_many (GST_BIN (priv->video_filter_bin),
                    priv->main_valve, priv->video_balance, NULL);

  ok &= gst_element_link_many (priv->main_valve, priv->video_balance, NULL);

  /* add ghostpads */

  pad = gst_element_get_static_pad (priv->video_balance, "src");
  gst_element_add_pad (priv->video_filter_bin, gst_ghost_pad_new ("src", pad));
  gst_object_unref (GST_OBJECT (pad));

  pad = gst_element_get_static_pad (priv->main_valve, "sink");
  gst_element_add_pad (priv->video_filter_bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (GST_OBJECT (pad));

  if (!ok)
    g_error ("Unable to create filter bin");

  return TRUE;
}

/*
 * cheese_camera_get_num_camera_devices:
 * @camera: a #CheeseCamera
 *
 * Get the number of #CheeseCameraDevice found on the system managed by
 * @camera.
 *
 * Returns: the number of #CheeseCameraDevice objects on the system
 */
static guint
cheese_camera_get_num_camera_devices (CheeseCamera *camera)
{
    CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  return priv->num_camera_devices;
}

/**
 * cheese_camera_get_selected_device:
 * @camera: a #CheeseCamera
 *
 * Get the currently-selected #CheeseCameraDevice of the @camera.
 *
 * Returns: (transfer none): a #CheeseCameraDevice, or %NULL if there is no
 * selected device
 */
CheeseCameraDevice *
cheese_camera_get_selected_device (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv;

  g_return_val_if_fail (CHEESE_IS_CAMERA (camera), NULL);

    priv = cheese_camera_get_instance_private (camera);

  if (cheese_camera_get_num_camera_devices (camera) > 0)
    return CHEESE_CAMERA_DEVICE (
             g_ptr_array_index (priv->camera_devices, priv->selected_device));
  else
    return NULL;
}

/**
 * cheese_camera_switch_camera_device:
 * @camera: a #CheeseCamera
 *
 * Toggle the playing/recording state of the @camera.
 */
void
cheese_camera_switch_camera_device (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv;
  gboolean pipeline_was_playing;

  g_return_if_fail (CHEESE_IS_CAMERA (camera));

    priv = cheese_camera_get_instance_private (camera);

  /* gboolean was_recording        = FALSE; */
  pipeline_was_playing = FALSE;

  if (priv->is_recording)
  {
    cheese_camera_stop_video_recording (camera);
    /* was_recording = TRUE; */
  }

  if (priv->pipeline_is_playing)
  {
    cheese_camera_stop (camera);
    pipeline_was_playing = TRUE;
  }

  cheese_camera_set_camera_source (camera);

  if (pipeline_was_playing)
  {
    cheese_camera_play (camera);
  }

  /* if (was_recording)
   * {
   * Restart recording... ?
   * } */
}

/**
 * cheese_camera_play:
 * @camera: a #CheeseCamera
 *
 * Set the state of the GStreamer pipeline associated with the #CheeseCamera to
 * playing.
 */

static void
cheese_camera_set_new_caps (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv;
  CheeseCameraDevice *device;
  GstCaps *caps;

  g_return_if_fail (CHEESE_IS_CAMERA (camera));

    priv = cheese_camera_get_instance_private (camera);
  device = g_ptr_array_index (priv->camera_devices, priv->selected_device);
  caps = cheese_camera_device_get_caps_for_format (device, priv->current_format);

  if (gst_caps_is_empty (caps))
  {
    gst_caps_unref (caps);
    g_boxed_free (CHEESE_TYPE_VIDEO_FORMAT, priv->current_format);
    priv->current_format = cheese_camera_device_get_best_format (device);
    g_object_notify_by_pspec (G_OBJECT (camera), properties[PROP_FORMAT]);
    caps = cheese_camera_device_get_caps_for_format (device, priv->current_format);
  }

  if (!gst_caps_is_empty (caps))
  {
    GST_INFO_OBJECT (camera, "SETTING caps %" GST_PTR_FORMAT, caps);
    g_object_set (gst_bin_get_by_name (GST_BIN (priv->video_source),
                  "video_source_filter"), "caps", caps, NULL);
    g_object_set (priv->camerabin, "viewfinder-caps", caps,
                  "image-capture-caps", caps, NULL);

    /* GStreamer >= 1.1.4 expects fully-specified video-capture-source caps. */
    caps = gst_caps_fixate (caps);

    g_object_set (priv->camerabin, "video-capture-caps", caps, NULL);
  }
  gst_caps_unref (caps);
}

void
cheese_camera_play (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);
  cheese_camera_set_new_caps (camera);
  g_object_set (priv->camera_source, "video-source", priv->video_source, NULL);
  g_object_set (priv->main_valve, "drop", FALSE, NULL);
  gst_element_set_state (priv->camerabin, GST_STATE_PLAYING);
  priv->pipeline_is_playing = TRUE;
}

/**
 * cheese_camera_stop:
 * @camera: a #CheeseCamera
 *
 * Set the state of the GStreamer pipeline associated with the #CheeseCamera to
 * NULL.
 */
void
cheese_camera_stop (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv;

  g_return_if_fail (CHEESE_IS_CAMERA (camera));

    priv = cheese_camera_get_instance_private (camera);

  if (priv->camerabin != NULL)
    gst_element_set_state (priv->camerabin, GST_STATE_NULL);
  priv->pipeline_is_playing = FALSE;
}

/*
 * cheese_camera_set_tags:
 * @camera: a #CheeseCamera
 *
 * Set tags on the camerabin element, such as the stream creation time and the
 * name of the application. Call this just before starting the capture process.
 */
static void
cheese_camera_set_tags (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv;
  CheeseCameraDevice *device;
  const gchar *device_name;
  GstDateTime *datetime;
  GstTagList *taglist;

  device = cheese_camera_get_selected_device (camera);
  device_name = cheese_camera_device_get_name (device);

  datetime = gst_date_time_new_now_local_time();

  taglist = gst_tag_list_new (
      GST_TAG_APPLICATION_NAME, PACKAGE_STRING,
      GST_TAG_DATE_TIME, datetime,
      GST_TAG_DEVICE_MODEL, device_name,
      GST_TAG_KEYWORDS, PACKAGE_NAME, NULL);

    priv = cheese_camera_get_instance_private (camera);
  gst_tag_setter_merge_tags (GST_TAG_SETTER (priv->camerabin), taglist,
        GST_TAG_MERGE_REPLACE);

  gst_date_time_unref (datetime);
  gst_tag_list_unref (taglist);
}

/**
 * cheese_camera_start_video_recording:
 * @camera: a #CheeseCamera
 * @filename: (type filename): the name of the video file to where the
 * recording will be saved
 *
 * Start a video recording with the @camera and save it to @filename.
 */
void
cheese_camera_start_video_recording (CheeseCamera *camera, const gchar *filename)
{
  CheeseCameraPrivate *priv;

  g_return_if_fail (CHEESE_IS_CAMERA (camera));

    priv = cheese_camera_get_instance_private (camera);

  g_object_set (priv->camerabin, "mode", MODE_VIDEO, NULL);
  g_object_set (priv->camerabin, "location", filename, NULL);
  cheese_camera_set_tags (camera);
  g_signal_emit_by_name (priv->camerabin, "start-capture", 0);
  priv->is_recording = TRUE;
}

/*
 * cheese_camera_force_stop_video_recording:
 * @data: a #CheeseCamera
 *
 * Forcibly stop a #CheeseCamera from recording video.
 *
 * Returns: %FALSE
 */
static gboolean
cheese_camera_force_stop_video_recording (gpointer data)
{
  CheeseCamera        *camera = CHEESE_CAMERA (data);
    CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  if (priv->is_recording)
  {
    GST_WARNING ("Cannot cleanly shutdown recording pipeline, forcing");
    g_signal_emit (camera, camera_signals[VIDEO_SAVED], 0);

    cheese_camera_stop (camera);
    cheese_camera_play (camera);
    priv->is_recording = FALSE;
  }

  return FALSE;
}

/**
 * cheese_camera_stop_video_recording:
 * @camera: a #CheeseCamera
 *
 * Stop recording video on the @camera.
 */
void
cheese_camera_stop_video_recording (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv;
  GstState             state;

  g_return_if_fail (CHEESE_IS_CAMERA (camera));

    priv = cheese_camera_get_instance_private (camera);

  gst_element_get_state (priv->camerabin, &state, NULL, 0);

  if (state == GST_STATE_PLAYING)
  {
    g_signal_emit_by_name (priv->camerabin, "stop-capture", 0);
  }
  else
  {
    cheese_camera_force_stop_video_recording (camera);
  }
}

static void
cheese_camera_finalize (GObject *object)
{
  CheeseCamera *camera;
  CheeseCameraPrivate *priv;

  camera = CHEESE_CAMERA (object);
    priv = cheese_camera_get_instance_private (camera);

  cheese_camera_stop (camera);

  if (priv->camerabin != NULL)
    gst_object_unref (priv->camerabin);

  g_clear_object (&priv->device);
  g_boxed_free (CHEESE_TYPE_VIDEO_FORMAT, priv->current_format);

  /* Free CheeseCameraDevice array */
  g_ptr_array_free (priv->camera_devices, TRUE);

  g_free (priv->initial_name);
  g_clear_object (&priv->monitor);

  G_OBJECT_CLASS (cheese_camera_parent_class)->finalize (object);
}

static void
cheese_camera_get_property (GObject *object, guint prop_id, GValue *value,
                            GParamSpec *pspec)
{
  CheeseCamera *self;
  CheeseCameraPrivate *priv;

  self = CHEESE_CAMERA (object);
    priv = cheese_camera_get_instance_private (self);

  switch (prop_id)
  {
    case PROP_VIDEO_TEXTURE:
      g_value_set_pointer (value, priv->video_texture);
      break;
    case PROP_DEVICE:
      g_value_set_object (value, priv->device);
      break;
    case PROP_FORMAT:
      g_value_set_boxed (value, priv->current_format);
      break;
    case PROP_NUM_CAMERA_DEVICES:
      g_value_set_uint (value, priv->num_camera_devices);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
cheese_camera_set_property (GObject *object, guint prop_id, const GValue *value,
                            GParamSpec *pspec)
{
  CheeseCamera *self;
  CheeseCameraPrivate *priv;

  self = CHEESE_CAMERA (object);
  priv = cheese_camera_get_instance_private (self);

  switch (prop_id)
  {
    case PROP_VIDEO_TEXTURE:
      priv->video_texture = g_value_get_pointer (value);
      break;
    case PROP_DEVICE:
      g_clear_object (&priv->device);
      priv->device = g_value_dup_object (value);
      break;
    case PROP_FORMAT:
      if (priv->current_format != NULL)
        g_boxed_free (CHEESE_TYPE_VIDEO_FORMAT, priv->current_format);
      priv->current_format = g_value_dup_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
cheese_camera_class_init (CheeseCameraClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  if (cheese_camera_cat == NULL)
    GST_DEBUG_CATEGORY_INIT (cheese_camera_cat,
                             "cheese-camera",
                             0, "Cheese Camera");

  object_class->finalize     = cheese_camera_finalize;
  object_class->get_property = cheese_camera_get_property;
  object_class->set_property = cheese_camera_set_property;

  /**
   * CheeseCamera::video-saved:
   * @camera: a #CheeseCamera
   *
   * Emitted when a video was saved to disk.
   */
  camera_signals[VIDEO_SAVED] = g_signal_new ("video-saved", G_OBJECT_CLASS_TYPE (klass),
                                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                              G_STRUCT_OFFSET (CheeseCameraClass, video_saved),
                                              NULL, NULL,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE, 0);

  /**
   * CheeseCamera::state-flags-changed:
   * @camera: a #CheeseCamera
   * @state: the #GstState which @camera changed to
   *
   * Emitted when the state of the @camera #GstElement changed.
   */
  camera_signals[STATE_FLAGS_CHANGED] = g_signal_new ("state-flags-changed", G_OBJECT_CLASS_TYPE (klass),
                                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                G_STRUCT_OFFSET (CheeseCameraClass, state_flags_changed),
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__INT,
                                                G_TYPE_NONE, 1, G_TYPE_INT);


  /**
   * CheeseCamera:video-texture:
   *
   * The video texture for the #CheeseCamera to render into.
   */
  properties[PROP_VIDEO_TEXTURE] = g_param_spec_pointer ("video-texture",
                                                         "Video texture",
                                                         "The video texture for the CheeseCamera to render into",
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS);

  /**
   * CheeseCamera:device:
   *
   * The device object to capture from.
   */
  properties[PROP_DEVICE] = g_param_spec_object ("device",
                                                 "Device",
                                                 "The device object to capture from",
                                                 CHEESE_TYPE_CAMERA_DEVICE,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_STATIC_STRINGS);

  /**
   * CheeseCamera:format:
   *
   * The format of the video capture device.
   */
  properties[PROP_FORMAT] = g_param_spec_boxed ("format",
                                                "Video format",
                                                "The format of the video capture device",
                                                CHEESE_TYPE_VIDEO_FORMAT,
                                                G_PARAM_READWRITE |
                                                G_PARAM_STATIC_STRINGS);

  /**
   * CheeseCamera:num-camera-devices:
   *
   * The currently number of camera devices available for being used.
   */

  properties[PROP_NUM_CAMERA_DEVICES] = g_param_spec_uint ("num-camera-devices",
                                                           "Number of camera devices",
                                                           "The currently number of camera devices available on the system",
                                                           0,
                                                           G_MAXUINT8,
                                                           0,
                                                           G_PARAM_READABLE |
                                                           G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, properties);
}

static void
cheese_camera_init (CheeseCamera *camera)
{
    CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  priv->is_recording            = FALSE;
  priv->pipeline_is_playing     = FALSE;
}

/**
 * cheese_camera_new:
 * @video_texture: an actor in which to render the video
 * @name: (allow-none): the name of the device
 * @x_resolution: the resolution width
 * @y_resolution: the resolution height
 *
 * Create a new #CheeseCamera object.
 *
 * Returns: a new #CheeseCamera
 */
CheeseCamera *
cheese_camera_new (ClutterActor *video_texture, const gchar *name,
                   gint x_resolution, gint y_resolution)
{
    CheeseCamera      *camera;
    CheeseVideoFormat format = { x_resolution, y_resolution };

    camera = g_object_new (CHEESE_TYPE_CAMERA, "video-texture", video_texture,
                           "format", &format, NULL);

    if (name)
    {
        CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

        priv->initial_name = g_strdup (name);
    }

    return camera;
}

/**
 * cheese_camera_set_device:
 * @camera: a #CheeseCamera
 * @device: the device object
 *
 * Set the active video capture device of the @camera.
 */
void
cheese_camera_set_device (CheeseCamera *camera, CheeseCameraDevice *device)
{
  g_return_if_fail (CHEESE_IS_CAMERA (camera));

  g_object_set (camera, "device", device, NULL);
}

static void
cheese_camera_size_change_cb (ClutterGstContent *content, gint width, gint height, CheeseCamera* camera)
{
  CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  clutter_actor_set_size (priv->video_texture, width, height);
}

/**
 * cheese_camera_setup:
 * @camera: a #CheeseCamera
 * @device: (allow-none): the video capture device, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Setup a video capture device.
 */
void
cheese_camera_setup (CheeseCamera *camera, CheeseCameraDevice *device, GError **error)
{
  CheeseCameraPrivate *priv;
  GError  *tmp_error = NULL;
  GstElement *video_sink;

  g_return_if_fail (error == NULL || *error == NULL);
  g_return_if_fail (CHEESE_IS_CAMERA (camera));

    priv = cheese_camera_get_instance_private (camera);

  cheese_camera_detect_camera_devices (camera);

  if (priv->num_camera_devices < 1)
  {
    g_set_error (error, CHEESE_CAMERA_ERROR, CHEESE_CAMERA_ERROR_NO_DEVICE, _("No device found"));
    return;
  }

  if (device != NULL)
  {
    cheese_camera_set_device (camera, device);
  }
  else
  {
    guint i;

    for (i = 0; i < priv->num_camera_devices; i++)
    {
      device = g_ptr_array_index (priv->camera_devices, i);

      if (g_strcmp0 (cheese_camera_device_get_name (device),
                     priv->initial_name) == 0)
      {
        cheese_camera_set_device (camera, device);
        break;
      }
    }
  }


  if ((priv->camerabin = gst_element_factory_make ("camerabin", "camerabin")) == NULL)
  {
    cheese_camera_set_error_element_not_found (error, "camerabin");
  }
  if ((priv->camera_source = gst_element_factory_make ("wrappercamerabinsrc", "camera_source")) == NULL)
  {
    cheese_camera_set_error_element_not_found (error, "wrappercamerabinsrc");
  }
  g_object_set (priv->camerabin, "camera-source", priv->camera_source, NULL);

  /* Create a clutter-gst sink and set it as camerabin sink*/

  video_sink = GST_ELEMENT (clutter_gst_video_sink_new ());
  g_object_set (G_OBJECT (priv->video_texture),
                "content", g_object_new (CLUTTER_GST_TYPE_CONTENT,
                                         "sink", video_sink,
                                         NULL),
                NULL);
  g_signal_connect (G_OBJECT (clutter_actor_get_content (priv->video_texture)),
                    "size-change", G_CALLBACK(cheese_camera_size_change_cb), camera);

  g_object_set (G_OBJECT (priv->camerabin), "viewfinder-sink", video_sink, NULL);

  /* Set flags to enable conversions*/

  cheese_camera_set_camera_source (camera);
  cheese_camera_set_video_recording (camera, &tmp_error);
  cheese_camera_create_video_filter_bin (camera, &tmp_error);

  if (tmp_error != NULL || (error != NULL && *error != NULL))
  {
    g_propagate_prefixed_error (error, tmp_error,
                                _("One or more needed GStreamer elements are missing: "));
    GST_WARNING ("%s", (*error)->message);
    return;
  }

  g_object_set (G_OBJECT (priv->camera_source), "video-source-filter", priv->video_filter_bin, NULL);

  priv->bus = gst_element_get_bus (priv->camerabin);
  gst_bus_add_signal_watch (priv->bus);

  g_signal_connect (G_OBJECT (priv->bus), "message",
                    G_CALLBACK (cheese_camera_bus_message_cb), camera);
}

/**
 * cheese_camera_get_camera_devices:
 * @camera: a #CheeseCamera
 *
 * Get the list of #CheeseCameraDevice objects, representing active video
 * capture devices on the system.
 *
 * Returns: (element-type Cheese.CameraDevice) (transfer container): an array
 * of #CheeseCameraDevice
 */
GPtrArray *
cheese_camera_get_camera_devices (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv;

  g_return_val_if_fail (CHEESE_IS_CAMERA (camera), NULL);

    priv = cheese_camera_get_instance_private (camera);

  return g_ptr_array_ref (priv->camera_devices);
}

/**
 * cheese_camera_get_video_formats:
 * @camera: a #CheeseCamera
 *
 * Gets the list of #CheeseVideoFormat supported by the selected
 * #CheeseCameraDevice on the @camera.
 *
 * Returns: (element-type Cheese.VideoFormat) (transfer container): a #GList of
 * #CheeseVideoFormat, or %NULL if there was no device selected
 */
GList *
cheese_camera_get_video_formats (CheeseCamera *camera)
{
  CheeseCameraDevice *device;

  g_return_val_if_fail (CHEESE_IS_CAMERA (camera), NULL);

  device = cheese_camera_get_selected_device (camera);

  if (device)
    return cheese_camera_device_get_format_list (device);
  else
    return NULL;
}

/*
 * cheese_camera_is_playing:
 * @camera: a #CheeseCamera
 *
 * Get whether the @camera is in the playing state.
 *
 * Returns: %TRUE if the #CheeseCamera is in the playing state, %FALSE
 * otherwise
 */
static gboolean
cheese_camera_is_playing (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);

  return priv->pipeline_is_playing;
}

/**
 * cheese_camera_set_video_format:
 * @camera: a #CheeseCamera
 * @format: a #CheeseVideoFormat
 *
 * Sets a #CheeseVideoFormat on a #CheeseCamera, restarting the video stream if
 * necessary.
 */
void
cheese_camera_set_video_format (CheeseCamera *camera, CheeseVideoFormat *format)
{
  CheeseCameraPrivate *priv;

  g_return_if_fail (CHEESE_IS_CAMERA (camera) || format != NULL);

    priv = cheese_camera_get_instance_private (camera);

  if (!(priv->current_format->width == format->width &&
        priv->current_format->height == format->height))
  {
    g_object_set (G_OBJECT (camera), "format", format, NULL);
    if (cheese_camera_is_playing (camera))
    {
      cheese_camera_stop (camera);
      cheese_camera_play (camera);
    }
  }
}

/**
 * cheese_camera_get_current_video_format:
 * @camera: a #CheeseCamera
 *
 * Get the #CheeseVideoFormat that is currently set on the @camera.
 *
 * Returns: (transfer none): the #CheeseVideoFormat set on the #CheeseCamera
 */
const CheeseVideoFormat *
cheese_camera_get_current_video_format (CheeseCamera *camera)
{
  CheeseCameraPrivate *priv;

  g_return_val_if_fail (CHEESE_IS_CAMERA (camera), NULL);

    priv = cheese_camera_get_instance_private (camera);

  return priv->current_format;
}

/**
 * cheese_camera_get_balance_property_range:
 * @camera: a #CheeseCamera
 * @property: name of the balance property
 * @min: (out): minimum value
 * @max: (out): maximum value
 * @def: (out): default value
 *
 * Get the minimum, maximum and default values for the requested @property of
 * the @camera.
 *
 * Returns: %TRUE if the operation was successful, %FALSE otherwise
 */
gboolean
cheese_camera_get_balance_property_range (CheeseCamera *camera,
                                          const gchar *property,
                                          gdouble *min, gdouble *max, gdouble *def)
{
  CheeseCameraPrivate *priv;
  GParamSpec          *pspec;

  g_return_val_if_fail (CHEESE_IS_CAMERA (camera), FALSE);

    priv = cheese_camera_get_instance_private (camera);

  *min = 0.0;
  *max = 1.0;
  *def = 0.5;

  if (!GST_IS_ELEMENT (priv->video_balance)) return FALSE;

  pspec = g_object_class_find_property (
    G_OBJECT_GET_CLASS (G_OBJECT (priv->video_balance)), property);

  g_return_val_if_fail (G_IS_PARAM_SPEC_DOUBLE (pspec), FALSE);

  *min = G_PARAM_SPEC_DOUBLE (pspec)->minimum;
  *max = G_PARAM_SPEC_DOUBLE (pspec)->maximum;
  *def = G_PARAM_SPEC_DOUBLE (pspec)->default_value;

  return TRUE;
}

/**
 * cheese_camera_set_balance_property:
 * @camera: A #CheeseCamera
 * @property: name of the balance property
 * @value: value to be set
 *
 * Set the requested @property on the @camera to @value.
 */
void
cheese_camera_set_balance_property (CheeseCamera *camera, const gchar *property, gdouble value)
{
  CheeseCameraPrivate *priv;

  g_return_if_fail (CHEESE_IS_CAMERA (camera));

    priv = cheese_camera_get_instance_private (camera);

  g_object_set (G_OBJECT (priv->video_balance), property, value, NULL);
}

/**
 * cheese_camera_get_recorded_time:
 * @camera: A #CheeseCamera
 *
 * Get a string representation of the playing time
 * of the current video recording
 *
 * Returns: A string with the time representation.
 */
gchar *
cheese_camera_get_recorded_time (CheeseCamera *camera)
{
    CheeseCameraPrivate *priv = cheese_camera_get_instance_private (camera);
  GstFormat format = GST_FORMAT_TIME;
  gint64 curtime;
  GstElement *videosink;
  const gint TUNIT_60 = 60;
  gint total_time;
  gint hours;
  gint minutes;
  gint seconds;
  gboolean ret = FALSE;

  g_return_val_if_fail (CHEESE_IS_CAMERA (camera), NULL);

  videosink = gst_bin_get_by_name (GST_BIN_CAST (priv->camerabin), "videobin-filesink");
  if (videosink) {
    ret = gst_element_query_position (videosink, format, &curtime);
    gst_object_unref (videosink);
  }
  if (ret) {

    // Substract seconds, minutes and hours.
    total_time = GST_TIME_AS_SECONDS (curtime);
    seconds = total_time % TUNIT_60;
    total_time = total_time - seconds;
    minutes = (total_time % (TUNIT_60 * TUNIT_60)) / TUNIT_60;
    total_time = total_time - (minutes * TUNIT_60);
    hours = total_time / (TUNIT_60 * TUNIT_60);

    /* Translators: This is a time format, like "09:05:02" for 9
     * hours, 5 minutes, and 2 seconds. You may change ":" to
     * the separator that your locale uses or use "%Id" instead
     * of "%d" if your locale uses localized digits.
     */
    return g_strdup_printf (C_("time format", "%02i:%02i:%02i"),
                            hours, minutes, seconds);
  } else {
    GST_WARNING ("Failed to get time from video filesink from camerabin");
    return NULL;
  }
}
