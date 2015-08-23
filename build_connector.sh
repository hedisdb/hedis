#!/bin/sh

CONNECTOR_NAME=$1

mkdir -p connectors

curl -o connectors/connector-$CONNECTOR_NAME.tar.gz -L https://github.com/hedisdb/hedis-connector-$CONNECTOR_NAME/archive/master.tar.gz

tar zxvf connectors/connector-$CONNECTOR_NAME.tar.gz -C connectors

cd connectors/hedis-connector-$CONNECTOR_NAME-master

make pre_install
make
sudo make install

cd ../../
