#include <gtest/gtest.h>
#include "../influx4c.h"

TEST (influx4c, _unescape_append)
{
    size_t len = 20;
    size_t used = 0;
    char* buffer = NULL;

    buffer = (char*)malloc(len);
    ASSERT_EQ(_unescape_append(&buffer, &len, &used, "test a,b\\=x", ",= "), 0);
    ASSERT_EQ(strcmp(buffer, "test\\ a\\,b\\\\=x"), 0);

    ASSERT_EQ(_unescape_append(&buffer, &len, &used, "", ""), 0);

    free(buffer);
}

char* format_line(const char* measurement, ...)
{
    va_list ap;
    char* line = NULL;
    int len = 0;

    va_start(ap, measurement);
    len = _format_line(&line, measurement, ap);
    va_end(ap);

    return len < 0 ? NULL : line;
}

TEST (influx4c, format_line)
{
    char* line = NULL;
    line = format_line("test",
        INFLUX_TAG("k", "v"),
        INFLUX_F_STR("s", "string"),
        INFLUX_F_FLT("f", 28),
        INFLUX_F_INT("i", 1048576),
        INFLUX_F_BOL("b", true),
        INFLUX_TS(1512722735522840439),
        INFLUX_END);
    ASSERT_TRUE(line != NULL);
    printf("%s\n", line);
    ASSERT_EQ(strcmp(line, "test,k=v s=\"string\",f=28,i=1048576i,b=t 1512722735522840439"), 0);

    free(line);
}

TEST (influx4c, DISABLED_send_udp)
{
    influx_client_t c;
    c.host = strdup("127.0.0.1");
    c.port = 8086;
    c.db = strdup("test");

    ASSERT_EQ(send_udp(&c, "temperature",
        INFLUX_TAG("k", "v"),
        INFLUX_F_FLT("f", 28),
        INFLUX_END), 0);

    free(c.host);
    free(c.db);
}

TEST (influx4c, DISABLED_post_http)
{
    influx_client_t c;
    c.host = strdup("127.0.0.1");
    c.port = 8086;
    c.db = strdup("test");

    ASSERT_EQ(post_http(&c, "temperature",
        INFLUX_TAG("k", "v"),
        INFLUX_F_FLT("f", 28),
        INFLUX_END), 0);

    free(c.host);
    free(c.db);
}

int main(int argc, char** argv) 
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS ();
}