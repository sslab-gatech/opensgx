#!/bin/bash

echo "Kill tor_processes.."
kill -9 $(ps -ef | grep 'tor --quiet' | awk '{print $2}')
echo "Done."
