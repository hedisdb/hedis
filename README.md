# Hedis

This is a modified version of [Redis](https://github.com/antirez/redis), maintaining by [Kewang](https://github.com/kewangtw) now.

## What is Hedis?

![hedis1](https://cloud.githubusercontent.com/assets/795839/8270046/6ad61e2e-17fb-11e5-9ba5-87706687d9ba.png)

Traditionally, application server retrieves hot data from in-memory database(like Redis) to reduce unnecessary paths. If in-memory database doesn't have specific data, application server gets back to retrieve data from original database.

![hedis2](https://cloud.githubusercontent.com/assets/795839/8270047/6af78870-17fb-11e5-8ad1-f077fb80de1d.png)

Hedis can retrieve data from **ANY** database directly. Application server retrieves hot data from Hedis server, If Hedis server doesn't have specific data, Hedis launch connector to retrieve data from original database.

## Hedis configuration file

```yaml
cdh1: # connector name (required)
  type: hbase # connector type (required)
  zookeeper: localhost:2181 # other configuration (depends on its type)
mysqltest:
  type: mysql
  username: root
  password: MY_PASSWORD
  host: localhost
  database: hedistest
otherdb:
  type: gooddb
```

## How to run

```sh
redis --hedis hedis.yml
```

## Connectors

* [hedis-connector-template](https://github.com/hedisdb/hedis-connector-template)
* [hedis-connector-hbase](https://github.com/hedisdb/hedis-connector-hbase)
* [hedis-connector-mysql](https://github.com/hedisdb/hedis-connector-mysql)

## Commands

### GET

```sh
GET [connector-name]://[connector-command]
```

HBase example:

```sh
# get "kewang" rowkey at "user" table on "cdh1" connector
GET "cdh1://user:kewang"
```

MySQL example:

```sh
# Query one record from "user" table on "mysqltest" connector
GET "mysqltest://select * from user limit 1"

```

### SCAN

TODO