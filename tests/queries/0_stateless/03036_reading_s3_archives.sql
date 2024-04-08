-- SELECT * FROM s3('http://localhost:11111/test/03036_archive1.zip :: example1.csv', 'NOSIGN') ORDER BY id;
SELECT * FROM s3(s3_conn, filename='03036_archive1.zip :: example1.csv') ORDER BY id;