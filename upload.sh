#!/bin/bash
now="$(date +"%r")"
msg=now+" commit"
git checkout master >> .local.git.out || echo
git add src/*.cc src/routes/*.cc include/*.hh Makefile >> .local.git.out  || echo
git commit -a -m "Commit at $now"  >> .local.git.out || echo
git add .local.git.out
git commit -am "Commit at $now"
git push origin master -f