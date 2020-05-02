#!/bin/bash
now="$(date +"%r")"
make
sleep 1
git add .
git commit -am "Commit at $now"
git push origin master -f