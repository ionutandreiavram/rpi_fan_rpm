/*============================================================================
All rights reserved.

Copyright (c) 2025-2026  RAlexandru Trifan,  Ionut Andrei Avram

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <glib/gi18n.h>

#ifdef LXPLUG
#include "plugin.h"
#else
#include "lxutils.h"
#endif

#include "fanrpm.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros                                                        */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Global data                                                                */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Prototypes                                                                 */
/*----------------------------------------------------------------------------*/

static gint _get_reading (const char *path);
static gint hwmon_get_rpm (char const *sensor_path);
static int add_sensor (FANRpmPlugin* c, char const* sensor_path, GetRpmFunc get_rpm);
static gboolean try_hwmon_sensors (FANRpmPlugin* c, const char *path);
static void find_hwmon_sensors (FANRpmPlugin* c);
static void check_sensors (FANRpmPlugin *c);
static gint get_rpm (FANRpmPlugin *c);
static gboolean cpu_update (FANRpmPlugin *c);
static gboolean write_config (FANRpmPlugin *c);
static void validate_rpms (FANRpmPlugin *c);

/*----------------------------------------------------------------------------*/
/* Function definitions                                                       */
/*----------------------------------------------------------------------------*/

static gint _get_reading (const char *path)
{
    FILE *state;
    char buf[256];
    char *pstr;

    if (!(state = fopen (path, "r")))
    {
        g_warning ("fanrpm: cannot open %s", path);
        return -1;
    }

    while (fgets (buf, 256, state) && !(pstr = buf));
    if (pstr)
    {
        fclose (state);
        return atoi (pstr);
    }

    fclose (state);
    return -1;
}

static gint hwmon_get_rpm (char const *sensor_path)
{
    if (sensor_path == NULL) return -1;
    return _get_reading (sensor_path);
}

static int add_sensor (FANRpmPlugin* c, char const* sensor_path, GetRpmFunc get_rpm)
{
    if (c->numsensors + 1 > MAX_NUM_SENSORS)
    {
        g_message ("FAN_RPM: Too many sensors (max %d), ignoring '%s'",
            MAX_NUM_SENSORS, sensor_path);
        return -1;
    }

    c->sensor_array[c->numsensors] = g_strdup (sensor_path);
    c->get_rpm[c->numsensors] = get_rpm;
    c->numsensors++;

    g_message ("FAN_RPM: Added sensor %s", sensor_path);

    return 0;
}

static gboolean try_hwmon_sensors (FANRpmPlugin* c, const char *path)
{
    GDir *sensorsDirectory;
    const char *sensor_name;
    char sensor_path[100], buf[256];
    FILE *fp;
    gboolean found = FALSE;

    if (!(sensorsDirectory = g_dir_open (path, 0, NULL))) return found;

    while ((sensor_name = g_dir_read_name (sensorsDirectory)))
    {
        if (strncmp (sensor_name, "fan", 3) == 0 && 
            strcmp (&sensor_name[4], "_input") == 0) 
        {
            snprintf (sensor_path, sizeof (sensor_path), "%s/fan%c_label", path, sensor_name[4]); 
            fp = fopen (sensor_path, "r");
            buf[0] = '\0';
            if (fp) //om vedea
            {
                if (fgets (buf, 256, fp))
                {
                    char *pp = strchr (buf, '\n');
                    if (pp) *pp = '\0';
                }
                fclose (fp);
            }
            snprintf (sensor_path, sizeof (sensor_path), "%s/%s", path, sensor_name);
            add_sensor (c, sensor_path, hwmon_get_rpm);
            found = TRUE;
        }
    }
    g_dir_close (sensorsDirectory);
    return found;
}

static void find_hwmon_sensors (FANRpmPlugin* c)
{
    char dir_path[100];
    int i; /* sensor type num, we'll try up to 4 */

    for (i = 0; i < 4; i++)
    {
        snprintf (dir_path, sizeof (dir_path), "/sys/class/hwmon/hwmon%d", i);
        try_hwmon_sensors (c, dir_path);
    }
}

static void check_sensors (FANRpmPlugin *c)
{
    int i;

    for (i = 0; i < c->numsensors; i++) g_free (c->sensor_array[i]);
    c->numsensors = 0;
    if (c->numsensors == 0) find_hwmon_sensors (c);
    g_message ("FAN_RPM: Found %d sensors", c->numsensors);
}

static gint get_rpm (FANRpmPlugin *c)
{
    gint min = 0, cur, i; 

    for (i = 0; i < c->numsensors; i++)
    {
        cur = c->get_rpm[i] (c->sensor_array[i]);
        if (cur > min) min = cur;
        c->rpm[i] = cur;
    }

    return min;
}

/* Periodic timer callback */

static gboolean cpu_update (FANRpmPlugin *c)
{
    char *buffer;
    int rpm, thr = 0;
    float frpm = 0;

    if (g_source_is_destroyed (g_main_current_source ())) return FALSE;

    rpm = get_rpm (c);

    buffer = g_strdup_printf ("%dr", rpm);

    validate_rpms (c);

    frpm = (float)rpm / 8000.0f; // Convert to a fraction of 8000 RPM
    graph_new_point (&(c->graph), frpm, thr, buffer);

    g_free (buffer);
    return TRUE;
}

static gboolean write_config (FANRpmPlugin *c)
{
#ifdef LXPLUG
    (void) c;
#else
    char *strval, *user_file;
    GKeyFile *kf;
    gsize len;

    user_file = g_build_filename (g_get_user_config_dir (), "wf-panel-pi.ini", NULL);
    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_integer (kf, "panel", "fanrpm_low_rpm", c->lower_rpm);
    g_key_file_set_integer (kf, "panel", "fanrpm_high_rpm", c->upper_rpm);

    strval = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_file, strval, len, NULL);

    g_free (strval);
    g_key_file_free (kf);
    g_free (user_file);
#endif

    return FALSE;
}

static void validate_rpms (FANRpmPlugin *c)
{
    int lower, upper;

    lower = c->lower_rpm;
    upper = c->upper_rpm;

    if (c->lower_rpm < 0 || c->lower_rpm > 8000) c->lower_rpm = 0;
    if (c->upper_rpm < 0 || c->upper_rpm > 8000) c->upper_rpm = 8000;
    if (c->upper_rpm <= c->lower_rpm)
    {
        c->lower_rpm = 0;
        c->upper_rpm = 8000;
    }

    if (lower != c->lower_rpm || upper != c->upper_rpm) g_idle_add ((GSourceFunc) write_config, (gpointer) c);
}

/*----------------------------------------------------------------------------*/
/* wf-panel plugin functions                                                  */
/*----------------------------------------------------------------------------*/

/* Handler for system config changed message from panel */
void fanrpm_update_display (FANRpmPlugin *c)
{
    validate_rpms (c);
    graph_reload (&(c->graph), wrap_icon_size (c), c->background_colour, c->foreground_colour, c->background_colour, c->background_colour);
}

void fanrpm_init (FANRpmPlugin *c)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    /* Allocate icon as a child of top level */
    graph_init (&(c->graph));
    gtk_container_add (GTK_CONTAINER (c->plugin), c->graph.da);

    /* Set up variables */
    c->ispi = is_pi ();
    
    /* Find the system thermal sensors */
    check_sensors (c);

    /* Constrain temperatures */
    validate_rpms (c);

    fanrpm_update_display (c);

    /* Connect a timer to refresh the statistics. */
    c->timer = g_timeout_add (1500, (GSourceFunc) cpu_update, (gpointer) c);

    /* Show the widget and return. */
    gtk_widget_show_all (c->plugin);
}

void fanrpm_destructor (gpointer user_data)
{
    FANRpmPlugin *c = (FANRpmPlugin *) user_data;

    graph_free (&(c->graph));
    if (c->timer) g_source_remove (c->timer);

    g_free (c);
}

/* End of file */
/*----------------------------------------------------------------------------*/
