#include <gtest/gtest.h>
#include <emock/emock.hpp>
#include <iostream>
#include <list>
using namespace std;
#include "../influxdb.h"

TEST (influxdb_c, _escaped_append)
{
    size_t len = 20;
    size_t used = 0;
    char* buffer = NULL;

    buffer = (char*)malloc(len);
    ASSERT_EQ(_escaped_append(&buffer, &len, &used, "test a,b\\=x", ",= "), 0);
    ASSERT_EQ(strncmp(buffer, "test\\ a\\,b\\\\=x", used), 0);

    ASSERT_EQ(_escaped_append(&buffer, &len, &used, "", ""), 0);

    // MEAS escape
    used = 0;
    ASSERT_EQ(_escaped_append(&buffer, &len, &used, "t=\"e\\s,t ", ", "), 0);
    ASSERT_EQ(strncmp(buffer, "t=\"e\\s\\,t\\ ", used), 0);

    // KEY/TAG_VALUE escape
    used = 0;
    ASSERT_EQ(_escaped_append(&buffer, &len, &used, "t=\"e\\s,t ", ",= "), 0);
    ASSERT_EQ(strncmp(buffer, "t\\=\"e\\s\\,t\\ ", used), 0);

    // FIELD_STRING escape
    used = 0;
    ASSERT_EQ(_escaped_append(&buffer, &len, &used, "t=\"e\\s,t ", "\""), 0);
    ASSERT_EQ(strncmp(buffer, "t=\\\"e\\s,t ", used), 0);

    free(buffer);
}

int format_line(char** line, ...)
{
    va_list ap;
    int len = 0;

    va_start(ap, line);
    len = _format_line(line, ap);
    va_end(ap);

    return len;
}

TEST (influxdb_c, format_line)
{
    char* line = NULL;
    ASSERT_TRUE(format_line(&line,
            INFLUX_MEAS("foo"),
            INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v2"),
            INFLUX_F_STR("s", "string"), INFLUX_F_FLT("f", 28.39, 2),
            INFLUX_MEAS("bar"),
            INFLUX_F_INT("i", 1048576), INFLUX_F_BOL("b", 1),
            INFLUX_TS(1512722735522840439),
        INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v2 s=\"string\",f=28.39\nbar i=1048576i,b=t 1512722735522840439\n"), 0);
    free(line);

    /////////////////////
    //
    // INPUT CHECK CASES

    // N1: MEAS first
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 2), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28.39"), 0);
    free(line);

    // E: not MEAS at first
    ASSERT_EQ(format_line(&line, INFLUX_END - 1), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_TAG("k", "v")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_F_STR("k", "v")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_F_FLT("f", 28.39, 2)), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_F_INT("i", 1048576)), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_F_BOL("b", 1)), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_TS(1512722735522840439)), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, IF_TYPE_TIMESTAMP + 1), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_END), -1);
    ASSERT_FALSE(line);

    // N: TAG/FIELD after MEAS
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_F_FLT("f", 28.39, 1), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v f=28.4"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo s=\"string\""), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo i=1048576i"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo b=t"), 0);
    free(line);

    // E: invalid type after MEAS
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_END - 1), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_MEAS("bar")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TS(1512722735522840439)), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), IF_TYPE_TIMESTAMP + 1), -1);
    ASSERT_FALSE(line);

    // N: TAG/FIELD after TAG
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v2"), 
        INFLUX_F_FLT("f", 28.39, 1), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v2 f=28.4"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_F_STR("s", "string"), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v s=\"string\""), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v f=28"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_F_INT("i", 1048576), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v i=1048576i"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_F_BOL("b", 1), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v b=t"), 0);
    free(line);

    // E: invalid type after TAG
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_END - 1), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_MEAS("bar")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TS(1512722735522840439)), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), IF_TYPE_TIMESTAMP + 1), -1);
    ASSERT_FALSE(line);

    // N: NEW_LINE after FIELD
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), 
        INFLUX_MEAS("bar"), INFLUX_F_FLT("f", 0, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo s=\"string\"\nbar f=0"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), 
        INFLUX_MEAS("bar"), INFLUX_F_FLT("f", 0, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28\nbar f=0"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), 
        INFLUX_MEAS("bar"), INFLUX_F_FLT("f", 0, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo i=1048576i\nbar f=0"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), 
        INFLUX_MEAS("bar"), INFLUX_F_FLT("f", 0, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo b=t\nbar f=0"), 0);
    free(line);

    // N: STRING FIELD after FIELD
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), INFLUX_F_STR("s1", "string"), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo s=\"string\",s1=\"string\""), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_F_STR("s1", "string"), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28,s1=\"string\""), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), INFLUX_F_STR("s1", "string"), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo i=1048576i,s1=\"string\""), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), INFLUX_F_STR("s1", "string"), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo b=t,s1=\"string\""), 0);
    free(line);

    // N: FLOAT FIELD after FIELD
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), INFLUX_F_FLT("f2", 28.39, 3), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo s=\"string\",f2=28.390"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_F_FLT("f2", 28.39, 3), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28,f2=28.390"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), INFLUX_F_FLT("f2", 28.39, 3), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo i=1048576i,f2=28.390"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), INFLUX_F_FLT("f2", 28.39, 3), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo b=t,f2=28.390"), 0);
    free(line);

    // N: INTEGER FIELD after FIELD
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), INFLUX_F_INT("i2", 10), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo s=\"string\",i2=10i"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_F_INT("i2", 10), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28,i2=10i"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), INFLUX_F_INT("i2", 10), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo i=1048576i,i2=10i"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), INFLUX_F_INT("i2", 10), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo b=t,i2=10i"), 0);
    free(line);

    // N: BOOLEAN FIELD after FIELD
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), INFLUX_F_BOL("b2", 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo s=\"string\",b2=f"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_F_BOL("b2", 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28,b2=f"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), INFLUX_F_BOL("b2", 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo i=1048576i,b2=f"), 0);
    free(line);
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), INFLUX_F_BOL("b2", 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo b=t,b2=f"), 0);
    free(line);

    // E: invalid type after STRING FIELD
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), INFLUX_END - 1), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), INFLUX_MEAS("bar")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), INFLUX_TAG("k", "v")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), IF_TYPE_TIMESTAMP + 1), -1);
    ASSERT_FALSE(line);

    // E: invalid type after FLOAT FIELD
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END - 1), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_MEAS("bar")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_TAG("k", "v")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), IF_TYPE_TIMESTAMP + 1), -1);
    ASSERT_FALSE(line);

    // E: invalid type after INTEGER FIELD
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), INFLUX_END - 1), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), INFLUX_MEAS("bar")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), INFLUX_TAG("k", "v")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_INT("i", 1048576), IF_TYPE_TIMESTAMP + 1), -1);
    ASSERT_FALSE(line);

    // E: invalid type after BOOLEAN FIELD
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), INFLUX_END - 1), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), INFLUX_MEAS("bar")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), INFLUX_TAG("k", "v")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_BOL("b", 1), IF_TYPE_TIMESTAMP + 1), -1);
    ASSERT_FALSE(line);

    // N: new line after TIMESTAMP
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), 
        INFLUX_TS(1512722735522840439), INFLUX_MEAS("bar"), INFLUX_F_FLT("f", 0, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo s=\"string\" 1512722735522840439\nbar f=0"), 0);
    free(line);

    // E: invalid type after TIMESTAMP
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), 
        INFLUX_TS(1512722735522840439), INFLUX_MEAS("bar"), INFLUX_END), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), 
        INFLUX_TS(1512722735522840439), INFLUX_TAG("k", "v")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), 
        INFLUX_TS(1512722735522840439), INFLUX_F_STR("s", "string")), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), 
        INFLUX_TS(1512722735522840439), INFLUX_F_FLT("f", 28.39, 0)), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), 
        INFLUX_TS(1512722735522840439), INFLUX_F_INT("i", 1048576)), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), 
        INFLUX_TS(1512722735522840439), INFLUX_F_BOL("b", 1)), -1);
    ASSERT_FALSE(line);
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_STR("s", "string"), 
        INFLUX_TS(1512722735522840439), IF_TYPE_TIMESTAMP + 1), -1);
    ASSERT_FALSE(line);

    // N: right syntax meas field timestamp
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 2), 
        INFLUX_TS(1512722735522840439), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28.39 1512722735522840439"), 0);
    free(line);

    // N: right syntax meas field,field2
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 2), 
        INFLUX_F_FLT("f2", 28.39, 1), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28.39,f2=28.4"), 0);
    free(line);

    // N: right syntax meas field,field2 timestamp
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 2), 
        INFLUX_F_FLT("f2", 28.39, 1), INFLUX_TS(1512722735522840439), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28.39,f2=28.4 1512722735522840439"), 0);
    free(line);

    // N: right syntax meas,tag field
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_F_FLT("f", 28.39, 2), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v f=28.39"), 0);
    free(line);

    // N: right syntax meas,tag field timestamp
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), INFLUX_TS(1512722735522840439), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v f=28.39 1512722735522840439"), 0);
    free(line);

    // N: right syntax meas,tag,tag2 field
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v f=28.39"), 0);
    free(line);

    // N: right syntax meas,tag,tag2 field timestamp
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), INFLUX_TS(1512722735522840439), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v f=28.39 1512722735522840439"), 0);
    free(line);

    // N: right syntax meas,tag,tag2 field,field2
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), INFLUX_F_FLT("f2", 28.39, 1), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v f=28.39,f2=28.4"), 0);
    free(line);

    // N: right syntax meas,tag,tag2 field,field2 timestamp
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), INFLUX_F_FLT("f2", 28.39, 1), INFLUX_TS(1512722735522840439), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v f=28.39,f2=28.4 1512722735522840439"), 0);
    free(line);

    // E: syntax error meas,tag
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_END), -1);
    ASSERT_FALSE(line);

    // E: syntax error meas timestamp
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TS(1512722735522840439), INFLUX_END), -1);
    ASSERT_FALSE(line);

    // E: syntax error meas,tag timestamp
    ASSERT_EQ(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), 
        INFLUX_TS(1512722735522840439), INFLUX_END), -1);
    ASSERT_FALSE(line);

    // N: right syntax meas field timestamp\nline
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 2), 
        INFLUX_TS(1512722735522840439), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28.39 1512722735522840439\nfoo f=28"), 0);
    free(line);

    // N: right syntax meas field,field2\nline
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 2), INFLUX_F_FLT("f2", 28.39, 1), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28.39,f2=28.4\nfoo f=28"), 0);
    free(line);

    // N: right syntax meas field,field2 timestamp\nline
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 2), 
        INFLUX_F_FLT("f2", 28.39, 1), INFLUX_TS(1512722735522840439), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28.39,f2=28.4 1512722735522840439\nfoo f=28"), 0);
    free(line);

    // N: right syntax meas,tag field\nline
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v f=28.39\nfoo f=28"), 0);
    free(line);

    // N: right syntax meas,tag field timestamp\nline
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), INFLUX_TS(1512722735522840439), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v f=28.39 1512722735522840439\nfoo f=28"), 0);
    free(line);

    // N: right syntax meas,tag,tag2 field\nline
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v f=28.39\nfoo f=28"), 0);
    free(line);

    // N: right syntax meas,tag,tag2 field timestamp\nline
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), INFLUX_TS(1512722735522840439), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v f=28.39 1512722735522840439\nfoo f=28"), 0);
    free(line);

    // N: right syntax meas,tag,tag2 field,field2\nline
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), INFLUX_F_FLT("f2", 28.39, 1), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v f=28.39,f2=28.4\nfoo f=28"), 0);
    free(line);

    // N: right syntax meas,tag,tag2 field,field2 timestamp\nline
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_TAG("k", "v"), INFLUX_TAG("k2", "v"), 
        INFLUX_F_FLT("f", 28.39, 2), INFLUX_F_FLT("f2", 28.39, 1), INFLUX_TS(1512722735522840439), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 0), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo,k=v,k2=v f=28.39,f2=28.4 1512722735522840439\nfoo f=28"), 0);
    free(line);

    // N: multi line
    ASSERT_TRUE(format_line(&line, INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.31, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.32, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.33, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.34, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.35, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.36, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.37, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.38, 2), 
        INFLUX_MEAS("foo"), INFLUX_F_FLT("f", 28.39, 2), INFLUX_END));
    ASSERT_TRUE(line != NULL);
    ASSERT_EQ(strcmp(line, "foo f=28.31\nfoo f=28.32\nfoo f=28.33\nfoo f=28.34\nfoo f=28.35\nfoo f=28.36\nfoo f=28.37\nfoo f=28.38\nfoo f=28.39"), 0);
    free(line);
}

TEST (influxdb_c, send_udp)
{
    influx_client_t c;
    c.host = strdup("127.0.0.1");
    c.port = 88888;
    c.db = strdup("test");
    c.usr = strdup("test");
    c.pwd = strdup("test");

    // case 1: _format_line < 0
    EMOCK(_format_line)
        .stubs()
        .with(any(), any())
        .will(returnValue(-1));

    ASSERT_EQ(send_udp(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -1);

    GlobalMockObject::verify();

    // case 2: inet_addr return INADDR_NONE
    EMOCK(inet_addr)
        .stubs()
        .with(any())
        .will(returnValue(INADDR_NONE));

    ASSERT_EQ(send_udp(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -2);

    GlobalMockObject::verify();

    // case 3: socket < 0
    EMOCK(socket)
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(-1));

    ASSERT_EQ(send_udp(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -3);

    GlobalMockObject::verify();

    // case 4: sendto < 0
    EMOCK(sendto)
        .stubs()
        .with(any(), any(), any(), any(), any(), any())
        .will(returnValue(-1));

    ASSERT_EQ(send_udp(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -4);

    GlobalMockObject::verify();

    // case 5: sendto < len
    int len = 0;
    EMOCK(sendto)
        .stubs()
        .with(any(), any(), spy(len), any(), any(), any())
        .will(returnValue(len - 1));

    ASSERT_EQ(send_udp(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -4);

    GlobalMockObject::verify();

    // case 6: sendto = len
    EMOCK(sendto)
        .stubs()
        .with(any(), any(), spy(len), any(), any(), any())
        .will(returnValue(len));

    ASSERT_EQ(send_udp(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), 0);

    GlobalMockObject::verify();

    free(c.host);
    free(c.db);
    free(c.usr);
    free(c.pwd);
}

int writev_mock_err(int, const struct iovec* iv, int)
{
    return (int)(iv[0].iov_len + iv[1].iov_len) - 1;
}

int writev_mock(int, const struct iovec* iv, int)
{
    return (int)(iv[0].iov_len + iv[1].iov_len);
}

list<string> g_recv_data;

int recv_mock(int, char* buffer, size_t len, int)
{
    return 0;
}

TEST (influxdb_c, post_http)
{
    influx_client_t c;
    c.host = strdup("127.0.0.1");
    c.port = 88888;
    c.db = strdup("test");
    c.usr = strdup("test");
    c.pwd = strdup("test");

    // case 1: _format_line < 0
    EMOCK(_format_line)
        .stubs()
        .with(any(), any())
        .will(returnValue(-1));

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -1);

    GlobalMockObject::verify();

    // case 2: inet_addr return INADDR_NONE
    EMOCK(inet_addr)
        .stubs()
        .with(any())
        .will(returnValue(INADDR_NONE));

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -4);

    GlobalMockObject::verify();

    // case 3: socket < 0
    EMOCK(socket)
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(-1));

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -5);

    GlobalMockObject::verify();

    // case 4: connect < 0
    EMOCK(connect)
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(-1));

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -6);

    GlobalMockObject::verify();

    // case 5: writev < 0
    EMOCK(connect)
        .stubs()
        .will(returnValue(0));
    EMOCK(writev)
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(-1));

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -7);

    GlobalMockObject::verify();

    // case 6: writev < len
    EMOCK(connect)
        .stubs()
        .will(returnValue(0));
    EMOCK(writev)
        .stubs()
        .with(any(), any(), eq(2))
        .will(invoke(writev_mock_err));

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -7);

    GlobalMockObject::verify();

    // case 7: writev = len
    EMOCK(connect)
        .stubs()
        .will(returnValue(0));
    EMOCK(writev)
        .stubs()
        .with(any(), any(), eq(2))
        .will(invoke(writev_mock));
    EMOCK(recv)
        .stubs()
        .with(any(), any(), any(), any())
        .will(returnValue(-1));

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), -8);

    GlobalMockObject::verify();

    // case 8: return status_code
    EMOCK(connect)
        .stubs()
        .will(returnValue(0));
    EMOCK(writev)
        .stubs()
        .with(any(), any(), eq(2))
        .will(invoke(writev_mock));
    EMOCK(recv)
        .stubs()
        .with(any(), any(), any(), any())
        .will(invoke(recv_mock));

    // case 9
    // return -8
    // 
    // 
    // case 10
    // return -9
    // 
    // case 11
    // return -10
    // 
    // case 12
    // return -11

    free(c.host);
    free(c.db);
    free(c.usr);
    free(c.pwd);
}

TEST (influxdb_c, DISABLED_send_udp)
{
    influx_client_t c;
    c.host = strdup("127.0.0.1");
    c.port = 8089;
    c.db = strdup("test");
    c.usr = strdup("test");
    c.pwd = strdup("test");

    ASSERT_EQ(send_udp(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_TAG("k", "v"),
        INFLUX_F_FLT("f", 28, 0),
        INFLUX_END), 0);

    free(c.host);
    free(c.db);
    free(c.usr);
    free(c.pwd);
}

TEST (influxdb_c, DISABLED_post_http)
{
    influx_client_t c;
    c.host = strdup("127.0.0.1");
    c.port = 8086;
    c.db = strdup("test");
    c.usr = strdup("test");
    c.pwd = strdup("test");

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_TAG("k", "v"),
        INFLUX_F_FLT("i", 28, 0),
        INFLUX_END), 0);

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_TAG("k", "v"),
        INFLUX_F_INT("i", 28),
        INFLUX_END), 400);

    free(c.usr);
    free(c.pwd);

    c.usr = c.pwd = NULL;

    ASSERT_EQ(post_http(&c,
        INFLUX_MEAS("temperature"),
        INFLUX_TAG("k", "v"),
        INFLUX_F_FLT("i", 28, 0),
        INFLUX_END), 401);

    free(c.host);
    free(c.db);
}

int main(int argc, char** argv) 
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS ();
}
