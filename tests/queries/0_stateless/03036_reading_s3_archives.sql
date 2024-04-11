-- Tags: no-fasttest
-- Tag no-fasttest: Depends on AWS

SELECT id, data, _file, _path FROM s3(s3_conn, filename='03036_archive1.zip :: example1.csv') ORDER BY id;
SELECT id, data, _file, _path FROM s3(s3_conn, filename='03036_archive2.zip :: example*.csv') ORDER BY id;
SELECT id, data, _file, _path FROM s3(s3_conn, filename='03036_archive*.zip :: example2.csv') ORDER BY id;
SELECT id, data, _file, _path FROM s3(s3_conn, filename='03036_archive*.zip :: example*') ORDER BY id;
SELECT id, data, _file, _path FROM s3(s3_conn, filename='03036_archive1.tar :: example1.csv') ORDER BY id;
SELECT id, data, _file, _path FROM s3(s3_conn, filename='03036_archive*.tar :: example4.csv') ORDER BY id;
SELECT id, data, _file, _path FROM s3(s3_conn, filename='03036_archive2.tar :: example*.csv') ORDER BY id;
SELECT id, data, _file, _path FROM s3(s3_conn, filename='03036_archive*.tar.gz :: example*.csv') ORDER BY id;
SELECT id, data, _file, _path FROM s3(s3_conn, filename='03036_archive*.tar* :: example{2..3}.csv') ORDER BY id;
