// Copyright 2019-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define LOG_LOCAL_LEVEL ESP_LOG_NONE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/errno.h>
#include "esp_log.h"
#include "esp-nanoconsole.h"
#define TAG "nano_console"

static SLIST_HEAD(_nc_cmd_head , _nc_cmd)
s_nc_cmd = SLIST_HEAD_INITIALIZER(s_nc_cmd);

void trim(char *title_p, char *title_tp) {
    int flag = 0;

    // from left
    while (*title_p) {
        if (!isspace((unsigned char ) *title_p) && flag == 0) {
            *title_tp++ = *title_p;
            flag = 1;
        }
        title_p++;
        if (flag == 1) {
            *title_tp++ = *title_p;
        }
    }

    // from right
    while (1) {
        title_tp--;
        if (!isspace((unsigned char ) *title_tp) && flag == 0) {
            break;
        }
        flag = 0;
        *title_tp = '\0';
    }
}

static inline nc_cmd_t *get_cmd(const char *buf) {
    nc_cmd_t *it;

    SLIST_FOREACH(it, &s_nc_cmd, entries)
    {
        if (!strcmp(it->name, buf)) {
            ESP_LOGI(TAG, "%s finds command %s\n", __func__, buf);
            return it;
        }
    }

    ESP_LOGI(TAG, "%s has not found command %s", __func__, buf);

    return NULL;
}

static void free_args(uintptr_t *argv) {
    int num = (int) argv[0];

    for (int i = 1; i < num; i++) {
        if (argv[i])
            free((void *) argv[i]);
        else {
            //ESP_LOGI(TAG, "%s checks command input arguments number %d is empty", __func__, i);
        }
    }

    free(argv);
}

static uintptr_t *alloc_args(const char *buf, int num) {
    const char *p = buf;
     uintptr_t *argv_buf;
           int cnt = 0;

    for (int i = 0; i < num; i++) {
        if (p[i] == '\0')
            cnt++;
    }

    argv_buf = calloc(1, (cnt + 1) * sizeof(uintptr_t));
    if (!argv_buf) {
        ESP_LOGI(TAG, "%s allocates memory for arguments table fail", __func__);
        return NULL;
    }
    argv_buf[0] = (uintptr_t) cnt + 1;

    for (int i = 0; i < cnt; i++) {
        int len = strlen(p) + 1;
        char *s = malloc(len);
        if (!s) {
            ESP_LOGI(TAG, "%s allocates memory for arguments %d fail", __func__, i);
            goto fail;
        }
        memcpy(s, p, len);

        p += len;
        argv_buf[i + 1] = (uintptr_t) s;
    }

    return argv_buf;

    fail: free_args(argv_buf);
    return NULL;
}

void nc_execute(char *buf, uint8_t data_source) {
    int n = strlen(buf)+1;
    int cnt;
    nc_cmd_t *cmd;

    char *pbuf = malloc(n);
    trim(buf, pbuf);
    n = strlen(pbuf)+1;
    for (cnt = 0;cnt < n; cnt++) {
        if (pbuf[cnt] == ' ')
            pbuf[cnt] = '\0';
    }

    cmd = get_cmd(pbuf);
    if (cmd) {
        uintptr_t *argv;
        argv = alloc_args(pbuf, n);
        if (!argv) {
            //ESP_LOGI(TAG, "%s gets command %s argument fail", __func__, cmd->name);
            free(pbuf);
            return;
        }

        cmd->func((int) argv[0] - 1, (char **) (&argv[1]), data_source);
        free_args(argv);
    }
    free(pbuf);
}

/**
 * @brief Initialize nano console
 */
int nc_init(void) {
    return 0;
}

/**
 * @brief Register a command to nano console core
 */
int nc_register_cmd(nc_cmd_handle_t *handle, const char *name, nc_func_t func) {
          int len;
      va_list ap;
    nc_cmd_t* cmd;

    cmd = malloc(sizeof(nc_cmd_t));
    if (!cmd) {
        ESP_LOGI(TAG, "%s alloc memory %d fail", __func__, __LINE__);
        return -ENOMEM;
    }

    len = strlen(name) + 1;
    cmd->name = malloc(len);
    if (!cmd->name) {
        ESP_LOGI(TAG, "%s alloc memory %d fail", __func__, __LINE__);
        free(cmd);
        return -ENOMEM;
    }

    memcpy((char *) cmd->name, name, len);
    cmd->func = func;

    SLIST_INSERT_HEAD(&s_nc_cmd, cmd, entries);

    *handle = cmd;

    va_end(ap);

    return 0;
}

/**
 * @brief Output formated string through nano console I/O stream
 */
int nc_printf(const char *fmt, ...) {
    va_list ap;
      char* pbuf;

    va_start(ap, fmt);

    int ret = vasprintf(&pbuf, fmt, ap);
    if (ret < 0)
        goto fail;

    ret = fwrite(pbuf, 1, ret, stdout);

    free(pbuf);

    va_end(ap);

    return ret;

    fail:
    va_end(ap);
    return -ENOMEM;
}
