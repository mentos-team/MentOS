#!/bin/bash
find -type l -exec git update-index --assume-unchanged {} \;
find -type l -exec bash -c 'ln -f "$(readlink -m "$0")" "$0"' {} \;