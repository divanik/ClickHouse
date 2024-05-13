import pytest
import time
from helpers.cluster import ClickHouseCluster
from typing import Optional
from logging import log

cluster = ClickHouseCluster(__file__)
node = cluster.add_instance(
    "node",
    with_minio=True,
    stay_alive=True,
    main_configs=[
        "configs/config.d/query_log.xml",
        "configs/config.d/schema_cache.xml",
    ],
)

@pytest.fixture(scope="module")
def start_cluster():
    try:
        cluster.start()
        yield cluster
    finally:
        cluster.shutdown()

s3_url_base = f'http://{cluster.minio_host}:{cluster.minio_port}/{cluster.minio_bucket}/'
table_generator_type = None

def check_profile_event_for_query(node, file, profile_event, amount=1):
    node.query("system flush logs")
    global table_generator_type
    assert (table_generator_type is not None)

    query_pattern = f"file('{file}'".replace("'", "\\'") if table_generator_type == 'file' else (f"s3('" + s3_url_base + f"{file}'").replace("'", "\\'")
    assert (
        int(
            node.query(
                f"select ProfileEvents['{profile_event}'] from system.query_log where query like '%{query_pattern}%' and query not like '%ProfileEvents%' and type = 'QueryFinish' order by query_start_time_microseconds desc limit 1"
            )
        )
        == amount
    )


def check_cache_misses(node, file, amount=1):
    check_profile_event_for_query(node, file, "SchemaInferenceCacheMisses", amount)


def check_cache_hits(node, file, amount=1):
    check_profile_event_for_query(node, file, "SchemaInferenceCacheHits", amount)


def check_cache_invalidations(node, file, amount=1):
    check_profile_event_for_query(
        node, file, "SchemaInferenceCacheInvalidations", amount
    )


def check_cache_evictions(node, file, amount=1):
    check_profile_event_for_query(node, file, "SchemaInferenceCacheEvictions", amount)


def check_cache_num_rows_hits(node, file, amount=1):
    check_profile_event_for_query(node, file, "SchemaInferenceCacheNumRowsHits", amount)


def check_cache(node, expected_files):
    sources = node.query("select source from system.schema_inference_cache")
    # for source in sources:
    #     print(source)
    assert sorted(map(lambda x: x.strip().split("/")[-1], sources.split())) == sorted(
        expected_files
    )

def generate_table(base_str : str, format : Optional[str] = None, schema : Optional[str] = None):
    if schema is None:
        if format is None:
            return base_str + ")"
        else:
            return base_str + f", '{format}')"
    else:
        if format is None:
            return base_str + f", 'auto', '{schema}')"
        else:
            return base_str + f", '{format}', '{schema}')"     

def file_generate_table(file : str, format : Optional[str] = None, schema : Optional[str] = None):
    return generate_table(f"file('{file}'", format, schema)

def s3_generate_table(file : str, format : Optional[str] = None, schema : Optional[str] = None):
    return generate_table(f"s3('" + s3_url_base + f"{file}', 'minio', 'minio123'", format, schema)

table_generators = [('s3', s3_generate_table)]#, ('file', file_generate_table)]

@pytest.mark.parametrize("table_generator_tuple", table_generators)
def test(start_cluster, table_generator_tuple):
    global table_generator_type
    table_generator_type = table_generator_tuple[0]
    table_generator = table_generator_tuple[1]

    allow_s3_truncate_settings = " SETTINGS  s3_truncate_on_insert=1" if (table_generator_type == 's3') else ""

    table_function_json_0 = table_generator('data.jsonl')
    # print("DEB PRINT", table_function_json_0)
    node.query(f"select * from numbers(100)")
    node.query(f"insert into function {table_function_json_0} select * from numbers(100)")
    # print("Query: ", f"insert into function {table_function_json_0} select * from numbers(100)")
    time.sleep(1)

    node.query(f"desc {table_function_json_0}")
    check_cache(node, ["data.jsonl"])
    check_cache_misses(node, "data.jsonl")

    node.query(f"desc {table_function_json_0}")
    check_cache_hits(node, "data.jsonl")

    node.query(f"insert into function {table_function_json_0} select * from numbers(100) {allow_s3_truncate_settings}")
    time.sleep(1)

    node.query(f"desc {table_function_json_0}")
    check_cache_invalidations(node, "data.jsonl")

    table_function_json_1 = table_generator("data1.jsonl")
    node.query(f"insert into function {table_function_json_1} select * from numbers(100) {allow_s3_truncate_settings}")
    time.sleep(1)

    node.query(f"desc {table_function_json_1}")
    check_cache(node, ["data.jsonl", "data1.jsonl"])
    check_cache_misses(node, "data1.jsonl")

    node.query(f"desc {table_function_json_1}")
    check_cache_hits(node, "data1.jsonl")

    table_function_json_2 = table_generator("data2.jsonl")
    node.query(f"insert into function {table_function_json_2} select * from numbers(100)")
    time.sleep(1)

    node.query(f"desc {table_function_json_2}")
    check_cache(node, ["data1.jsonl", "data2.jsonl"])
    check_cache_misses(node, "data2.jsonl")
    check_cache_evictions(node, "data2.jsonl")

    node.query(f"desc {table_function_json_2}")
    check_cache_hits(node, "data2.jsonl")

    # node.query(f"desc {table_function_json_1}")
    # check_cache_hits(node, "data1.jsonl")

    # node.query(f"desc {table_function_json_0}")
    # check_cache(node, ["data.jsonl", "data1.jsonl"])
    # check_cache_misses(node, "data.jsonl")
    # check_cache_evictions(node, "data.jsonl")

    # node.query(f"desc {table_function_json_2}")
    # check_cache(node, ["data.jsonl", "data2.jsonl"])
    # check_cache_misses(node, "data2.jsonl")
    # check_cache_evictions(node, "data2.jsonl")

    # node.query(f"desc {table_function_json_2}")
    # check_cache_hits(node, "data2.jsonl")

    # node.query(f"desc {table_function_json_0}")
    # check_cache_hits(node, "data.jsonl")

    # table_function_json_3 = table_generator("data3.jsonl")
    # node.query(f"insert into function {table_function_json_3} select * from numbers(100)")
    # time.sleep(1)

    # table_function_json_asterisk = table_generator('data*.jsonl')
    # node.query(f"desc {table_function_json_asterisk}")
    # check_cache_hits(node, "data*.jsonl")

    # node.query(f"system drop schema cache for {table_generator_type}")
    # check_cache(node, [])

    # node.query(f"desc {table_function_json_asterisk}")
    # check_cache_misses(node, "data*.jsonl", 4)

    # node.query("system drop schema cache")
    # check_cache(node, [])

    # node.query(f"desc {table_function_json_asterisk}")
    # check_cache_misses(node, "data*.jsonl", 4)

    # node.query("system drop schema cache")
    # check_cache(node, [])

    # table_function_csv_0 = table_generator('data.csv')
    # node.query(f"insert into function {table_function_csv_0} select * from numbers(100)")
    # time.sleep(1)


    # table_function_csv_with_schema_0 = table_generator('data.csv', 'auto', 'x UInt64')
    # print("DEB_PRINT2", table_function_csv_with_schema_0)
    # res = node.query(f"select count() from {table_function_csv_with_schema_0}")
    # assert int(res) == 100
    # check_cache(node, ["data.csv"])
    # check_cache_misses(node, "data.csv")

    # res = node.query(f"select count() from {table_function_csv_with_schema_0}")
    # assert int(res) == 100
    # check_cache_hits(node, "data.csv")

    # node.query(
    #     f"insert into function {table_function_csv_with_schema_0} select * from numbers(100)"
    # )
    # time.sleep(1)

    # res = node.query(f"select count() from {table_function_csv_with_schema_0}")
    # assert int(res) == 200
    # check_cache_invalidations(node, "data.csv")


    # table_function_csv_1 = table_generator('data1.csv')
    # node.query(f"insert into function {table_function_csv_1} select * from numbers(100)")
    # time.sleep(1)

    # table_function_csv_with_schema_1 = table_generator('data1.csv', 'auto', 'x UInt64')
    # res = node.query(f"select count() from {table_function_csv_with_schema_1}")
    # assert int(res) == 100
    # check_cache(node, ["data.csv", "data1.csv"])
    # check_cache_misses(node, "data1.csv")

    # res = node.query(f"select count() from {table_function_csv_with_schema_1}")
    # assert int(res) == 100
    # check_cache_hits(node, "data1.csv")

    # table_function_csv_with_schema_asterisk = table_generator('data*.csv', 'auto', 'x UInt64')

    # res = node.query(f"select count() from {table_function_csv_with_schema_asterisk}")
    # assert int(res) == 300
    # check_cache_hits(node, "data*.csv", 2)

    # node.query(f"system drop schema cache for {table_generator_type}")
    # check_cache(node, [])

    # res = node.query(f"select count() from {table_function_csv_with_schema_asterisk}")
    # assert int(res) == 300
    # check_cache_misses(node, "data*.csv", 2)

    # node.query(f"system drop schema cache for {table_generator_type}")
    # check_cache(node, [])

    # table_function_parquet = table_generator('data.parquet')

    # node.query(f"insert into function {table_function_parquet} select * from numbers(100)")
    # time.sleep(1)
    
    # res = node.query(f"select count() from {table_function_parquet}")
    # assert int(res) == 100
    # check_cache_misses(node, "data.parquet")
    # check_cache_hits(node, "data.parquet")
    # check_cache_num_rows_hits(node, "data.parquet")
