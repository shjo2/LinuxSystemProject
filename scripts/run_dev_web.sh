#!/bin/bash

cd toy-api && npm ci && \
cd ../toy-fe && npm ci && npm start

