# Hedis

[![Join the chat at https://gitter.im/hedisdb/hedis](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/hedisdb/hedis?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Hedis stands for "Hyper Redis". It can retrieve data from **ANY** database directly and stores to itself.

[![Hedis DEMO](http://img.youtube.com/vi/aAgJaqvgqLU/0.jpg)](http://www.youtube.com/watch?v=aAgJaqvgqLU)

## What is Hedis?

![hedis1](https://cloud.githubusercontent.com/assets/795839/8285334/3d6ee5e8-1935-11e5-809e-59bf9afbbe92.png)

Traditionally, application server retrieves hot data from in-memory database (like Redis) to reduce unnecessary paths. If in-memory database doesn't have specific data, application server gets back to retrieve data from original database.

![hedis2](https://cloud.githubusercontent.com/assets/795839/8285335/3d967ce8-1935-11e5-8b3b-0194d90b9e4d.png)

Hedis can retrieve data from **ANY** database directly. Application server retrieves hot data from Hedis server, If Hedis server doesn't have specific data, Hedis retrieves data from original database via **ANY** connector.

## Hedis configuration file

```yaml
cdh1: # connector name (REQUIRED)
  type: hbase # connector type (REQUIRED)
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

### Connectors

* [hedis-connector-template](https://github.com/hedisdb/hedis-connector-template)
* [hedis-connector-hbase](https://github.com/hedisdb/hedis-connector-hbase)
* [hedis-connector-mysql](https://github.com/hedisdb/hedis-connector-mysql)

## How to run

### First all

Sets environment variables on various connectors

```sh
# HBase environment variables

export CLASSPATH=$CLASSPATH:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/async-1.4.0.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/asynchbase-1.5.0-libhbase-20140311.193218-1.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/commons-configuration-1.6.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/commons-lang-2.5.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/commons-logging-1.1.1.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/hadoop-core-1.0.4.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/hbase-0.94.17.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/libhbase-1.0-SNAPSHOT.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/log4j-1.2.17.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/netty-3.8.0.Final.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/protobuf-java-2.5.0.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/slf4j-api-1.7.5.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/slf4j-log4j12-1.7.5.jar:/home/kewang/git/libhbase/target/libhbase-1.0-SNAPSHOT/lib/zookeeper-3.4.5.jar
export HBASE_LIB_DIR=/home/kewang/hbase/lib
export HBASE_CONF_DIR=/home/kewang/hbase/conf
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib/jvm/java-7-oracle/jre/lib/amd64/server

# MySQL environment variables

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib/x86_64-linux-gnu
```

### Run it

```sh
redis-server --hedis hedis.yml
```

## Commands

### GET

```sh
GET [CONNECTOR-NAME]://[CONNECTOR-COMMAND]
```

#### Example

```sh
# HBase example:
#
# get "kewang" rowkey at "user" table on "cdh1" connector
GET "cdh1://user@kewang"

# MySQL example:
#
# Query one record at "user" table on "mysqltest" connector
GET "mysqltest://select * from user limit 1"
```

### SCAN, and so on.

TODO

## Related with Redis

This is a modified version of [Redis](https://github.com/antirez/redis), maintaining by [Kewang](https://github.com/kewangtw) now.
