# influxdb-c

A header-only C write client for InfluxDB.

[![license](https://img.shields.io/badge/license-MIT-brightgreen.svg?style=flat)](https://github.com/orca-zhang/influxdb-c/blob/master/LICENSE)
[![Build Status](https://semaphoreci.com/api/v1/orca-zhang-91/influxdb-c/branches/master/shields_badge.svg)](https://semaphoreci.com/orca-zhang-91/influxdb-c)

- Supported versions:
  - InfluxDB v0.9 ~ v1.4
  - Check yourself while using other versions.

### Why use influxdb-c?

- **Exactly small**:
  - Less than 300 lines and only about 10KB.
- **Easy to use**:
  - It's designed to be used without extra studies.
- **Easy to assemble**:
  - Only a tiny header file needs to be included.
- **No dependencies**:
  - Unless std C libraries.
- **Under serious testing**:
  - Uses gtest & [mockcpp](https://github.com/ez8-co/mockcpp)

### Examples

#### Before using

- The very simple thing you should do before using is only:

    ```c
    #include "influxdb.h"
    ```

#### Write example

- You should according to the [write syntax](https://docs.influxdata.com/influxdb/v1.4/write_protocols/line_protocol_reference/) while writing series(metrics).

    ```
    measurement[,tag-key=tag-value...] field-key=field-value[,field2-key=field2-value...] [unix-nano-timestamp]
    ```


- You can rapidly start writing series by using one of the following examples:

- Client configurations:

    ```c
    influx_client_t c;
    c.host = strdup("127.0.0.1");
    c.port = 8086;
    c.db = strdup("db");
    c.usr = strdup("usr");
    c.pwd = strdup("pwd");
    ```

- Under C99, you can use:

    ```c
    influx_client_t c = {
        .host = strdup("127.0.0.1"),
        .port = 8086,
        .db = strdup("db"),
        .usr = strdup("usr"),
        .pwd = strdup("pwd")
    };
    ```

- Then send out the series by calling `post_http`:

    ```c
    post_http(&c,
        INFLUX_MEAS("foo"),
        INFLUX_TAG("k", "v"),
        INFLUX_TAG("x", "y"),
        INFLUX_F_INT("x", 10),
        INFLUX_F_FLT("y", 10.3, 2),
        INFLUX_F_FLT("z", 10.3456, 2),
        INFLUX_F_BOL("b", 10),
        INFLUX_TS(1512722735522840439),
        INFLUX_END);
    ```

  - **NOTE**:
    - 3rd parameter of `INFLUX_F_FLT()` is `precision` for floating point value.
    - `usr` and `pwd` is optional for authorization.
    - `INFLUX_END` is the delimiter for variable arguments list that **should not be ommitted**.

- The series sent is:

    ```
    foo,k=v,x=y x=10i,y=10.30,z=10.35,b=t 1512722735522840439
    ```

- You could change `post_http` to `send_udp` for UDP request. And only `host` and `port` are required for UDP operation.

    ```c
    influx_client_t c = {strdup("127.0.0.1"), 8091, NULL, NULL, NULL};

    send_udp(&c,
        INFLUX_MEAS("foo"),
        INFLUX_TAG("k", "v"),
        INFLUX_F_INT("x", 10),
        INFLUX_END);
    ```

- Bulk/batch write is also supported:

    ```c
    send_udp(&c,
        INFLUX_MEAS("foo"),  // series 1
        INFLUX_F_INT("x", 10),

        INFLUX_MEAS("foo"),  // series 2
        INFLUX_F_FLT("y", 10.3, 2),

        INFLUX_END);
    ```

- The series sent are:

    ```
    foo x=10i
    bar y=10.30
    ```

- If measurement data is sent from within a loop, but higher write performance
  is needed, one has to format each measurement separately, then at the end
  send the formatted line to the database. This example sends 10 measurements
  with a single http request:

  ```c
  influx_client_t c = {strdup("127.0.0.1"), 8091, NULL, NULL, NULL};
  char *line = NULL;
  int len = 0;
  int used = 0;

  for (int i = 0; i < 10; ++i) {
      used = format_line(&line, &len, used,
          INFLUX_MEAS("foo"),
          INFLUX_TAG("k", "v"),
          INFLUX_F_INT("x", i),
          INFLUX_END);
  }

  post_http_send_line(&c, line, used);
  ```

### TODO

- Add more test cases for send functions.
- Supports DSN initializatin for influx_client_t.
- Add query function.
- Do not need to connect every time.

### Misc

- Please feel free to use influxdb-c.
- Looking forward to your suggestions.
- If your project is using influxdb-c, you can show your project or company here by creating a issue or let me know.
