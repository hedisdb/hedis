# Hedis

This is a modified version of [Redis](https://github.com/antirez/redis), maintaining by [Kewang](https://github.com/kewangtw) now.

## What is Hedis?

Hedis can store data from **ANY** database.

Traditionally, application server retrieves hot data from in-memory database to reduce unnecessary paths. If in-memory database doesn't have specific data, application server turns to retrieve data from original database.