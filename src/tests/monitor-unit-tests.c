/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "tests/monitor-unit-tests.h"

#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"
#include "backends/meta-monitor.h"
#include "tests/meta-monitor-manager-test.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

#define MAX_N_MODES 10
#define MAX_N_OUTPUTS 10
#define MAX_N_CRTCS 10
#define MAX_N_MONITORS 10
#define MAX_N_LOGICAL_MONITORS 10

/*
 * The following structures are used to define test cases.
 *
 * Each test case consists of a test case setup and a test case expectaction.
 * and a expected result, consisting
 * of an array of monitors, logical monitors and a screen size.
 *
 * TEST CASE SETUP:
 *
 * A test case setup consists of an array of modes, an array of outputs and an
 * array of CRTCs.
 *
 * A mode has a width and height in pixels, and a refresh rate in updates per
 * second.
 *
 * An output has an array of available modes, and a preferred mode. Modes are
 * defined as indices into the modes array of the test case setup.
 *
 * It also has CRTc and an array of possible CRTCs. Crtcs are defined as indices
 * into the CRTC array. The CRTC value -1 means no CRTC.
 *
 * It also has various meta data, such as physical dimension, tile info and
 * scale.
 *
 * A CRTC only has a current mode. A mode is defined as an index into the modes
 * array.
 *
 *
 * TEST CASE EXPECTS:
 *
 * A test case expects consists of an array of monitors, an array of logical
 * monitors, a output and crtc count, and a screen width.
 *
 * A monitor represents a physical monitor (such as an external monitor, or a
 * laptop panel etc). A monitor consists of an array of outputs, defined by
 * indices into the setup output array, and the physical dimensions.
 *
 * A logical monitor represents a region of the total screen area. It contains
 * the expected layout and a scale.
 */

typedef struct _MonitorTestCaseMode
{
  int width;
  int height;
  float refresh_rate;
} MonitorTestCaseMode;

typedef struct _MonitorTestCaseOutput
{
  int crtc;
  int modes[MAX_N_MODES];
  int n_modes;
  int preferred_mode;
  int possible_crtcs[MAX_N_CRTCS];
  int n_possible_crtcs;
  int width_mm;
  int height_mm;
  MetaTileInfo tile_info;
  int scale;
} MonitorTestCaseOutput;

typedef struct _MonitorTestCaseCrtc
{
  int current_mode;
} MonitorTestCaseCrtc;

typedef struct _MonitorTestCaseSetup
{
  MonitorTestCaseMode modes[MAX_N_MODES];
  int n_modes;

  MonitorTestCaseOutput outputs[MAX_N_OUTPUTS];
  int n_outputs;

  MonitorTestCaseCrtc crtcs[MAX_N_CRTCS];
  int n_crtcs;
} MonitorTestCaseSetup;

typedef struct _MonitorTestCaseMonitor
{
  long outputs[MAX_N_OUTPUTS];
  int n_outputs;
  int width_mm;
  int height_mm;
} MonitorTestCaseMonitor;

typedef struct _MonitorTestCaseLogicalMonitor
{
  MetaRectangle layout;
  int scale;
} MonitorTestCaseLogicalMonitor;

typedef struct _MonitorTestCaseExpect
{
  MonitorTestCaseMonitor monitors[MAX_N_MONITORS];
  int n_monitors;
  MonitorTestCaseLogicalMonitor logical_monitors[MAX_N_LOGICAL_MONITORS];
  int n_logical_monitors;
  int n_outputs;
  int n_crtcs;
  int n_tiled_monitors;
  int screen_width;
  int screen_height;
} MonitorTestCaseExpect;

typedef struct _MonitorTestCase
{
  MonitorTestCaseSetup setup;
  MonitorTestCaseExpect expect;
} MonitorTestCase;

static MonitorTestCase initial_test_case = {
  .setup = {
    .modes = {
      {
        .width = 1024,
        .height = 768,
        .refresh_rate = 60.0
      }
    },
    .n_modes = 1,
    .outputs = {
       {
        .crtc = 0,
        .modes = { 0 },
        .n_modes = 1,
        .preferred_mode = 0,
        .possible_crtcs = { 0 },
        .n_possible_crtcs = 1,
        .width_mm = 222,
        .height_mm = 125
      },
      {
        .crtc = 1,
        .modes = { 0 },
        .n_modes = 1,
        .preferred_mode = 0,
        .possible_crtcs = { 1 },
        .n_possible_crtcs = 1,
        .width_mm = 220,
        .height_mm = 124
      }
    },
    .n_outputs = 2,
    .crtcs = {
      {
        .current_mode = 0
      },
      {
        .current_mode = 0
      }
    },
    .n_crtcs = 2
  },

  .expect = {
    .monitors = {
      {
        .outputs = { 0 },
        .n_outputs = 1,
        .width_mm = 222,
        .height_mm = 125
      },
      {
        .outputs = { 1 },
        .n_outputs = 1,
        .width_mm = 220,
        .height_mm = 124
      }
    },
    .n_monitors = 2,
    .logical_monitors = {
      {
        .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      },
      {
        .layout = { .x = 1024, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      }
    },
    .n_logical_monitors = 2,
    .n_outputs = 2,
    .n_crtcs = 2,
    .screen_width = 1024 * 2,
    .screen_height = 768
  }
};

static MetaOutput *
output_from_winsys_id (MetaMonitorManager *monitor_manager,
                       long                winsys_id)
{
  unsigned int i;

  for (i = 0; i < monitor_manager->n_outputs; i++)
    {
      MetaOutput *output = &monitor_manager->outputs[i];

      if (output->winsys_id == winsys_id)
        return output;
    }

  return NULL;
}

static void
check_monitor_configuration (MonitorTestCase *test_case)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);
  int tiled_monitor_count;
  GList *monitors;
  GList *logical_monitors;
  int n_logical_monitors;
  GList *l;
  int i;

  g_assert (monitor_manager->screen_width == test_case->expect.screen_width);
  g_assert (monitor_manager->screen_height == test_case->expect.screen_height);
  g_assert ((int) monitor_manager->n_outputs == test_case->expect.n_outputs);
  g_assert ((int) monitor_manager->n_crtcs == test_case->expect.n_crtcs);

  tiled_monitor_count =
    meta_monitor_manager_test_get_tiled_monitor_count (monitor_manager_test);
  g_assert (tiled_monitor_count == test_case->expect.n_tiled_monitors);

  monitors = meta_monitor_manager_get_monitors (monitor_manager);
  g_assert ((int) g_list_length (monitors) == test_case->expect.n_monitors);
  for (l = monitors, i = 0; l; l = l->next, i++)
    {
      MetaMonitor *monitor = l->data;
      GList *outputs;
      GList *l_output;
      int j;
      int width_mm, height_mm;

      outputs = meta_monitor_get_outputs (monitor);

      g_assert ((int) g_list_length (outputs) ==
                test_case->expect.monitors[i].n_outputs);

      for (l_output = outputs, j = 0; l_output; l_output = l_output->next, j++)
        {
          MetaOutput *output = l_output->data;
          long winsys_id = test_case->expect.monitors[i].outputs[j];

          g_assert (output == output_from_winsys_id (monitor_manager,
                                                     winsys_id));
        }

      meta_monitor_get_physical_dimensions (monitor, &width_mm, &height_mm);
      g_assert (width_mm == test_case->expect.monitors[i].width_mm);
      g_assert (height_mm == test_case->expect.monitors[i].height_mm);
    }

  n_logical_monitors =
    meta_monitor_manager_get_num_logical_monitors (monitor_manager);
  g_assert (n_logical_monitors == test_case->expect.n_logical_monitors);

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);
  for (l = logical_monitors, i = 0; l; l = l->next, i++)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      MonitorTestCaseLogicalMonitor *test_logical_monitor =
        &test_case->expect.logical_monitors[i];
      int j;

      g_assert (logical_monitor->rect.x == test_logical_monitor->layout.x);
      g_assert (logical_monitor->rect.y == test_logical_monitor->layout.y);
      g_assert (logical_monitor->rect.width ==
                test_logical_monitor->layout.width);
      g_assert (logical_monitor->rect.height ==
                test_logical_monitor->layout.height);
      g_assert (logical_monitor->scale == test_logical_monitor->scale);

      for (j = 0; j < logical_monitor->n_outputs; j++)
        {
          MetaOutput *output = logical_monitor->outputs[j];

          g_assert (output->crtc->logical_monitor == logical_monitor);
        }
    }
  g_assert (n_logical_monitors == i);
}

static MetaMonitorTestSetup *
create_monitor_test_setup (MonitorTestCase *test_case)
{
  MetaMonitorTestSetup *test_setup;
  int i;

  test_setup = g_new0 (MetaMonitorTestSetup, 1);

  test_setup->n_modes = test_case->setup.n_modes;
  test_setup->modes = g_new0 (MetaCrtcMode, test_setup->n_modes);
  for (i = 0; i < test_setup->n_modes; i++)
    {
      test_setup->modes[i] = (MetaCrtcMode) {
        .mode_id = i,
        .width = test_case->setup.modes[i].width,
        .height = test_case->setup.modes[i].height,
        .refresh_rate = test_case->setup.modes[i].refresh_rate
      };
    }

  test_setup->n_crtcs = test_case->setup.n_crtcs;
  test_setup->crtcs = g_new0 (MetaCrtc, test_setup->n_crtcs);
  for (i = 0; i < test_setup->n_crtcs; i++)
    {
      int current_mode_index;
      MetaCrtcMode *current_mode;

      current_mode_index = test_case->setup.crtcs[i].current_mode;
      if (current_mode_index == -1)
        current_mode = NULL;
      else
        current_mode = &test_setup->modes[current_mode_index];

      test_setup->crtcs[i] = (MetaCrtc) {
        .crtc_id = i + 1,
        .current_mode = current_mode,
        .transform = META_MONITOR_TRANSFORM_NORMAL,
        .all_transforms = ALL_TRANSFORMS
      };
    }

  test_setup->n_outputs = test_case->setup.n_outputs;
  test_setup->outputs = g_new0 (MetaOutput, test_setup->n_outputs);
  for (i = 0; i < test_setup->n_outputs; i++)
    {
      int crtc_index;
      MetaCrtc *crtc;
      int preferred_mode_index;
      MetaCrtcMode *preferred_mode;
      MetaCrtcMode **modes;
      int n_modes;
      int j;
      MetaCrtc **possible_crtcs;
      int n_possible_crtcs;
      int scale;

      crtc_index = test_case->setup.outputs[i].crtc;
      if (crtc_index == -1)
        crtc = NULL;
      else
        crtc = &test_setup->crtcs[crtc_index];

      preferred_mode_index = test_case->setup.outputs[i].preferred_mode;
      if (preferred_mode_index == -1)
        preferred_mode = NULL;
      else
        preferred_mode = &test_setup->modes[preferred_mode_index];

      n_modes = test_case->setup.outputs[i].n_modes;
      modes = g_new0 (MetaCrtcMode *, n_modes);
      for (j = 0; j < n_modes; j++)
        {
          int mode_index;

          mode_index = test_case->setup.outputs[i].modes[j];
          modes[j] = &test_setup->modes[mode_index];
        }

      n_possible_crtcs = test_case->setup.outputs[i].n_possible_crtcs;
      possible_crtcs = g_new0 (MetaCrtc *, n_possible_crtcs);
      for (j = 0; j < n_possible_crtcs; j++)
        {
          int possible_crtc_index;

          possible_crtc_index = test_case->setup.outputs[i].possible_crtcs[j];
          possible_crtcs[j] = &test_setup->crtcs[possible_crtc_index];
        }

      scale = test_case->setup.outputs[i].scale;
      if (scale < 1)
        scale = 1;

      test_setup->outputs[i] = (MetaOutput) {
        .crtc = crtc,
        .winsys_id = i,
        .name = g_strdup_printf ("LVDS%d", i + 1),
        .vendor = g_strdup ("MetaProducts Inc."),
        .product = g_strdup ("unknown"),
        .serial = g_strdup ("0xC0FFEE"),
        .suggested_x = -1,
        .suggested_y = -1,
        .hotplug_mode_update = TRUE, /* Results in config being ignored */
        .width_mm = test_case->setup.outputs[i].width_mm,
        .height_mm = test_case->setup.outputs[i].height_mm,
        .subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN,
        .preferred_mode = preferred_mode,
        .n_modes = n_modes,
        .modes = modes,
        .n_possible_crtcs = n_possible_crtcs,
        .possible_crtcs = possible_crtcs,
        .n_possible_clones = 0,
        .possible_clones = NULL,
        .backlight = -1,
        .connector_type = META_CONNECTOR_TYPE_LVDS,
        .tile_info = test_case->setup.outputs[i].tile_info,
        .scale = scale
      };
    }

  return test_setup;
}

static void
meta_test_monitor_initial_linear_config (void)
{
  check_monitor_configuration (&initial_test_case);
}

static void
emulate_hotplug (MetaMonitorTestSetup *test_setup)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorManagerTest *monitor_manager_test =
    META_MONITOR_MANAGER_TEST (monitor_manager);

  meta_monitor_manager_test_emulate_hotplug (monitor_manager_test, test_setup);
}

static void
meta_test_monitor_one_disconnected_linear_config (void)
{
  MonitorTestCase test_case = initial_test_case;
  MetaMonitorTestSetup *test_setup;

  test_case.setup.n_outputs = 1;

  test_case.expect = (MonitorTestCaseExpect) {
    .monitors = {
      {
        .outputs = { 0 },
        .n_outputs = 1,
        .width_mm = 222,
        .height_mm = 125
      }
    },
    .n_monitors = 1,
    .logical_monitors = {
      {
        .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      },
    },
    .n_logical_monitors = 1,
    .n_outputs = 1,
    .n_crtcs = 2,
    .screen_width = 1024,
    .screen_height = 768
  };

  test_setup = create_monitor_test_setup (&test_case);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_one_off_linear_config (void)
{
  MonitorTestCase test_case;
  MetaMonitorTestSetup *test_setup;
  MonitorTestCaseOutput outputs[] = {
    {
      .crtc = 0,
      .modes = { 0 },
      .n_modes = 1,
      .preferred_mode = 0,
      .possible_crtcs = { 0 },
      .n_possible_crtcs = 1,
      .width_mm = 222,
      .height_mm = 125
    },
    {
      .crtc = -1,
      .modes = { 0 },
      .n_modes = 1,
      .preferred_mode = 0,
      .possible_crtcs = { 1 },
      .n_possible_crtcs = 1,
      .width_mm = 224,
      .height_mm = 126
    }
  };

  test_case = initial_test_case;

  memcpy (&test_case.setup.outputs, &outputs, sizeof (outputs));
  test_case.setup.n_outputs = G_N_ELEMENTS (outputs);

  test_case.setup.crtcs[1].current_mode = -1;

  test_case.expect = (MonitorTestCaseExpect) {
    .monitors = {
      {
        .outputs = { 0 },
        .n_outputs = 1,
        .width_mm = 222,
        .height_mm = 125
      },
      {
        .outputs = { 1 },
        .n_outputs = 1,
        .width_mm = 224,
        .height_mm = 126
      }
    },
    .n_monitors = 2,
    .logical_monitors = {
      {
        .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      },
      {
        .layout = { .x = 1024, .y = 0, .width = 1024, .height = 768 },
        .scale = 1
      },
    },
    .n_logical_monitors = 2,
    .n_outputs = 2,
    .n_crtcs = 2,
    .screen_width = 1024 * 2,
    .screen_height = 768
  };

  test_setup = create_monitor_test_setup (&test_case);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_preferred_linear_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 800,
          .height = 600,
          .refresh_rate = 60.0
        },
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.0
        },
        {
          .width = 1280,
          .height = 720,
          .refresh_rate = 60.0
        }
      },
      .n_modes = 3,
      .outputs = {
        {
          .crtc = -1,
          .modes = { 0, 1, 2 },
          .n_modes = 3,
          .preferred_mode = 1,
          .possible_crtcs = { 0 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125
        }
      },
      .n_outputs = 1,
      .crtcs = {
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 1
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .width_mm = 222,
          .height_mm = 125
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .layout = { .x = 0, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        },
      },
      .n_logical_monitors = 1,
      .n_outputs = 1,
      .n_crtcs = 1,
      .screen_width = 1024,
      .screen_height = 768,
    }
  };
  MetaMonitorTestSetup *test_setup;

  test_setup = create_monitor_test_setup (&test_case);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_tiled_linear_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 400,
          .height = 600,
          .refresh_rate = 60.0
        },
      },
      .n_modes = 1,
      .outputs = {
        {
          .crtc = -1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 0 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 0,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        },
        {
          .crtc = -1,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
          .tile_info = {
            .group_id = 1,
            .max_h_tiles = 2,
            .max_v_tiles = 1,
            .loc_h_tile = 1,
            .loc_v_tile = 0,
            .tile_w = 400,
            .tile_h = 600
          }
        }
      },
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = -1
        },
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 2
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0, 1 },
          .n_outputs = 2,
          .width_mm = 222,
          .height_mm = 125,
        }
      },
      .n_monitors = 1,
      .logical_monitors = {
        {
          .layout = { .x = 0, .y = 0, .width = 800, .height = 600 },
          .scale = 1
        },
      },
      .n_logical_monitors = 1,
      .n_outputs = 2,
      .n_crtcs = 2,
      .n_tiled_monitors = 1,
      .screen_width = 800,
      .screen_height = 600,
    }
  };
  MetaMonitorTestSetup *test_setup;

  test_setup = create_monitor_test_setup (&test_case);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

static void
meta_test_monitor_hidpi_linear_config (void)
{
  MonitorTestCase test_case = {
    .setup = {
      .modes = {
        {
          .width = 1280,
          .height = 720,
          .refresh_rate = 60.0
        },
        {
          .width = 1024,
          .height = 768,
          .refresh_rate = 60.0
        }
      },
      .n_modes = 2,
      .outputs = {
        {
          .crtc = 0,
          .modes = { 0 },
          .n_modes = 1,
          .preferred_mode = 0,
          .possible_crtcs = { 0 },
          .n_possible_crtcs = 1,
          /* These will result in DPI of about 216" */
          .width_mm = 150,
          .height_mm = 85,
          .scale = 2,
        },
        {
          .crtc = 1,
          .modes = { 1 },
          .n_modes = 1,
          .preferred_mode = 1,
          .possible_crtcs = { 1 },
          .n_possible_crtcs = 1,
          .width_mm = 222,
          .height_mm = 125,
          .scale = 1,
        }
      },
      .n_outputs = 2,
      .crtcs = {
        {
          .current_mode = -1
        },
        {
          .current_mode = -1
        }
      },
      .n_crtcs = 2
    },

    .expect = {
      .monitors = {
        {
          .outputs = { 0 },
          .n_outputs = 1,
          .width_mm = 150,
          .height_mm = 85
        },
        {
          .outputs = { 1 },
          .n_outputs = 1,
          .width_mm = 222,
          .height_mm = 125
        }
      },
      .n_monitors = 2,
      .logical_monitors = {
        {
          .layout = { .x = 0, .y = 0, .width = 1280, .height = 720 },
          .scale = 2
        },
        {
          .layout = { .x = 1280, .y = 0, .width = 1024, .height = 768 },
          .scale = 1
        }
      },
      .n_logical_monitors = 2,
      .n_outputs = 2,
      .n_crtcs = 2,
      .screen_width = 1280 + 1024,
      .screen_height = 768
    }
  };
  MetaMonitorTestSetup *test_setup;

  test_setup = create_monitor_test_setup (&test_case);
  emulate_hotplug (test_setup);
  check_monitor_configuration (&test_case);
}

void
init_monitor_tests (void)
{
  MetaMonitorTestSetup *initial_test_setup;

  initial_test_setup = create_monitor_test_setup (&initial_test_case);
  meta_monitor_manager_test_init_test_setup (initial_test_setup);

  g_test_add_func ("/backends/monitor/initial-linear-config",
                   meta_test_monitor_initial_linear_config);
  g_test_add_func ("/backends/monitor/one-disconnected-linear-config",
                   meta_test_monitor_one_disconnected_linear_config);
  g_test_add_func ("/backends/monitor/one-off-linear-config",
                   meta_test_monitor_one_off_linear_config);
  g_test_add_func ("/backends/monitor/preferred-linear-config",
                   meta_test_monitor_preferred_linear_config);
  g_test_add_func ("/backends/monitor/tiled-linear-config",
                   meta_test_monitor_tiled_linear_config);
  g_test_add_func ("/backends/monitor/hidpi-linear-config",
                   meta_test_monitor_hidpi_linear_config);
}