/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "oc_api.h"
#include "port/oc_clock.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>

static pthread_mutex_t mutex;
static pthread_cond_t cv;
static struct timespec ts;
static int quit = 0;
static bool light_state = false;
static int counter;

static void
set_device_custom_property(void *data)
{
  oc_set_custom_device_property(purpose, "desk lamp");
}

static void
app_init(void)
{
  oc_init_platform("Intel", NULL, NULL);

  oc_add_device("/oic/d", "oic.d.light", "Kishen's light", "1.0", "1.0",
                set_device_custom_property, NULL);
}

static void
get_count(oc_request_t *request, oc_interface_mask_t interface, void *user_data)
{
  PRINT("GET_count:\n");
  oc_rep_start_root_object();
  switch (interface) {
  case OC_IF_BASELINE:
    oc_process_baseline_interface(request->resource);
  case OC_IF_R:
    oc_rep_set_int(root, count, counter);
    break;
  default:
    break;
  }
  oc_rep_end_root_object();
  oc_send_response(request, OC_STATUS_OK);
}

static void
get_light(oc_request_t *request, oc_interface_mask_t interface, void *user_data)
{
  PRINT("GET_light:\n");
  oc_rep_start_root_object();
  switch (interface) {
  case OC_IF_BASELINE:
    oc_process_baseline_interface(request->resource);
  case OC_IF_RW:
    oc_rep_set_boolean(root, state, light_state);
    break;
  default:
    break;
  }
  oc_rep_end_root_object();
  oc_send_response(request, OC_STATUS_OK);
  PRINT("Light state %d\n", light_state);
}

static void
post_light(oc_request_t *request, oc_interface_mask_t interface,
           void *user_data)
{
  PRINT("POST_light:\n");
  oc_rep_t *rep = request->request_payload;
  while (rep != NULL) {
    PRINT("key: %s ", oc_string(rep->name));
    switch (rep->type) {
    case BOOL:
      light_state = rep->value_boolean;
      PRINT("value: %d\n", light_state);
      break;
    default:
      break;
    }
    rep = rep->next;
  }

  oc_rep_start_root_object();
  oc_rep_set_boolean(root, state, light_state);
  oc_rep_end_root_object();

  oc_send_response(request, OC_STATUS_CHANGED);
}

static void
put_light(oc_request_t *request, oc_interface_mask_t interface, void *user_data)
{
  post_light(request, interface, user_data);
}

static void
register_resources(void)
{
  oc_resource_t *res1 = oc_new_resource("/light/1", 1, 0);
  oc_resource_bind_resource_type(res1, "oic.r.light");
  oc_resource_bind_resource_interface(res1, OC_IF_RW);
  oc_resource_set_default_interface(res1, OC_IF_RW);

#ifdef OC_SECURITY
  oc_resource_make_secure(res1);
#endif

  oc_resource_set_discoverable(res1, true);
  oc_resource_set_periodic_observable(res1, 1);
  oc_resource_set_request_handler(res1, OC_GET, get_light, NULL);
  oc_resource_set_request_handler(res1, OC_POST, post_light, NULL);
  oc_resource_set_request_handler(res1, OC_PUT, put_light, NULL);
  oc_add_resource(res1);

  oc_resource_t *res2 = oc_new_resource("/count/1", 1, 0);
  oc_resource_bind_resource_type(res2, "oic.r.counter");
  oc_resource_bind_resource_interface(res2, OC_IF_R);
  oc_resource_set_default_interface(res2, OC_IF_R);

#ifdef OC_SECURITY
  oc_resource_make_secure(res2);
#endif

  oc_resource_set_discoverable(res2, true);
  oc_resource_set_periodic_observable(res2, 1);
  oc_resource_set_request_handler(res2, OC_GET, get_count, NULL);
  oc_add_resource(res2);

#if defined(OC_COLLECTIONS)
  oc_resource_t *col = oc_new_collection("/lights", 1, 0);
  oc_resource_bind_resource_type(col, "oic.wk.col");
  oc_resource_set_discoverable(col, true);

#ifdef OC_SECURITY
  oc_resource_make_secure(col);
#endif

  oc_link_t *l1 = oc_new_link("/light/1", 1, 0);
  oc_link_set_if(l1, OC_IF_BASELINE | OC_IF_RW);
  oc_link_add_rt(l1, "oic.r.light");
  oc_link_set_bp(l1, "if=oic.if.baseline");
  oc_collection_add_link(col, l1);

  oc_link_t *l2 = oc_new_link("/count/1", 1, 0);
  oc_link_set_if(l2, OC_IF_BASELINE | OC_IF_R);
  oc_link_add_rt(l2, "oic.r.counter");
  oc_collection_add_link(col, l2);
  oc_add_collection(col);
#endif /* OC_COLLECTIONS */
}

static void
signal_event_loop(void)
{
  pthread_mutex_lock(&mutex);
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mutex);
}

static void
handle_signal(int signal)
{
  signal_event_loop();
  quit = 1;
}

int
main(void)
{
  int init;
  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = handle_signal;
  sigaction(SIGINT, &sa, NULL);

  static const oc_handler_t handler = {.init = app_init,
                                       .signal_event_loop = signal_event_loop,
                                       .register_resources =
                                         register_resources };

  oc_clock_time_t next_event;

#ifdef OC_SECURITY
  oc_storage_config("./creds");
#endif /* OC_SECURITY */

  init = oc_main_init(&handler);
  if (init < 0)
    return init;

  while (quit != 1) {
    next_event = oc_main_poll();
    pthread_mutex_lock(&mutex);
    if (next_event == 0) {
      pthread_cond_wait(&cv, &mutex);
    } else {
      ts.tv_sec = (next_event / OC_CLOCK_SECOND);
      ts.tv_nsec = (next_event % OC_CLOCK_SECOND) * 1.e09 / OC_CLOCK_SECOND;
      pthread_cond_timedwait(&cv, &mutex, &ts);
    }
    pthread_mutex_unlock(&mutex);
  }

  oc_main_shutdown();
  return 0;
}
